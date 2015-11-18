// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/bsp_tree.h"

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "cc/base/container_util.h"
#include "cc/output/bsp_compare_result.h"
#include "cc/quads/draw_polygon.h"

namespace cc {

BspNode::BspNode(scoped_ptr<DrawPolygon> data) : node_data(std::move(data)) {}

BspNode::~BspNode() {
}

BspTree::BspTree(std::deque<scoped_ptr<DrawPolygon>>* list) {
  if (list->size() == 0)
    return;

  root_ = make_scoped_ptr(new BspNode(PopFront(list)));
  BuildTree(root_.get(), list);
}

// The idea behind using a deque for BuildTree's input is that we want to be
// able to place polygons that we've decided aren't splitting plane candidates
// at the back of the queue while moving the candidate splitting planes to the
// front when the heuristic decides that they're a better choice. This way we
// can always simply just take from the front of the deque for our node's
// data.
void BspTree::BuildTree(BspNode* node,
                        std::deque<scoped_ptr<DrawPolygon>>* polygon_list) {
  std::deque<scoped_ptr<DrawPolygon>> front_list;
  std::deque<scoped_ptr<DrawPolygon>> back_list;

  // We take in a list of polygons at this level of the tree, and have to
  // find a splitting plane, then classify polygons as either in front of
  // or behind that splitting plane.
  while (!polygon_list->empty()) {
    // Is this particular polygon in front of or behind our splitting polygon.
    BspCompareResult comparer_result =
        GetNodePositionRelative(*polygon_list->front(), *(node->node_data));

    // If it's clearly behind or in front of the splitting plane, we use the
    // heuristic to decide whether or not we should put it at the back
    // or front of the list.
    switch (comparer_result) {
      case BSP_FRONT:
        front_list.push_back(PopFront(polygon_list));
        break;
      case BSP_BACK:
        back_list.push_back(PopFront(polygon_list));
        break;
      case BSP_SPLIT:
      {
        scoped_ptr<DrawPolygon> polygon;
        scoped_ptr<DrawPolygon> new_front;
        scoped_ptr<DrawPolygon> new_back;
        // Time to split this geometry, *it needs to be split by node_data.
        polygon = PopFront(polygon_list);
        bool split_result =
            polygon->Split(*(node->node_data), &new_front, &new_back);
        DCHECK(split_result);
        if (!split_result) {
          break;
        }
        front_list.push_back(std::move(new_front));
        back_list.push_back(std::move(new_back));
        break;
      }
      case BSP_COPLANAR_FRONT:
        node->coplanars_front.push_back(PopFront(polygon_list));
        break;
      case BSP_COPLANAR_BACK:
        node->coplanars_back.push_back(PopFront(polygon_list));
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  // Build the back subtree using the front of the back_list as our splitter.
  if (back_list.size() > 0) {
    node->back_child = make_scoped_ptr(new BspNode(PopFront(&back_list)));
    BuildTree(node->back_child.get(), &back_list);
  }

  // Build the front subtree using the front of the front_list as our splitter.
  if (front_list.size() > 0) {
    node->front_child = make_scoped_ptr(new BspNode(PopFront(&front_list)));
    BuildTree(node->front_child.get(), &front_list);
  }
}

BspCompareResult BspTree::GetNodePositionRelative(const DrawPolygon& node_a,
                                                  const DrawPolygon& node_b) {
  return DrawPolygon::SideCompare(node_a, node_b);
}

// The base comparer with 0,0,0 as camera position facing forward
BspCompareResult BspTree::GetCameraPositionRelative(const DrawPolygon& node) {
  if (node.normal().z() > 0.0f) {
    return BSP_FRONT;
  }
  return BSP_BACK;
}

BspTree::~BspTree() {
}

}  // namespace cc
