// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_PRIORITY_TREE_H_
#define NET_SPDY_SPDY_PRIORITY_TREE_H_

#include <cmath>
#include <deque>
#include <map>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"

namespace net {

// This data structure implements the HTTP/2 stream priority tree defined in
// section 5.3 of RFC 7540:
// http://tools.ietf.org/html/rfc7540#section-5.3
//
// Nodes can be added and removed, and dependencies between them defined.
// Nodes constitute a tree rooted at node ID 0: each node has a single parent
// node, and 0 or more child nodes.  Individual nodes can be marked as ready to
// read/write, and then the whole structure can be queried to pick the next
// node to read/write out of those that are ready.
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
 public:
  typedef std::pair<NodeId, float> PriorityNode;
  typedef std::vector<PriorityNode> PriorityList;

  SpdyPriorityTree();

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

  // Get the children of the given node.  If the node doesn't exist, or has no
  // child, returns empty vector.
  std::vector<NodeId> GetChildren(NodeId node_id) const;

  // Set the priority of the given node.
  bool SetWeight(NodeId node_id, int weight);

  // Set the parent of the given node.  Returns true on success.
  // Returns false and has no effect if the node and/or the parent doesn't
  // exist. If the new parent is a descendant of the node (i.e. this would have
  // created a cycle) then we rearrange the topology of the tree as described
  // in section 5.3.3 of RFC 7540:
  // https://tools.ietf.org/html/rfc7540#section-5.3.3
  bool SetParent(NodeId node_id, NodeId parent_id, bool exclusive);

  // Returns true if the node parent_id has child_id in its children.
  bool HasChild(NodeId parent_id, NodeId child_id) const;

  // Mark a node as blocked or unblocked. Return true on success, or false
  // if unable to mark the specified node.
  bool SetBlocked(NodeId node_id, bool blocked);

  // Mark whether or not a node is ready to write; i.e. whether there is
  // buffered data for the associated stream. Return true on success, or false
  // if unable to mark the specified node.
  bool SetReady(NodeId node_id, bool ready);

  // Returns an ordered list of writeable nodes and their priorities.
  // Priority is calculated as:
  // parent's priority * (node's weight / sum of sibling weights)
  PriorityList GetPriorityList();

 private:
  struct Node;
  typedef std::vector<Node*> NodeVector;
  typedef std::map<NodeId, Node*> NodeMap;

  struct Node {
    // ID for this node.
    NodeId id;
    // ID of parent node.
    Node* parent = nullptr;
    // Weights can range between 1 and 256 (inclusive).
    int weight = kDefaultWeight;
    // The total weight of this node's direct descendants.
    int total_child_weights = 0;
    // The total weight of direct descendants that are writeable
    // (ready to write and not blocked). This value does not necessarily
    // reflect the current state of the tree; instead, we lazily update it
    // on calls to PropagateNodeState().
    int total_writeable_child_weights = 0;
    // Pointers to nodes for children, if any.
    NodeVector children;
    // Is the associated stream write-blocked?
    bool blocked = false;
    // Does the stream have data ready for writing?
    bool ready = false;
    // The fraction of resources to dedicate to this node.
    float priority = 0;
  };

  static bool Remove(NodeVector* nodes, const Node* node);

  // Update the value of total_writeable_child_weights for the given node
  // to reflect the current state of the tree.
  void PropagateNodeState(Node* node);

  // Get the given node, or return nullptr if it doesn't exist.
  const Node* FindNode(NodeId node_id) const;
  Node* FindNode(NodeId node_id);

  // Return true if all internal invariants hold (useful for unit tests).
  // Unless there are bugs, this should always return true.
  bool ValidateInvariantsForTests() const;

  Node* root_node_;    // pointee owned by all_nodes_
  NodeMap all_nodes_;  // maps from node IDs to Node objects
  STLValueDeleter<NodeMap> all_nodes_deleter_;

  DISALLOW_COPY_AND_ASSIGN(SpdyPriorityTree);
};

template <typename NodeId>
SpdyPriorityTree<NodeId>::SpdyPriorityTree()
    : all_nodes_deleter_(&all_nodes_) {
  root_node_ = new Node();
  root_node_->id = kRootNodeId;
  root_node_->weight = kDefaultWeight;
  root_node_->parent = nullptr;
  root_node_->priority = 1.0;
  root_node_->ready = true;
  all_nodes_[kRootNodeId] = root_node_;
}

template <typename NodeId>
bool SpdyPriorityTree<NodeId>::NodePriorityComparator::operator()(
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
  return ContainsKey(all_nodes_, node_id);
}

