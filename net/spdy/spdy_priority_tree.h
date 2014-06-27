// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_PRIORITY_TREE_H_
#define NET_SPDY_SPDY_PRIORITY_TREE_H_

#include <cmath>
#include <list>
#include <map>
#include <queue>
#include <set>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"

namespace net {

// This data structure implements the HTTP2 prioritization data structure
// defined in draft standard:
// http://tools.ietf.org/html/draft-ietf-httpbis-http2-13
//
// Nodes can be added and removed, and dependencies between them defined.  Each
// node can have at most one parent and at most one child (forming a list), but
// there can be multiple lists, with each list root having its own priority.
// Individual nodes can also be marked as ready to read/write, and then the
// whole structure can be queried to pick the next node to read/write out of
// those ready.
//
// The NodeId type must be a POD that supports comparison (most
// likely, it will be a number).

namespace test {
template <typename NodeId>
class SpdyPriorityTreePeer;
}

const int kRootNodeId = 0;
const int kDefaultWeight = 16;
const int kMinWeight = 1;
const int kMaxWeight = 256;

template <typename NodeId>
class SpdyPriorityTree {
  typedef std::vector<std::pair<NodeId, float> > PriorityNodeList;

 public:
  SpdyPriorityTree();
  ~SpdyPriorityTree();

  typedef std::list<NodeId> List;
  struct Node {
    Node();
    ~Node();

    NodeId id;
    NodeId parent_id;
    int weight;  // Weights can range between 1 and 256 (inclusive).
    // The total weight of this node's direct descendants.
    int total_child_weights;
    // The total weight of direct descendants that are writeable
    // (ready to write and not blocked). This value does not necessarily
    // reflect the current state of the tree; instead, we lazily update it
    // on calls to PropagateNodeState(node.id).
    int total_writeable_child_weights;
    List* child_list;  // node ID's of children, if any
    bool blocked;  // Is the associated stream write-blocked?
    bool ready;  // Does the stream have data ready for writing?
    float priority;  // The fraction of resources to dedicate to this node.
  };

  // Orders in descending order of priority.
  struct NodePriorityComparator {
    bool operator ()(const std::pair<NodeId, float>& lhs,
                     const std::pair<NodeId, float>& rhs);
  };

  friend class test::SpdyPriorityTreePeer<NodeId>;

  // Return the number of nodes currently in the tree.
  int num_nodes() const;

  // Return true if the tree contains a node with the given ID.
  bool NodeExists(NodeId node_id) const;

  // Add a new node with the given weight and parent. Non-exclusive nodes
  // simply get added below the parent node. If exclusive = true, the node
  // becomes the parent's sole child and the parent's previous children
  // become the children of the new node.
  // Returns true on success. Returns false if the node already exists
  // in the tree, or if the parent node does not exist.
  bool AddNode(NodeId node_id, NodeId parent_id, int weight, bool exclusive);

  // Remove an existing node from the tree.  Returns true on success, or
  // false if the node doesn't exist.
  bool RemoveNode(NodeId node_id);

  // Get the weight of the given node.
  int GetWeight(NodeId node_id) const;

  // Get the parent of the given node.  If the node doesn't exist, or is a root
  // node (and thus has no parent), returns NodeId().
  NodeId GetParent(NodeId node_id) const;

  // Get the child list of the given node.  If the node doesn't exist, or has no
  // child, returns NULL.
  std::list<NodeId>* GetChildren(NodeId node_id) const;

  // Set the priority of the given node.
  bool SetWeight(NodeId node_id, int weight);

  // Set the parent of the given node.  Returns true on success.
  // Returns false and has no effect if the node and/or the parent doesn't
  // exist. If the new parent is a descendant of the node (i.e. this would have
  // created a cycle) then we rearrange the topology of the tree as described
  // in the HTTP2 spec.
  bool SetParent(NodeId node_id, NodeId parent_id, bool exclusive);

  // Returns true if the node parent_id has child_id in its child_list.
  bool HasChild(NodeId parent_id, NodeId child_id) const;

  // Mark a node as blocked or unblocked. Return true on success, or false
  // if unable to mark the specified node.
  bool SetBlocked(NodeId node_id, bool blocked);

  // Mark whether or not a node is ready to write; i.e. whether there is
  // buffered data for the associated stream. Return true on success, or false
  // if unable to mark the specified node.
  bool SetReady(NodeId node_id, bool ready);

  // Return true if all internal invariants hold (useful for unit tests).
  // Unless there are bugs, this should always return true.
  bool ValidateInvariantsForTests() const;

  // Get the given node, or return NULL if it doesn't exist.
  const Node* FindNode(NodeId node_id) const;

