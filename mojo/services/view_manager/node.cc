// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/node.h"

#include "mojo/services/view_manager/node_delegate.h"

namespace mojo {
namespace service {

Node::Node(NodeDelegate* delegate, const NodeId& id)
    : delegate_(delegate),
      id_(id),
      parent_(NULL),
      visible_(true) {
  DCHECK(delegate);  // Must provide a delegate.
}

Node::~Node() {
  while (!children_.empty())
    children_.front()->parent()->Remove(children_.front());

  if (parent_)
    parent_->Remove(this);

  delegate_->OnNodeDestroyed(this);
}

void Node::Add(Node* child) {
  // We assume validation checks happened already.
  DCHECK(child);
  DCHECK(child != this);
  DCHECK(!child->Contains(this));
  if (child->parent() == this) {
    if (children_.size() == 1)
      return;  // Already in the right position.
    Reorder(child, children_.back(), ORDER_DIRECTION_ABOVE);
    return;
  }

  const Node* old_parent = child->parent();
  if (child->parent())
    child->parent()->RemoveImpl(child);

  child->parent_ = this;
  children_.push_back(child);
  child->delegate_->OnNodeHierarchyChanged(child, this, old_parent);
}

void Node::Remove(Node* child) {
  // We assume validation checks happened else where.
  DCHECK(child);
  DCHECK(child != this);
  DCHECK(child->parent() == this);

  RemoveImpl(child);
  child->delegate_->OnNodeHierarchyChanged(child, NULL, this);
}

void Node::Reorder(Node* child, Node* relative, OrderDirection direction) {
  // We assume validation checks happened else where.
  DCHECK(child);
  DCHECK(child->parent() == this);
  DCHECK_GT(children_.size(), 1u);
  children_.erase(std::find(children_.begin(), children_.end(), child));
  Nodes::iterator i = std::find(children_.begin(), children_.end(), relative);
  if (direction == ORDER_DIRECTION_ABOVE) {
    DCHECK(i != children_.end());
    children_.insert(++i, child);
  } else if (direction == ORDER_DIRECTION_BELOW) {
    DCHECK(i != children_.end());
    children_.insert(i, child);
  }
}

void Node::SetBounds(const gfx::Rect& bounds) {
  if (bounds_ == bounds)
    return;

  const gfx::Rect old_bounds = bounds_;
  bounds_ = bounds;
  delegate_->OnNodeBoundsChanged(this, old_bounds, bounds);
}

const Node* Node::GetRoot() const {
  const Node* node = this;
  while (node && node->parent())
    node = node->parent();
  return node;
}

std::vector<const Node*> Node::GetChildren() const {
  std::vector<const Node*> children;
  children.reserve(children_.size());
  for (size_t i = 0; i < children_.size(); ++i)
    children.push_back(children_[i]);
  return children;
}

std::vector<Node*> Node::GetChildren() {
  // TODO(sky): rename to children() and fix return type.
  return children_;
}

bool Node::Contains(const Node* node) const {
  for (const Node* parent = node; parent; parent = parent->parent_) {
    if (parent == this)
      return true;
  }
  return false;
}

void Node::SetVisible(bool value) {
  if (visible_ == value)
    return;

  visible_ = value;
  // TODO(sky): notification, including repaint.
}

void Node::SetBitmap(const SkBitmap& bitmap) {
  bitmap_ = bitmap;
  delegate_->OnNodeBitmapChanged(this);
}

void Node::RemoveImpl(Node* node) {
  node->parent_ = NULL;
  children_.erase(std::find(children_.begin(), children_.end(), node));
}

}  // namespace service
}  // namespace mojo
