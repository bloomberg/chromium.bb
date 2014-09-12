// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/server_view.h"

#include "mojo/services/view_manager/server_view_delegate.h"

namespace mojo {
namespace service {

ServerView::ServerView(ServerViewDelegate* delegate, const ViewId& id)
    : delegate_(delegate), id_(id), parent_(NULL), visible_(true) {
  DCHECK(delegate);  // Must provide a delegate.
}

ServerView::~ServerView() {
  while (!children_.empty())
    children_.front()->parent()->Remove(children_.front());

  if (parent_)
    parent_->Remove(this);

  delegate_->OnViewDestroyed(this);
}

void ServerView::Add(ServerView* child) {
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

  const ServerView* old_parent = child->parent();
  child->delegate_->OnWillChangeViewHierarchy(child, this, old_parent);
  if (child->parent())
    child->parent()->RemoveImpl(child);

  child->parent_ = this;
  children_.push_back(child);
  child->delegate_->OnViewHierarchyChanged(child, this, old_parent);
}

void ServerView::Remove(ServerView* child) {
  // We assume validation checks happened else where.
  DCHECK(child);
  DCHECK(child != this);
  DCHECK(child->parent() == this);

  child->delegate_->OnWillChangeViewHierarchy(child, NULL, this);
  RemoveImpl(child);
  child->delegate_->OnViewHierarchyChanged(child, NULL, this);
}

void ServerView::Reorder(ServerView* child,
                         ServerView* relative,
                         OrderDirection direction) {
  // We assume validation checks happened else where.
  DCHECK(child);
  DCHECK(child->parent() == this);
  DCHECK_GT(children_.size(), 1u);
  children_.erase(std::find(children_.begin(), children_.end(), child));
  Views::iterator i = std::find(children_.begin(), children_.end(), relative);
  if (direction == ORDER_DIRECTION_ABOVE) {
    DCHECK(i != children_.end());
    children_.insert(++i, child);
  } else if (direction == ORDER_DIRECTION_BELOW) {
    DCHECK(i != children_.end());
    children_.insert(i, child);
  }
}

void ServerView::SetBounds(const gfx::Rect& bounds) {
  if (bounds_ == bounds)
    return;

  const gfx::Rect old_bounds = bounds_;
  bounds_ = bounds;
  delegate_->OnViewBoundsChanged(this, old_bounds, bounds);
}

const ServerView* ServerView::GetRoot() const {
  const ServerView* view = this;
  while (view && view->parent())
    view = view->parent();
  return view;
}

std::vector<const ServerView*> ServerView::GetChildren() const {
  std::vector<const ServerView*> children;
  children.reserve(children_.size());
  for (size_t i = 0; i < children_.size(); ++i)
    children.push_back(children_[i]);
  return children;
}

std::vector<ServerView*> ServerView::GetChildren() {
  // TODO(sky): rename to children() and fix return type.
  return children_;
}

bool ServerView::Contains(const ServerView* view) const {
  for (const ServerView* parent = view; parent; parent = parent->parent_) {
    if (parent == this)
      return true;
  }
  return false;
}

void ServerView::SetVisible(bool value) {
  if (visible_ == value)
    return;

  delegate_->OnWillChangeViewVisibility(this);
  visible_ = value;
}

bool ServerView::IsDrawn(const ServerView* root) const {
  if (!root->visible_)
    return false;
  const ServerView* view = this;
  while (view && view != root && view->visible_)
    view = view->parent_;
  return view == root;
}

void ServerView::SetSurfaceId(cc::SurfaceId surface_id) {
  surface_id_ = surface_id;
  delegate_->OnViewSurfaceIdChanged(this);
}

void ServerView::RemoveImpl(ServerView* view) {
  view->parent_ = NULL;
  children_.erase(std::find(children_.begin(), children_.end(), view));
}

}  // namespace service
}  // namespace mojo