  // Returns an ordered list of writeable nodes and their priorities.
  // Priority is calculated as:
  // parent's priority * (node's weight / sum of sibling weights)
  PriorityNodeList GetPriorityList();

 protected:
  // Update the value of total_writeable_child_weights for the given node
  // to reflect the current state of the tree.
  void PropagateNodeState(NodeId node);

 private:
  typedef base::hash_map<NodeId, Node> NodeMap;

  NodeMap all_nodes_;  // maps from node IDs to Node objects

  DISALLOW_COPY_AND_ASSIGN(SpdyPriorityTree);
};

template <typename NodeId>
SpdyPriorityTree<NodeId>::SpdyPriorityTree() {
  Node* root_node = &all_nodes_[kRootNodeId];
  root_node->id = kRootNodeId;
  root_node->weight = kDefaultWeight;
  root_node->parent_id = static_cast<NodeId>(kRootNodeId);
  root_node->child_list = new std::list<NodeId>;
  root_node->priority = 1.0;
  root_node->ready = true;
}

template <typename NodeId>
SpdyPriorityTree<NodeId>::~SpdyPriorityTree() {}

template <typename NodeId>
SpdyPriorityTree<NodeId>::Node::Node() :
  parent_id(kRootNodeId),
  weight(kDefaultWeight),
  total_child_weights(0),
  total_writeable_child_weights(0),
  child_list(),
  blocked(false),
  ready(false),
  priority(0) {
}

template <typename NodeId>
SpdyPriorityTree<NodeId>::Node::~Node() {
  delete child_list;
}

template <typename NodeId>
bool SpdyPriorityTree<NodeId>::NodePriorityComparator::operator ()(
    const std::pair<NodeId, float>& lhs,
    const std::pair<NodeId, float>& rhs) {
  return lhs.second > rhs.second;
}

template <typename NodeId>
int SpdyPriorityTree<NodeId>::num_nodes() const {
  return all_nodes_.size();
}

template <typename NodeId>
bool SpdyPriorityTree<NodeId>::NodeExists(NodeId node_id) const {
  return all_nodes_.count(node_id) != 0;
}

template <typename NodeId>
bool SpdyPriorityTree<NodeId>::AddNode(NodeId node_id,
                                       NodeId parent_id,
                                       int weight,
                                       bool exclusive) {
  if (NodeExists(node_id) || !NodeExists(parent_id)) {
    return false;
  }
  if (weight < kMinWeight || weight > kMaxWeight) {
    return false;
  }
  Node* parent = &all_nodes_[parent_id];
  Node* new_node = &all_nodes_[node_id];
  new_node->id = node_id;
  new_node->weight = weight;
  new_node->parent_id = parent_id;
  if (exclusive) {
    // Move the parent's current children below the new node.
    new_node->child_list = parent->child_list;
    new_node->total_child_weights = parent->total_child_weights;
    // Update each child's parent_id.
    for (typename List::iterator it = new_node->child_list->begin();
         it != new_node->child_list->end(); ++it) {
      Node* child = &all_nodes_[*it];
      child->parent_id = node_id;
    }
    // Clear parent's old child data.
    parent->child_list = new std::list<NodeId>;
    parent->total_child_weights = 0;
  } else {
    new_node->child_list = new std::list<NodeId>;
  }
  // Add new node to parent.
  parent->child_list->push_back(node_id);
  parent->total_child_weights += weight;
  return true;
}

template <typename NodeId>
bool SpdyPriorityTree<NodeId>::RemoveNode(NodeId node_id) {
  if (node_id == static_cast<NodeId>(kRootNodeId) || !NodeExists(node_id)) {
    return false;
  }
  const Node& node = all_nodes_[node_id];

  DCHECK(NodeExists(node.parent_id));
  Node* parent = &all_nodes_[node.parent_id];
  // Remove the node id from parent's child list.
  parent->child_list->remove(node_id);
  parent->total_child_weights -= node.weight;

  // Move the node's children to the parent's child list.
  if (node.child_list != NULL) {
    // Update each child's parent_id and weight.
    for (typename List::iterator it = node.child_list->begin();
         it != node.child_list->end(); ++it) {
      Node* child = &all_nodes_[*it];
      child->parent_id = node.parent_id;
      // Divide the removed node's weight among its children, rounding to the
      // nearest valid weight.
      float float_weight =  node.weight * static_cast<float>(child->weight) /
                            static_cast<float>(node.total_child_weights);
      int new_weight = std::floor(float_weight + 0.5);
      if (new_weight == 0) {
        new_weight = 1;
      }
      child->weight = new_weight;
      parent->total_child_weights += child->weight;
    }
    parent->child_list->splice(parent->child_list->end(), *node.child_list);
  }

  // Delete the node.
  all_nodes_.erase(node_id);
  return true;
}

template <typename NodeId>
int SpdyPriorityTree<NodeId>::GetWeight(NodeId node_id) const {
  const Node* node = FindNode(node_id);
  if (node != NULL) {
    return node->weight;
  }
  return 0;
}

template <typename NodeId>
NodeId SpdyPriorityTree<NodeId>::GetParent(NodeId node_id) const {
  const Node* node = FindNode(node_id);
  if (node != NULL && node->id != static_cast<NodeId>(kRootNodeId)) {
    return node->parent_id;
  }
  return static_cast<NodeId>(kRootNodeId);
}

template <typename NodeId>
std::list<NodeId>* SpdyPriorityTree<NodeId>::GetChildren(NodeId node_id) const {
  const Node* node = FindNode(node_id);
  if (node != NULL) {
    return node->child_list;
  }
  return NULL;
}

template <typename NodeId>
bool SpdyPriorityTree<NodeId>::SetWeight(
    NodeId node_id, int weight) {
  if (!NodeExists(node_id)) {
    return false;
  }
  if (weight < kMinWeight || weight > kMaxWeight) {
    return false;
  }

  Node* node = &all_nodes_[node_id];
  Node* parent = &all_nodes_[node->parent_id];

  parent->total_child_weights += (weight - node->weight);
  node->weight = weight;

  return true;
}


template <typename NodeId>
bool SpdyPriorityTree<NodeId>::SetParent(
    NodeId node_id, NodeId parent_id, bool exclusive) {
  if (!NodeExists(node_id) || !NodeExists(parent_id)) {
    return false;
  }
  if (node_id == parent_id) return false;

  Node* node = &all_nodes_[node_id];
  Node* new_parent = &all_nodes_[parent_id];
  // If the new parent is already the node's parent, we're done.
  if (node->parent_id == parent_id) {
    return true;
  }

  // Next, check to see if the new parent is currently a descendant
  // of the node.
  Node* last = new_parent;
  NodeId last_id = parent_id;
  bool cycle_exists = false;
  while (last->parent_id != static_cast<NodeId>(kRootNodeId)) {
    if (last->parent_id == node_id) {
      cycle_exists = true;
      break;
    }
    last_id = last->parent_id;
    DCHECK(NodeExists(last_id));
    last = &all_nodes_[last_id];
  }

  if (cycle_exists) {
    // The new parent moves to the level of the current node.
    SetParent(parent_id, node->parent_id, false);
  }

  // Remove node from old parent's child list.
  const NodeId old_parent_id = node->parent_id;
  DCHECK(NodeExists(old_parent_id));
  Node* old_parent = &all_nodes_[old_parent_id];
  old_parent->child_list->remove(node_id);
  old_parent->total_child_weights -= node->weight;

  // Make the change.
  node->parent_id = parent_id;
  new_parent->child_list->push_back(node_id);
  new_parent->total_child_weights += node->weight;
  return true;
}

template <typename NodeId>
bool SpdyPriorityTree<NodeId>::SetBlocked(NodeId node_id, bool blocked) {
  if (!NodeExists(node_id)) {
    return false;
  }

  Node* node = &all_nodes_[node_id];
  node->blocked = blocked;
  return true;
}

template <typename NodeId>
bool SpdyPriorityTree<NodeId>::SetReady(NodeId node_id, bool ready) {
  if (!NodeExists(node_id)) {
    return false;
  }
  Node* node = &all_nodes_[node_id];
  node->ready = ready;
  return true;
}

template <typename NodeId>
void SpdyPriorityTree<NodeId>::PropagateNodeState(NodeId node_id) {
  // Reset total_writeable_child_weights to its maximum value.
  Node* node = &all_nodes_[node_id];
  node->total_writeable_child_weights = node->total_child_weights;
  for (typename List::iterator it = node->child_list->begin();
       it != node->child_list->end(); ++it) {
    PropagateNodeState(*it);
  }
  if (node->total_writeable_child_weights == 0 &&
      (node->blocked || !node->ready)) {
    // Tell the parent that this entire subtree is unwriteable.
    Node* parent = &all_nodes_[node->parent_id];
    parent->total_writeable_child_weights -= node->weight;
  }
}

template <typename NodeId>
const typename SpdyPriorityTree<NodeId>::Node*
SpdyPriorityTree<NodeId>::FindNode(NodeId node_id) const {
  typename NodeMap::const_iterator iter = all_nodes_.find(node_id);
  if (iter == all_nodes_.end()) {
    return NULL;
  }
  return &iter->second;
}

template <typename NodeId>
bool SpdyPriorityTree<NodeId>::HasChild(NodeId parent_id,
                                        NodeId child_id) const {
  const Node* parent = FindNode(parent_id);
  return parent->child_list->end() !=
         std::find(parent->child_list->begin(),
                   parent->child_list->end(),
                   child_id);
}

template <typename NodeId>
std::vector<std::pair<NodeId, float> >
SpdyPriorityTree<NodeId>::GetPriorityList() {
  typedef std::pair<NodeId, float> PriorityNode;
  typedef std::vector<PriorityNode> PriorityList;
  PriorityList priority_list;

  // Update total_writeable_child_weights to reflect the current
  // state of the tree.
  PropagateNodeState(kRootNodeId);

  List queue;
  const Node* root_node = FindNode(kRootNodeId);
  DCHECK(root_node->priority == 1.0);
  // Start by examining our top-level nodes.
  for (typename List::iterator it = root_node->child_list->begin();
       it != root_node->child_list->end(); ++it) {
    queue.push_back(*it);
  }
  while (!queue.empty()) {
    NodeId current_node_id = queue.front();
    Node* current_node = &all_nodes_[current_node_id];
    const Node* parent_node = FindNode(current_node->parent_id);
    if (current_node->blocked || !current_node->ready) {
      if (current_node->total_writeable_child_weights > 0) {
        // This node isn't writeable, but it has writeable children.
        // Calculate the total fraction of resources we can allot
        // to this subtree.
        current_node->priority = parent_node->priority *
            (static_cast<float>(current_node->weight) /
             static_cast<float>(parent_node->total_writeable_child_weights));
        // Examine the children.
        for (typename List::iterator it = current_node->child_list->begin();
             it != current_node->child_list->end(); ++it) {
          queue.push_back(*it);
        }
      } else {
        // There's nothing to see in this subtree.
        current_node->priority = 0;
      }
    } else {
      // This node is writeable; calculate its priority.
      current_node->priority = parent_node->priority *
          (static_cast<float>(current_node->weight) /
           static_cast<float>(parent_node->total_writeable_child_weights));
      // Add this node to the priority list.
      priority_list.push_back(PriorityNode(current_node_id,
                                           current_node->priority));
    }
    // Remove this node from the queue.
    queue.pop_front();
  }

  // Sort the nodes in descending order of priority.
  std::sort(priority_list.begin(), priority_list.end(),
            NodePriorityComparator());

  return priority_list;
}

template <typename NodeId>
bool SpdyPriorityTree<NodeId>::ValidateInvariantsForTests() const {
  int total_nodes = 0;
  int nodes_visited = 0;
  // Iterate through all nodes in the map.
  for (typename NodeMap::const_iterator iter = all_nodes_.begin();
       iter != all_nodes_.end(); ++iter) {
    ++total_nodes;
    ++nodes_visited;
    const Node& node = iter->second;
    // All nodes except the root should have a parent, and should appear in
    // the child_list of that parent.
    if (node.id != static_cast<NodeId>(kRootNodeId) &&
        (!NodeExists(node.parent_id) ||
         !HasChild(node.parent_id, node.id))) {
      DLOG(INFO) << "Parent node " << node.parent_id
                 << " does not exist, or does not list node " << node.id
                 << " as its child.";
      return false;
    }

    if (!node.child_list->empty()) {
      int total_child_weights = 0;
      // Iterate through the node's children.
      for (typename List::iterator it = node.child_list->begin();
           it != node.child_list->end(); ++it) {
        ++nodes_visited;
        // Each node in the list should exist and should have this node
        // set as its parent.
        if (!NodeExists(*it) || node.id != GetParent(*it)) {
          DLOG(INFO) << "Child node " << *it << " does not exist, "
                     << "or does not list " << node.id << " as its parent.";
          return false;
        }
        const Node* child = FindNode(*it);
        total_child_weights += child->weight;
      }
      // Verify that total_child_weights is correct.
      if (total_child_weights != node.total_child_weights) {
        DLOG(INFO) << "Child weight totals do not agree. For node " << node.id
                   << " total_child_weights has value "
                   << node.total_child_weights
                   << ", expected " << total_child_weights;
        return false;
      }
    }
  }

  // Make sure num_nodes reflects the total number of nodes the map contains.
  if (total_nodes != num_nodes()) {
    DLOG(INFO) << "Map contains incorrect number of nodes.";
    return false;
  }
  // Validate the validation function; we should have visited each node twice
  // (except for the root)
  DCHECK(nodes_visited == 2*num_nodes() - 1);
  return true;
}

}  // namespace net

#endif  // NET_SPDY_SPDY_PRIORITY_TREE_H_