template <typename NodeId>
bool SpdyPriorityTree<NodeId>::AddNode(NodeId node_id,
                                       NodeId parent_id,
                                       int weight,
                                       bool exclusive) {
  if (NodeExists(node_id) || weight < kMinWeight || weight > kMaxWeight) {
    return false;
  }
  Node* parent = FindNode(parent_id);
  if (parent == nullptr) {
    return false;
  }
  Node* new_node = new Node;
  new_node->id = node_id;
  new_node->weight = weight;
  new_node->parent = parent;
  all_nodes_[node_id] = new_node;
  if (exclusive) {
    // Move the parent's current children below the new node.
    using std::swap;
    swap(new_node->children, parent->children);
    new_node->total_child_weights = parent->total_child_weights;
    // Update each child's parent.
    for (Node* child : new_node->children) {
      child->parent = new_node;
    }
    // Clear parent's old child data.
    DCHECK(parent->children.empty());
    parent->total_child_weights = 0;
  }
  // Add new node to parent.
  parent->children.push_back(new_node);
  parent->total_child_weights += weight;
  return true;
}

template <typename NodeId>
bool SpdyPriorityTree<NodeId>::RemoveNode(NodeId node_id) {
  if (node_id == kRootNodeId) {
    return false;
  }
  // Remove the node from table.
  typename NodeMap::iterator it = all_nodes_.find(node_id);
  if (it == all_nodes_.end()) {
    return false;
  }
  scoped_ptr<Node> node(it->second);
  all_nodes_.erase(it);

  Node* parent = node->parent;
  // Remove the node from parent's child list.
  Remove(&parent->children, node.get());
  parent->total_child_weights -= node->weight;

  // Move the node's children to the parent's child list.
  // Update each child's parent and weight.
  for (Node* child : node->children) {
    child->parent = parent;
    parent->children.push_back(child);
    // Divide the removed node's weight among its children, rounding to the
    // nearest valid weight.
    float float_weight = node->weight * static_cast<float>(child->weight) /
                         static_cast<float>(node->total_child_weights);
    int new_weight = floor(float_weight + 0.5);
    if (new_weight == 0) {
      new_weight = 1;
    }
    child->weight = new_weight;
    parent->total_child_weights += child->weight;
  }

  return true;
}

template <typename NodeId>
int SpdyPriorityTree<NodeId>::GetWeight(NodeId node_id) const {
  const Node* node = FindNode(node_id);
  return (node == nullptr) ? 0 : node->weight;
}

template <typename NodeId>
NodeId SpdyPriorityTree<NodeId>::GetParent(NodeId node_id) const {
  const Node* node = FindNode(node_id);
  // Root node has null parent.
  return (node == nullptr || node->parent == nullptr) ? kRootNodeId
                                                      : node->parent->id;
}

template <typename NodeId>
std::vector<NodeId> SpdyPriorityTree<NodeId>::GetChildren(
    NodeId node_id) const {
  std::vector<NodeId> child_vec;
  const Node* node = FindNode(node_id);
  if (node != nullptr) {
    child_vec.reserve(node->children.size());
    for (Node* child : node->children) {
      child_vec.push_back(child->id);
    }
  }
  return child_vec;
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

  Node* node = all_nodes_[node_id];
  if (node->parent != nullptr) {
    node->parent->total_child_weights += (weight - node->weight);
  }
  node->weight = weight;

  return true;
}


template <typename NodeId>
bool SpdyPriorityTree<NodeId>::SetParent(
    NodeId node_id, NodeId parent_id, bool exclusive) {
  if (node_id == kRootNodeId || node_id == parent_id) {
    return false;
  }
  Node* node = FindNode(node_id);
  Node* new_parent = FindNode(parent_id);
  if (node == nullptr || new_parent == nullptr) {
    return false;
  }

  // If the new parent is already the node's parent, we're done.
  if (node->parent == new_parent) {
    return true;
  }

  // Next, check to see if the new parent is currently a descendant
  // of the node.
  Node* last = new_parent->parent;
  bool cycle_exists = false;
  while (last != nullptr) {
    if (last == node) {
      cycle_exists = true;
      break;
    }
    last = last->parent;
  }

  if (cycle_exists) {
    // The new parent moves to the level of the current node.
    SetParent(parent_id, node->parent->id, false);
  }

  // Remove node from old parent's child list.
  Node* old_parent = node->parent;
  Remove(&old_parent->children, node);
  old_parent->total_child_weights -= node->weight;

  if (exclusive) {
    // Move the new parent's current children below the current node.
    for (Node* child : new_parent->children) {
      child->parent = node;
      node->children.push_back(child);
    }
    node->total_child_weights += new_parent->total_child_weights;
    // Clear new parent's old child data.
    new_parent->children.clear();
    new_parent->total_child_weights = 0;
  }

  // Make the change.
  node->parent = new_parent;
  new_parent->children.push_back(node);
  new_parent->total_child_weights += node->weight;
  return true;
}

