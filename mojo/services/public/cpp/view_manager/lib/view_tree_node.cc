// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/view_tree_node.h"

#include "mojo/services/public/cpp/view_manager/lib/view_manager_synchronizer.h"
#include "mojo/services/public/cpp/view_manager/lib/view_private.h"
#include "mojo/services/public/cpp/view_manager/lib/view_tree_node_private.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node_observer.h"

namespace mojo {
namespace view_manager {

namespace {

void NotifyViewTreeChangeAtReceiver(
    ViewTreeNode* receiver,
    const ViewTreeNodeObserver::TreeChangeParams& params) {
  ViewTreeNodeObserver::TreeChangeParams local_params = params;
  local_params.receiver = receiver;
  FOR_EACH_OBSERVER(ViewTreeNodeObserver,
                    *ViewTreeNodePrivate(receiver).observers(),
                    OnTreeChange(local_params));
}

void NotifyViewTreeChangeUp(
    ViewTreeNode* start_at,
    const ViewTreeNodeObserver::TreeChangeParams& params) {
  for (ViewTreeNode* current = start_at; current; current = current->parent())
    NotifyViewTreeChangeAtReceiver(current, params);
}

void NotifyViewTreeChangeDown(
    ViewTreeNode* start_at,
    const ViewTreeNodeObserver::TreeChangeParams& params) {
  NotifyViewTreeChangeAtReceiver(start_at, params);
  ViewTreeNode::Children::const_iterator it = start_at->children().begin();
  for (; it != start_at->children().end(); ++it)
    NotifyViewTreeChangeDown(*it, params);
}

void NotifyViewTreeChange(
    const ViewTreeNodeObserver::TreeChangeParams& params) {
  NotifyViewTreeChangeDown(params.target, params);
  if (params.old_parent)
    NotifyViewTreeChangeUp(params.old_parent, params);
  if (params.new_parent)
    NotifyViewTreeChangeUp(params.new_parent, params);
}

class ScopedTreeNotifier {
 public:
  ScopedTreeNotifier(ViewTreeNode* target,
                     ViewTreeNode* old_parent,
                     ViewTreeNode* new_parent) {
    params_.target = target;
    params_.old_parent = old_parent;
    params_.new_parent = new_parent;
    NotifyViewTreeChange(params_);
  }
  ~ScopedTreeNotifier() {
    params_.phase = ViewTreeNodeObserver::DISPOSITION_CHANGED;
    NotifyViewTreeChange(params_);
  }

 private:
  ViewTreeNodeObserver::TreeChangeParams params_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTreeNotifier);
};

void RemoveChildImpl(ViewTreeNode* child, ViewTreeNode::Children* children) {
  ViewTreeNode::Children::iterator it =
      std::find(children->begin(), children->end(), child);
  if (it != children->end()) {
    children->erase(it);
    ViewTreeNodePrivate(child).ClearParent();
  }
}

class ScopedOrderChangedNotifier {
 public:
  ScopedOrderChangedNotifier(ViewTreeNode* node,
                             ViewTreeNode* relative_node,
                             OrderDirection direction)
      : node_(node),
        relative_node_(relative_node),
        direction_(direction) {
    FOR_EACH_OBSERVER(
        ViewTreeNodeObserver,
        *ViewTreeNodePrivate(node_).observers(),
        OnNodeReordered(node_,
                        relative_node_,
                        direction_,
                        ViewTreeNodeObserver::DISPOSITION_CHANGING));

  }
  ~ScopedOrderChangedNotifier() {
    FOR_EACH_OBSERVER(
        ViewTreeNodeObserver,
        *ViewTreeNodePrivate(node_).observers(),
        OnNodeReordered(node_,
                        relative_node_,
                        direction_,
                        ViewTreeNodeObserver::DISPOSITION_CHANGED));
  }

 private:
  ViewTreeNode* node_;
  ViewTreeNode* relative_node_;
  OrderDirection direction_;

