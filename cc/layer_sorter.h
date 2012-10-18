// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCLayerSorter_h
#define CCLayerSorter_h

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "CCLayerImpl.h"
#include "FloatPoint3D.h"
#include "FloatQuad.h"
#include "FloatRect.h"

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

namespace WebKit {
class WebTransformationMatrix;
}

namespace cc {

struct GraphEdge;

// Holds various useful properties derived from a layer's 3D outline.
struct LayerShape {
    LayerShape();
    LayerShape(float width, float height, const WebKit::WebTransformationMatrix& drawTransform);

    float layerZFromProjectedPoint(const FloatPoint&) const;

    FloatPoint3D layerNormal;
    FloatPoint3D transformOrigin;
    FloatQuad projectedQuad;
    FloatRect projectedBounds;
};

struct GraphNode {
    explicit GraphNode(CCLayerImpl* cclayer);
    ~GraphNode();

    CCLayerImpl* layer;
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



class CCLayerSorter {
public:
    CCLayerSorter();
    ~CCLayerSorter();

    typedef std::vector<CCLayerImpl*> LayerList;

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

    DISALLOW_COPY_AND_ASSIGN(CCLayerSorter);
};

}
#endif
