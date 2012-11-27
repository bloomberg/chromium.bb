// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYER_SORTER_H_
#define CC_LAYER_SORTER_H_

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "cc/cc_export.h"
#include "cc/layer_impl.h"
#include "ui/gfx/point3_f.h"
#include "ui/gfx/quad_f.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/vector3d_f.h"

#if defined(COMPILER_GCC)
namespace cc
{
    struct GraphEdge;
};

namespace BASE_HASH_NAMESPACE {
template<>
struct hash<cc::GraphEdge*> {
  size_t operator()(cc::GraphEdge* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
} // namespace BASE_HASH_NAMESPACE
#endif // COMPILER

namespace gfx {
class Transform;
}

namespace cc {

struct GraphEdge;

// Holds various useful properties derived from a layer's 3D outline.
struct CC_EXPORT LayerShape {
    LayerShape();
    LayerShape(float width, float height, const gfx::Transform& drawTransform);
    ~LayerShape();

    float layerZFromProjectedPoint(const gfx::PointF&) const;

    gfx::Vector3dF layerNormal;
    gfx::Point3F transformOrigin;
    gfx::QuadF projectedQuad;
    gfx::RectF projectedBounds;
};

struct GraphNode {
    explicit GraphNode(LayerImpl* layerImpl);
    ~GraphNode();

    LayerImpl* layer;
    LayerShape shape;
    std::vector<GraphEdge*> incoming;
    std::vector<GraphEdge*> outgoing;
    float incomingEdgeWeight;
};

struct GraphEdge {
    GraphEdge(GraphNode* fromNode, GraphNode* toNode, float weight)
        : from(fromNode)
        , to(toNode)
        , weight(weight)
    {
    }

    GraphNode* from;
    GraphNode* to;
    float weight;
};



class CC_EXPORT LayerSorter {
public:
    LayerSorter();
    ~LayerSorter();

    typedef std::vector<LayerImpl*> LayerList;

    void sort(LayerList::iterator first, LayerList::iterator last);

    enum ABCompareResult {
        ABeforeB,
        BBeforeA,
        None
    };

    static ABCompareResult checkOverlap(LayerShape*, LayerShape*, float zThreshold, float& weight);

private:
    typedef std::vector<GraphNode> NodeList;
    typedef std::vector<GraphEdge> EdgeList;
    NodeList m_nodes;
    EdgeList m_edges;
    float m_zRange;

    typedef base::hash_map<GraphEdge*, GraphEdge*> EdgeMap;
    EdgeMap m_activeEdges;

    void createGraphNodes(LayerList::iterator first, LayerList::iterator last);
    void createGraphEdges();
    void removeEdgeFromList(GraphEdge*, std::vector<GraphEdge*>&);

    DISALLOW_COPY_AND_ASSIGN(LayerSorter);
};

}
#endif  // CC_LAYER_SORTER_H_