  DISALLOW_COPY_AND_ASSIGN(ScopedOrderChangedNotifier);
};

// Returns true if the order actually changed.
bool ReorderImpl(ViewTreeNode::Children* children,
                 ViewTreeNode* node,
                 ViewTreeNode* relative,
                 OrderDirection direction) {
  DCHECK(relative);
  DCHECK_NE(node, relative);
  DCHECK_EQ(node->parent(), relative->parent());

  const size_t child_i =
      std::find(children->begin(), children->end(), node) - children->begin();
  const size_t target_i =
      std::find(children->begin(), children->end(), relative) -
      children->begin();
  if ((direction == ORDER_ABOVE && child_i == target_i + 1) ||
      (direction == ORDER_BELOW && child_i + 1 == target_i)) {
    return false;
  }

  ScopedOrderChangedNotifier notifier(node, relative, direction);

  const size_t dest_i =
      direction == ORDER_ABOVE ?
      (child_i < target_i ? target_i : target_i + 1) :
      (child_i < target_i ? target_i - 1 : target_i);
  children->erase(children->begin() + child_i);
  children->insert(children->begin() + dest_i, node);

  return true;
}

class ScopedSetActiveViewNotifier {
 public:
  ScopedSetActiveViewNotifier(ViewTreeNode* node,
                              View* old_view,
                              View* new_view)
      : node_(node),
        old_view_(old_view),
        new_view_(new_view) {
    FOR_EACH_OBSERVER(
        ViewTreeNodeObserver,
        *ViewTreeNodePrivate(node).observers(),
        OnNodeActiveViewChange(node_,
                               old_view_,
                               new_view_,
                               ViewTreeNodeObserver::DISPOSITION_CHANGING));
  }
  ~ScopedSetActiveViewNotifier() {
    FOR_EACH_OBSERVER(
        ViewTreeNodeObserver,
        *ViewTreeNodePrivate(node_).observers(),
        OnNodeActiveViewChange(node_,
                               old_view_,
                               new_view_,
                               ViewTreeNodeObserver::DISPOSITION_CHANGED));
  }

 private:
  ViewTreeNode* node_;
  View* old_view_;
  View* new_view_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSetActiveViewNotifier);
};

class ScopedSetBoundsNotifier {
 public:
  ScopedSetBoundsNotifier(ViewTreeNode* node,
                          const gfx::Rect& old_bounds,
                          const gfx::Rect& new_bounds)
      : node_(node),
        old_bounds_(old_bounds),
        new_bounds_(new_bounds) {
    FOR_EACH_OBSERVER(
        ViewTreeNodeObserver,
        *ViewTreeNodePrivate(node_).observers(),
        OnNodeBoundsChange(node_,
                           old_bounds_,
                           new_bounds_,
                           ViewTreeNodeObserver::DISPOSITION_CHANGING));
  }
  ~ScopedSetBoundsNotifier() {
    FOR_EACH_OBSERVER(
        ViewTreeNodeObserver,
        *ViewTreeNodePrivate(node_).observers(),
        OnNodeBoundsChange(node_,
                           old_bounds_,
                           new_bounds_,
                           ViewTreeNodeObserver::DISPOSITION_CHANGED));
  }

 private:
  ViewTreeNode* node_;
  const gfx::Rect old_bounds_;
  const gfx::Rect new_bounds_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSetBoundsNotifier);
};

class ScopedDestructionNotifier {
 public:
  explicit ScopedDestructionNotifier(ViewTreeNode* node)
      : node_(node) {
    FOR_EACH_OBSERVER(
        ViewTreeNodeObserver,
        *ViewTreeNodePrivate(node_).observers(),
        OnNodeDestroy(node_, ViewTreeNodeObserver::DISPOSITION_CHANGING));
  }
  ~ScopedDestructionNotifier() {
    FOR_EACH_OBSERVER(
        ViewTreeNodeObserver,
        *ViewTreeNodePrivate(node_).observers(),
        OnNodeDestroy(node_, ViewTreeNodeObserver::DISPOSITION_CHANGED));
  }

 private:
  ViewTreeNode* node_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDestructionNotifier);
};

