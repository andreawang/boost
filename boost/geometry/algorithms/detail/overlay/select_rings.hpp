// Boost.Geometry (aka GGL, Generic Geometry Library)
//
// Copyright Barend Gehrels 2011, Amsterdam, the Netherlands.
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_SELECT_RINGS_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_SELECT_RINGS_HPP

#include <map>


#include <boost/geometry/algorithms/area.hpp>
#include <boost/geometry/algorithms/within.hpp>
#include <boost/geometry/algorithms/detail/point_on_border.hpp>
#include <boost/geometry/algorithms/detail/ring_identifier.hpp>
#include <boost/geometry/algorithms/detail/overlay/ring_properties.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>


namespace boost { namespace geometry
{


#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{


namespace dispatch
{

    template <typename Tag, typename Geometry>
    struct select_rings
    {};

    template <typename Box>
    struct select_rings<box_tag, Box>
    {
        template <typename Geometry, typename Map>
        static inline void apply(Box const& box, Geometry const& geometry, ring_identifier const& id, Map& map)
        {
            map[id] = typename Map::mapped_type(box, geometry);
        }

        template <typename Map>
        static inline void apply(Box const& box, ring_identifier const& id, Map& map)
        {
            map[id] = typename Map::mapped_type(box);
        }
    };

    template <typename Ring>
    struct select_rings<ring_tag, Ring>
    {
        template <typename Geometry, typename Map>
        static inline void apply(Ring const& ring, Geometry const& geometry, ring_identifier const& id, Map& map)
        {
            if (boost::size(ring) > 0)
            {
                map[id] = typename Map::mapped_type(ring, geometry);
            }
        }

        template <typename Map>
        static inline void apply(Ring const& ring, ring_identifier const& id, Map& map)
        {
            if (boost::size(ring) > 0)
            {
                map[id] = typename Map::mapped_type(ring);
            }
        }
    };


    template <typename Polygon>
    struct select_rings<polygon_tag, Polygon>
    {
        template <typename Geometry, typename Map>
        static inline void apply(Polygon const& polygon, Geometry const& geometry, ring_identifier id, Map& map)
        {
            typedef typename geometry::ring_type<Polygon>::type ring_type;
            typedef select_rings<ring_tag, ring_type> per_ring;

            per_ring::apply(exterior_ring(polygon), geometry, id, map);

            typename interior_return_type<Polygon const>::type rings
                        = interior_rings(polygon);
            for (BOOST_AUTO_TPL(it, boost::begin(rings)); it != boost::end(rings); ++it)
            {
                id.ring_index++;
                per_ring::apply(*it, geometry, id, map);
            }
        }

        template <typename Map>
        static inline void apply(Polygon const& polygon, ring_identifier id, Map& map)
        {
            typedef typename geometry::ring_type<Polygon>::type ring_type;
            typedef select_rings<ring_tag, ring_type> per_ring;

            per_ring::apply(exterior_ring(polygon), id, map);

            typename interior_return_type<Polygon const>::type rings
                        = interior_rings(polygon);
            for (BOOST_AUTO_TPL(it, boost::begin(rings)); it != boost::end(rings); ++it)
            {
                id.ring_index++;
                per_ring::apply(*it, id, map);
            }
        }
    };
}


template<overlay_type OverlayType>
struct decide
{};

template<>
struct decide<overlay_union>
{
    template <typename Code>
    static bool include(ring_identifier const& id, Code const& code)
    {
        return code.within_code * -1 == 1;
    }

    template <typename Code>
    static bool reversed(ring_identifier const& , Code const& )
    {
        return false;
    }
};

template<>
struct decide<overlay_difference>
{
    template <typename Code>
    static bool include(ring_identifier const& id, Code const& code)
    {
        bool is_first = id.source_index == 0;
        return code.within_code * -1 * (is_first ? 1 : -1) == 1;
    }

    template <typename Code>
    static bool reversed(ring_identifier const& id, Code const& code)
    {
        return include(id, code) && id.source_index == 1;
    }
};

template<>
struct decide<overlay_intersection>
{
    template <typename Code>
    static bool include(ring_identifier const& id, Code const& code)
    {
        return code.within_code * 1 == 1;
    }

    template <typename Code>
    static bool reversed(ring_identifier const& , Code const& )
    {
        return false;
    }
};


template
<
    overlay_type OverlayType,
    typename IntersectionMap, typename SelectionMap
>
inline void update_selection_map(IntersectionMap const& intersection_map,
            SelectionMap const& map_with_all, SelectionMap& selection_map)
{
    selection_map.clear();

    for (typename SelectionMap::const_iterator it = boost::begin(map_with_all);
        it != boost::end(map_with_all);
        ++it)
    {
        /*
        int union_code = it->second.within_code * -1;
        bool is_first = it->first.source_index == 0;
        std::cout << it->first << " " << it->second.area
            << ": " << it->second.within_code
            << " union: " << union_code
            << " intersection: " << (it->second.within_code * 1)
            << " G1-G2: " << (union_code * (is_first ? 1 : -1))
            << " G2-G1: " << (union_code * (is_first ? -1 : 1))
            << " -> " << (decide<OverlayType>::include(it->first, it->second) ? "INC" : "")
            << decide<OverlayType>::reverse(it->first, it->second)
            << std::endl;
        */

        bool found = intersection_map.find(it->first) != intersection_map.end();
        if (! found)
        {
            if (decide<OverlayType>::include(it->first, it->second))
            {
                selection_map[it->first] = it->second;
                selection_map[it->first].reversed = decide<OverlayType>::reversed(it->first, it->second);
            }
        }
    }
}


/*!
\brief The function select_rings select rings based on the overlay-type (union,intersection)
*/
template
<
    overlay_type OverlayType,
    typename Geometry1, typename Geometry2,
    typename IntersectionMap, typename SelectionMap
>
inline void select_rings(Geometry1 const& geometry1, Geometry2 const& geometry2,
            IntersectionMap const& intersection_map, SelectionMap& selection_map)
{
    typedef typename geometry::tag<Geometry1>::type tag1;
    typedef typename geometry::tag<Geometry2>::type tag2;

    SelectionMap map_with_all;
    dispatch::select_rings<tag1, Geometry1>::apply(geometry1, geometry2, ring_identifier(0, -1, -1), map_with_all);
    dispatch::select_rings<tag2, Geometry2>::apply(geometry2, geometry1, ring_identifier(1, -1, -1), map_with_all);

    update_selection_map<OverlayType>(intersection_map, map_with_all, selection_map);
}

template
<
    overlay_type OverlayType,
    typename Geometry,
    typename IntersectionMap, typename SelectionMap
>
inline void select_rings(Geometry const& geometry,
            IntersectionMap const& intersection_map, SelectionMap& selection_map)
{
    typedef typename geometry::tag<Geometry>::type tag;

    SelectionMap map_with_all;
    dispatch::select_rings<tag, Geometry>::apply(geometry, ring_identifier(0, -1, -1), map_with_all);

    update_selection_map<OverlayType>(intersection_map, map_with_all, selection_map);
}


}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL


}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_SELECT_RINGS_HPP
