// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/test_change_tracker.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/view_manager/util.h"

namespace mojo {
namespace service {

std::string NodeIdToString(Id id) {
  return (id == 0) ? "null" :
      base::StringPrintf("%d,%d", HiWord(id), LoWord(id));
}

namespace {

std::string RectToString(const gfx::Rect& rect) {
  return base::StringPrintf("%d,%d %dx%d", rect.x(), rect.y(), rect.width(),
                            rect.height());
}

std::string DirectionToString(OrderDirection direction) {
  return direction == ORDER_DIRECTION_ABOVE ? "above" : "below";
}

std::string ChangeToDescription1(const Change& change) {
  switch (change.type) {
    case CHANGE_TYPE_EMBED:
      return base::StringPrintf("OnEmbed creator=%s",
                                change.creator_url.data());

    case CHANGE_TYPE_NODE_BOUNDS_CHANGED:
      return base::StringPrintf(
          "BoundsChanged node=%s old_bounds=%s new_bounds=%s",
          NodeIdToString(change.node_id).c_str(),
          RectToString(change.bounds).c_str(),
          RectToString(change.bounds2).c_str());

    case CHANGE_TYPE_NODE_HIERARCHY_CHANGED:
      return base::StringPrintf(
            "HierarchyChanged node=%s new_parent=%s old_parent=%s",
            NodeIdToString(change.node_id).c_str(),
            NodeIdToString(change.node_id2).c_str(),
            NodeIdToString(change.node_id3).c_str());

    case CHANGE_TYPE_NODE_REORDERED:
      return base::StringPrintf(
          "Reordered node=%s relative=%s direction=%s",
          NodeIdToString(change.node_id).c_str(),
          NodeIdToString(change.node_id2).c_str(),
          DirectionToString(change.direction).c_str());

    case CHANGE_TYPE_NODE_DELETED:
      return base::StringPrintf("NodeDeleted node=%s",
                                NodeIdToString(change.node_id).c_str());

    case CHANGE_TYPE_INPUT_EVENT:
      return base::StringPrintf(
          "InputEvent node=%s event_action=%d",
          NodeIdToString(change.node_id).c_str(),
          change.event_action);
    case CHANGE_TYPE_DELEGATE_EMBED:
      return base::StringPrintf("DelegateEmbed url=%s",
                                change.embed_url.data());
  }
  return std::string();
}

}  // namespace

std::vector<std::string> ChangesToDescription1(
    const std::vector<Change>& changes) {
  std::vector<std::string> strings(changes.size());
  for (size_t i = 0; i < changes.size(); ++i)
    strings[i] = ChangeToDescription1(changes[i]);
  return strings;
}

std::string ChangeNodeDescription(const std::vector<Change>& changes) {
  if (changes.size() != 1)
    return std::string();
  std::vector<std::string> node_strings(changes[0].nodes.size());
  for (size_t i = 0; i < changes[0].nodes.size(); ++i)
    node_strings[i] = "[" + changes[0].nodes[i].ToString() + "]";
  return JoinString(node_strings, ',');
}

TestNode NodeDataToTestNode(const NodeDataPtr& data) {
  TestNode node;
  node.parent_id = data->parent_id;
  node.node_id = data->node_id;
  return node;
}

void NodeDatasToTestNodes(const Array<NodeDataPtr>& data,
                          std::vector<TestNode>* test_nodes) {
  for (size_t i = 0; i < data.size(); ++i)
    test_nodes->push_back(NodeDataToTestNode(data[i]));
}

Change::Change()
    : type(CHANGE_TYPE_EMBED),
      connection_id(0),
      node_id(0),
      node_id2(0),
      node_id3(0),
      event_action(0),
      direction(ORDER_DIRECTION_ABOVE) {
}

Change::~Change() {
}

TestChangeTracker::TestChangeTracker()
    : delegate_(NULL) {
}

TestChangeTracker::~TestChangeTracker() {
}

void TestChangeTracker::OnEmbed(ConnectionSpecificId connection_id,
                                const String& creator_url,
                                NodeDataPtr root) {
  Change change;
  change.type = CHANGE_TYPE_EMBED;
  change.connection_id = connection_id;
  change.creator_url = creator_url;
  change.nodes.push_back(NodeDataToTestNode(root));
  AddChange(change);
}

void TestChangeTracker::OnNodeBoundsChanged(Id node_id,
                                            RectPtr old_bounds,
                                            RectPtr new_bounds) {
  Change change;
  change.type = CHANGE_TYPE_NODE_BOUNDS_CHANGED;
  change.node_id = node_id;
  change.bounds = old_bounds.To<gfx::Rect>();
  change.bounds2 = new_bounds.To<gfx::Rect>();
  AddChange(change);
}

void TestChangeTracker::OnNodeHierarchyChanged(Id node_id,
                                               Id new_parent_id,
                                               Id old_parent_id,
                                               Array<NodeDataPtr> nodes) {
  Change change;
  change.type = CHANGE_TYPE_NODE_HIERARCHY_CHANGED;
  change.node_id = node_id;
  change.node_id2 = new_parent_id;
  change.node_id3 = old_parent_id;
  NodeDatasToTestNodes(nodes, &change.nodes);
  AddChange(change);
}

void TestChangeTracker::OnNodeReordered(Id node_id,
                                        Id relative_node_id,
                                        OrderDirection direction) {
  Change change;
  change.type = CHANGE_TYPE_NODE_REORDERED;
  change.node_id = node_id;
  change.node_id2 = relative_node_id;
  change.direction = direction;
  AddChange(change);
}

void TestChangeTracker::OnNodeDeleted(Id node_id) {
  Change change;
  change.type = CHANGE_TYPE_NODE_DELETED;
  change.node_id = node_id;
  AddChange(change);
}

void TestChangeTracker::OnNodeInputEvent(Id node_id, EventPtr event) {
  Change change;
  change.type = CHANGE_TYPE_INPUT_EVENT;
  change.node_id = node_id;
  change.event_action = event->action;
  AddChange(change);
}

void TestChangeTracker::DelegateEmbed(const String& url) {
  Change change;
  change.type = CHANGE_TYPE_DELEGATE_EMBED;
  change.embed_url = url;
  AddChange(change);
}

void TestChangeTracker::AddChange(const Change& change) {
  changes_.push_back(change);
  if (delegate_)
    delegate_->OnChangeAdded();
}

std::string TestNode::ToString() const {
  return base::StringPrintf("node=%s parent=%s",
                            NodeIdToString(node_id).c_str(),
                            NodeIdToString(parent_id).c_str());
}

}  // namespace service
}  // namespace mojo