// Some operations are only permitted in the connection that created the node.
bool OwnsNode(ViewManager* manager, ViewTreeNode* node) {
  return !manager ||
      static_cast<ViewManagerSynchronizer*>(manager)->OwnsNode(node->id());
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ViewTreeNode, public:

// static
ViewTreeNode* ViewTreeNode::Create(ViewManager* view_manager) {
  ViewTreeNode* node = new ViewTreeNode(view_manager);
  static_cast<ViewManagerSynchronizer*>(view_manager)->AddNode(node);
  return node;
}

void ViewTreeNode::Destroy() {
  if (!OwnsNode(manager_, this))
    return;

  if (manager_)
    static_cast<ViewManagerSynchronizer*>(manager_)->DestroyViewTreeNode(id_);
  while (!children_.empty())
    children_.front()->Destroy();
  LocalDestroy();
}

void ViewTreeNode::SetBounds(const gfx::Rect& bounds) {
  if (!OwnsNode(manager_, this))
    return;

  if (manager_)
    static_cast<ViewManagerSynchronizer*>(manager_)->SetBounds(id_, bounds);
  LocalSetBounds(bounds_, bounds);
}

void ViewTreeNode::AddObserver(ViewTreeNodeObserver* observer) {
  observers_.AddObserver(observer);
}

void ViewTreeNode::RemoveObserver(ViewTreeNodeObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ViewTreeNode::AddChild(ViewTreeNode* child) {
  // TODO(beng): not necessarily valid to all connections, but possibly to the
  //             embeddee in an embedder-embeddee relationship.
  if (manager_)
    CHECK_EQ(ViewTreeNodePrivate(child).view_manager(), manager_);
  LocalAddChild(child);
  if (manager_)
    static_cast<ViewManagerSynchronizer*>(manager_)->AddChild(child->id(), id_);
}

void ViewTreeNode::RemoveChild(ViewTreeNode* child) {
  // TODO(beng): not necessarily valid to all connections, but possibly to the
  //             embeddee in an embedder-embeddee relationship.
  if (manager_)
    CHECK_EQ(ViewTreeNodePrivate(child).view_manager(), manager_);
  LocalRemoveChild(child);
  if (manager_) {
    static_cast<ViewManagerSynchronizer*>(manager_)->RemoveChild(child->id(),
                                                                 id_);
  }
}

void ViewTreeNode::MoveToFront() {
  Reorder(parent_->children_.back(), ORDER_ABOVE);
}

void ViewTreeNode::MoveToBack() {
  Reorder(parent_->children_.front(), ORDER_BELOW);
}

void ViewTreeNode::Reorder(ViewTreeNode* relative, OrderDirection direction) {
  if (!LocalReorder(relative, direction))
    return;
  if (manager_) {
    static_cast<ViewManagerSynchronizer*>(manager_)->Reorder(id_,
                                                             relative->id(),
                                                             direction);
  }
}

bool ViewTreeNode::Contains(ViewTreeNode* child) const {
  if (manager_)
    CHECK_EQ(ViewTreeNodePrivate(child).view_manager(), manager_);
  for (ViewTreeNode* p = child->parent(); p; p = p->parent()) {
    if (p == this)
      return true;
  }
  return false;
}

ViewTreeNode* ViewTreeNode::GetChildById(Id id) {
  if (id == id_)
    return this;
  // TODO(beng): this could be improved depending on how we decide to own nodes.
  Children::const_iterator it = children_.begin();
  for (; it != children_.end(); ++it) {
    ViewTreeNode* node = (*it)->GetChildById(id);
    if (node)
      return node;
  }
  return NULL;
}

void ViewTreeNode::SetActiveView(View* view) {
  // TODO(beng): not necessarily valid to all connections, but possibly to the
  //             embeddee in an embedder-embeddee relationship.
  if (manager_)
    CHECK_EQ(ViewPrivate(view).view_manager(), manager_);
  LocalSetActiveView(view);
  if (manager_) {
    static_cast<ViewManagerSynchronizer*>(manager_)->SetActiveView(
        id_, active_view_->id());
  }
}

void ViewTreeNode::Embed(const String& url) {
  static_cast<ViewManagerSynchronizer*>(manager_)->Embed(url, id_);
}

////////////////////////////////////////////////////////////////////////////////
// ViewTreeNode, protected:

ViewTreeNode::ViewTreeNode()
    : manager_(NULL),
      id_(-1),
      parent_(NULL),
      active_view_(NULL) {}

ViewTreeNode::~ViewTreeNode() {
  ScopedDestructionNotifier notifier(this);
  if (parent_)
    parent_->LocalRemoveChild(this);
  // TODO(beng): It'd be better to do this via a destruction observer in the
  //             synchronizer.
  if (manager_)
    static_cast<ViewManagerSynchronizer*>(manager_)->RemoveNode(id_);
}

////////////////////////////////////////////////////////////////////////////////
// ViewTreeNode, private:

ViewTreeNode::ViewTreeNode(ViewManager* manager)
    : manager_(manager),
      id_(static_cast<ViewManagerSynchronizer*>(
          manager_)->CreateViewTreeNode()),
      parent_(NULL),
      active_view_(NULL) {}

void ViewTreeNode::LocalDestroy() {
  delete this;
}

void ViewTreeNode::LocalAddChild(ViewTreeNode* child) {
  ScopedTreeNotifier notifier(child, child->parent(), this);
  if (child->parent())
    RemoveChildImpl(child, &child->parent_->children_);
  children_.push_back(child);
  child->parent_ = this;
}

void ViewTreeNode::LocalRemoveChild(ViewTreeNode* child) {
  DCHECK_EQ(this, child->parent());
  ScopedTreeNotifier notifier(child, this, NULL);
  RemoveChildImpl(child, &children_);
}

bool ViewTreeNode::LocalReorder(ViewTreeNode* relative,
                                OrderDirection direction) {
  return ReorderImpl(&parent_->children_, this, relative, direction);
}

void ViewTreeNode::LocalSetActiveView(View* view) {
  ScopedSetActiveViewNotifier notifier(this, active_view_, view);
  if (active_view_)
    ViewPrivate(active_view_).set_node(NULL);
  active_view_ = view;
  if (active_view_)
    ViewPrivate(active_view_).set_node(this);
}

void ViewTreeNode::LocalSetBounds(const gfx::Rect& old_bounds,
                                  const gfx::Rect& new_bounds) {
  DCHECK(old_bounds == bounds_);
  ScopedSetBoundsNotifier notifier(this, old_bounds, new_bounds);
  bounds_ = new_bounds;
}

}  // namespace view_manager
}  // namespace mojo

