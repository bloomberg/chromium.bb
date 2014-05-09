// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/node.h"

#include "mojo/services/view_manager/node_delegate.h"
#include "mojo/services/view_manager/view.h"
#include "ui/aura/window_property.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"

DECLARE_WINDOW_PROPERTY_TYPE(mojo::services::view_manager::Node*);

namespace mojo {
namespace services {
namespace view_manager {

DEFINE_WINDOW_PROPERTY_KEY(Node*, kNodeKey, NULL);

Node::Node(NodeDelegate* delegate, const NodeId& id)
    : delegate_(delegate),
      id_(id),
      view_(NULL),
      window_(this) {
  DCHECK(delegate);  // Must provide a delegate.
  window_.set_owned_by_parent(false);
  window_.AddObserver(this);
  window_.SetProperty(kNodeKey, this);
  window_.Init(aura::WINDOW_LAYER_TEXTURED);

  // TODO(sky): this likely needs to be false and add a visibility API.
  window_.Show();
}

Node::~Node() {
  SetView(NULL);
}

Node* Node::GetParent() {
  if (!window_.parent())
    return NULL;
  return window_.parent()->GetProperty(kNodeKey);
}

void Node::Add(Node* child) {
  window_.AddChild(&child->window_);
}

void Node::Remove(Node* child) {
  window_.RemoveChild(&child->window_);
}

std::vector<Node*> Node::GetChildren() {
  std::vector<Node*> children;
  children.reserve(window_.children().size());
  for (size_t i = 0; i < window_.children().size(); ++i)
    children.push_back(window_.children()[i]->GetProperty(kNodeKey));
  return children;
}

void Node::SetView(View* view) {
  if (view == view_)
    return;

  // Detach view from existing node. This way notifications are sent out.
  if (view && view->node())
    view->node()->SetView(NULL);

  ViewId old_view_id;
  if (view_) {
    view_->set_node(NULL);
    old_view_id = view_->id();
  }
  view_ = view;
  ViewId view_id;
  if (view) {
    view_id = view->id();
    view->set_node(this);
  }
  delegate_->OnNodeViewReplaced(id_, view_id, old_view_id);
}

void Node::OnWindowHierarchyChanged(
    const aura::WindowObserver::HierarchyChangeParams& params) {
  if (params.target != &window_ || params.receiver != &window_)
    return;
  NodeId new_parent_id;
  if (params.new_parent && params.new_parent->GetProperty(kNodeKey))
    new_parent_id = params.new_parent->GetProperty(kNodeKey)->id();
  NodeId old_parent_id;
  if (params.old_parent && params.old_parent->GetProperty(kNodeKey))
    old_parent_id = params.old_parent->GetProperty(kNodeKey)->id();
  delegate_->OnNodeHierarchyChanged(id_, new_parent_id, old_parent_id);
}

gfx::Size Node::GetMinimumSize() const {
  return gfx::Size();
}

gfx::Size Node::GetMaximumSize() const {
  return gfx::Size();
}

void Node::OnBoundsChanged(const gfx::Rect& old_bounds,
                           const gfx::Rect& new_bounds) {
}

gfx::NativeCursor Node::GetCursor(const gfx::Point& point) {
  return gfx::kNullCursor;
}

int Node::GetNonClientComponent(const gfx::Point& point) const {
  return HTCAPTION;
}

bool Node::ShouldDescendIntoChildForEventHandling(
    aura::Window* child,
    const gfx::Point& location) {
  return true;
}

bool Node::CanFocus() {
  return true;
}

void Node::OnCaptureLost() {
}

void Node::OnPaint(gfx::Canvas* canvas) {
  if (view_) {
    canvas->DrawImageInt(
        gfx::ImageSkia::CreateFrom1xBitmap(view_->bitmap()), 0, 0);
  }
}

void Node::OnDeviceScaleFactorChanged(float device_scale_factor) {
}

void Node::OnWindowDestroying(aura::Window* window) {
}

void Node::OnWindowDestroyed(aura::Window* window) {
}

void Node::OnWindowTargetVisibilityChanged(bool visible) {
}

bool Node::HasHitTestMask() const {
  return false;
}

void Node::GetHitTestMask(gfx::Path* mask) const {
}

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