template <typename NodeId>
bool SpdyPriorityTree<NodeId>::SetBlocked(NodeId node_id, bool blocked) {
  if (!NodeExists(node_id)) {
    return false;
  }

  Node* node = all_nodes_[node_id];
  node->blocked = blocked;
  return true;
}

template <typename NodeId>
bool SpdyPriorityTree<NodeId>::SetReady(NodeId node_id, bool ready) {
  if (!NodeExists(node_id)) {
    return false;
  }
  Node* node = all_nodes_[node_id];
  node->ready = ready;
  return true;
}

template <typename NodeId>
bool SpdyPriorityTree<NodeId>::Remove(NodeVector* nodes, const Node* node) {
  for (typename NodeVector::iterator it = nodes->begin(); it != nodes->end();
       ++it) {
    if (*it == node) {
      nodes->erase(it);
      return true;
    }
  }
  return false;
}

template <typename NodeId>
void SpdyPriorityTree<NodeId>::PropagateNodeState(Node* node) {
  // Reset total_writeable_child_weights to its maximum value.
  node->total_writeable_child_weights = node->total_child_weights;
  for (Node* child : node->children) {
    PropagateNodeState(child);
  }
  if (node->total_writeable_child_weights == 0 &&
      (node->blocked || !node->ready)) {
    // Tell the parent that this entire subtree is unwriteable.
    node->parent->total_writeable_child_weights -= node->weight;
  }
}

template <typename NodeId>
const typename SpdyPriorityTree<NodeId>::Node*
SpdyPriorityTree<NodeId>::FindNode(NodeId node_id) const {
  typename NodeMap::const_iterator it = all_nodes_.find(node_id);
  return (it == all_nodes_.end() ? nullptr : it->second);
}

template <typename NodeId>
typename SpdyPriorityTree<NodeId>::Node* SpdyPriorityTree<NodeId>::FindNode(
    NodeId node_id) {
  typename NodeMap::const_iterator it = all_nodes_.find(node_id);
  return (it == all_nodes_.end() ? nullptr : it->second);
}

template <typename NodeId>
bool SpdyPriorityTree<NodeId>::HasChild(NodeId parent_id,
                                        NodeId child_id) const {
  const Node* parent = FindNode(parent_id);
  if (parent == nullptr) {
    return false;
  }
  auto found =
      std::find_if(parent->children.begin(), parent->children.end(),
                   [child_id](Node* node) { return node->id == child_id; });
  return found != parent->children.end();
}

template <typename NodeId>
std::vector<std::pair<NodeId, float> >
SpdyPriorityTree<NodeId>::GetPriorityList() {
  PriorityList priority_list;

  // Update total_writeable_child_weights to reflect the current
  // state of the tree.
  PropagateNodeState(root_node_);

  std::deque<Node*> queue;
  DCHECK(root_node_->priority == 1.0);
  // Start by examining our top-level nodes.
  for (Node* child : root_node_->children) {
    queue.push_back(child);
  }
  while (!queue.empty()) {
    Node* current_node = queue.front();
    const Node* parent_node = current_node->parent;
    if (current_node->blocked || !current_node->ready) {
      if (current_node->total_writeable_child_weights > 0) {
        // This node isn't writeable, but it has writeable children.
        // Calculate the total fraction of resources we can allot
        // to this subtree.
        current_node->priority = parent_node->priority *
            (static_cast<float>(current_node->weight) /
             static_cast<float>(parent_node->total_writeable_child_weights));
        // Examine the children.
        for (Node* child : current_node->children) {
          queue.push_back(child);
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
      priority_list.push_back(
          PriorityNode(current_node->id, current_node->priority));
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
  for (const auto& kv : all_nodes_) {
    ++total_nodes;
    ++nodes_visited;
    const Node& node = *kv.second;
    // All nodes except the root should have a parent, and should appear in
    // the children of that parent.
    if (node.id != kRootNodeId && !HasChild(node.parent->id, node.id)) {
      DLOG(INFO) << "Parent node " << node.parent->id
                 << " does not exist, or does not list node " << node.id
                 << " as its child.";
      return false;
    }

    if (!node.children.empty()) {
      int total_child_weights = 0;
      // Iterate through the node's children.
      for (Node* child : node.children) {
        ++nodes_visited;
        // Each node in the list should exist and should have this node
        // set as its parent.
        if (!NodeExists(child->id) || node.id != GetParent(child->id)) {
          DLOG(INFO) << "Child node " << child->id << " does not exist, "
                     << "or does not list " << node.id << " as its parent.";
          return false;
        }
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
