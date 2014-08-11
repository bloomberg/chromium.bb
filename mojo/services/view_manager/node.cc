// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/node.h"

#include "mojo/services/view_manager/node_delegate.h"
#include "ui/aura/window_property.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"

DECLARE_WINDOW_PROPERTY_TYPE(mojo::service::Node*);

namespace mojo {
namespace service {

DEFINE_WINDOW_PROPERTY_KEY(Node*, kNodeKey, NULL);

Node::Node(NodeDelegate* delegate, const NodeId& id)
    : delegate_(delegate),
      id_(id),
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
  // This is implicitly done during deletion of the window, but we do it here so
  // that we're in a known state.
  if (window_.parent())
    window_.parent()->RemoveChild(&window_);

  delegate_->OnNodeDestroyed(this);
}

// static
Node* Node::NodeForWindow(aura::Window* window) {
  return window->GetProperty(kNodeKey);
}

const Node* Node::GetParent() const {
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

void Node::Reorder(Node* child, Node* relative, OrderDirection direction) {
  if (direction == ORDER_DIRECTION_ABOVE)
    window_.StackChildAbove(child->window(), relative->window());
  else if (direction == ORDER_DIRECTION_BELOW)
    window_.StackChildBelow(child->window(), relative->window());
}

const Node* Node::GetRoot() const {
  const aura::Window* window = &window_;
  while (window && window->parent())
    window = window->parent();
  return window->GetProperty(kNodeKey);
}

std::vector<const Node*> Node::GetChildren() const {
  std::vector<const Node*> children;
  children.reserve(window_.children().size());
  for (size_t i = 0; i < window_.children().size(); ++i)
    children.push_back(window_.children()[i]->GetProperty(kNodeKey));
  return children;
}

std::vector<Node*> Node::GetChildren() {
  std::vector<Node*> children;
  children.reserve(window_.children().size());
  for (size_t i = 0; i < window_.children().size(); ++i)
    children.push_back(window_.children()[i]->GetProperty(kNodeKey));
  return children;
}

bool Node::Contains(const Node* node) const {
  return node && window_.Contains(&(node->window_));
}

bool Node::IsVisible() const {
  return window_.TargetVisibility();
}

void Node::SetVisible(bool value) {
  if (value)
    window_.Show();
  else
    window_.Hide();
}

void Node::SetBitmap(const SkBitmap& bitmap) {
  bitmap_ = bitmap;
  window_.SchedulePaintInRect(gfx::Rect(window_.bounds().size()));
}

void Node::OnWindowHierarchyChanged(
    const aura::WindowObserver::HierarchyChangeParams& params) {
  if (params.target != &window_ || params.receiver != &window_)
    return;
  const Node* new_parent = params.new_parent ?
      params.new_parent->GetProperty(kNodeKey) : NULL;
  const Node* old_parent = params.old_parent ?
      params.old_parent->GetProperty(kNodeKey) : NULL;
  // This check is needed because even the root Node's aura::Window has a
  // parent, but the Node itself has no parent (so it's possible for us to
  // receive this notification from aura when no logical Node hierarchy change
  // has actually ocurred).
  if (new_parent != old_parent)
    delegate_->OnNodeHierarchyChanged(this, new_parent, old_parent);
}

gfx::Size Node::GetMinimumSize() const {
  return gfx::Size();
}

gfx::Size Node::GetMaximumSize() const {
  return gfx::Size();
}

void Node::OnBoundsChanged(const gfx::Rect& old_bounds,
                           const gfx::Rect& new_bounds) {
  delegate_->OnNodeBoundsChanged(this, old_bounds, new_bounds);
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
  canvas->DrawImageInt(gfx::ImageSkia::CreateFrom1xBitmap(bitmap_), 0, 0);
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

void Node::OnEvent(ui::Event* event) {
  delegate_->OnNodeInputEvent(this, event);
}

}  // namespace service
}  // namespace mojo
