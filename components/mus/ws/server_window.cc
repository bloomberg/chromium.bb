// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/server_window.h"

#include <inttypes.h>

#include "base/strings/stringprintf.h"
#include "components/mus/ws/server_window_delegate.h"
#include "components/mus/ws/server_window_observer.h"
#include "mojo/converters/geometry/geometry_type_converters.h"

namespace mus {

namespace ws {

ServerWindow::ServerWindow(ServerWindowDelegate* delegate, const WindowId& id)
    : delegate_(delegate),
      id_(id),
      parent_(nullptr),
      visible_(false),
      opacity_(1),
      is_draggable_window_container_(false),
      // Don't notify newly added observers during notification. This causes
      // problems for code that adds an observer as part of an observer
      // notification (such as ServerWindowDrawTracker).
      observers_(
          base::ObserverList<ServerWindowObserver>::NOTIFY_EXISTING_ONLY) {
  DCHECK(delegate);  // Must provide a delegate.
}

ServerWindow::~ServerWindow() {
  FOR_EACH_OBSERVER(ServerWindowObserver, observers_,
                    OnWillDestroyWindow(this));

  while (!children_.empty())
    children_.front()->parent()->Remove(children_.front());

  if (parent_)
    parent_->Remove(this);

  FOR_EACH_OBSERVER(ServerWindowObserver, observers_, OnWindowDestroyed(this));
}

void ServerWindow::AddObserver(ServerWindowObserver* observer) {
  observers_.AddObserver(observer);
}

void ServerWindow::RemoveObserver(ServerWindowObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ServerWindow::Bind(mojo::InterfaceRequest<mojom::Surface> request,
                        mojom::SurfaceClientPtr client) {
  GetOrCreateSurface()->Bind(request.Pass(), client.Pass());
}

void ServerWindow::Add(ServerWindow* child) {
  // We assume validation checks happened already.
  DCHECK(child);
  DCHECK(child != this);
  DCHECK(!child->Contains(this));
  if (child->parent() == this) {
    if (children_.size() == 1)
      return;  // Already in the right position.
    Reorder(child, children_.back(), mojom::ORDER_DIRECTION_ABOVE);
    return;
  }

  ServerWindow* old_parent = child->parent();
  FOR_EACH_OBSERVER(ServerWindowObserver, child->observers_,
                    OnWillChangeWindowHierarchy(child, this, old_parent));

  if (child->parent())
    child->parent()->RemoveImpl(child);

  child->parent_ = this;
  children_.push_back(child);
  FOR_EACH_OBSERVER(ServerWindowObserver, child->observers_,
                    OnWindowHierarchyChanged(child, this, old_parent));
}

void ServerWindow::Remove(ServerWindow* child) {
  // We assume validation checks happened else where.
  DCHECK(child);
  DCHECK(child != this);
  DCHECK(child->parent() == this);

  FOR_EACH_OBSERVER(ServerWindowObserver, child->observers_,
                    OnWillChangeWindowHierarchy(child, nullptr, this));
  RemoveImpl(child);
  FOR_EACH_OBSERVER(ServerWindowObserver, child->observers_,
                    OnWindowHierarchyChanged(child, nullptr, this));
}

void ServerWindow::Reorder(ServerWindow* child,
                           ServerWindow* relative,
                           mojom::OrderDirection direction) {
  // We assume validation checks happened else where.
  DCHECK(child);
  DCHECK(child->parent() == this);
  DCHECK_GT(children_.size(), 1u);
  children_.erase(std::find(children_.begin(), children_.end(), child));
  Windows::iterator i = std::find(children_.begin(), children_.end(), relative);
  if (direction == mojom::ORDER_DIRECTION_ABOVE) {
    DCHECK(i != children_.end());
    children_.insert(++i, child);
  } else if (direction == mojom::ORDER_DIRECTION_BELOW) {
    DCHECK(i != children_.end());
    children_.insert(i, child);
  }
  FOR_EACH_OBSERVER(ServerWindowObserver, observers_,
                    OnWindowReordered(this, relative, direction));
}

void ServerWindow::SetBounds(const gfx::Rect& bounds) {
  if (bounds_ == bounds)
    return;

  // TODO(fsamuel): figure out how will this work with CompositorFrames.

  // |client_area_| is relative to the bounds. If the size of bounds change
  // we have to reset the client area. We assume any size change is quicky
  // followed by a client area change.
  if (bounds_.size() != bounds.size())
    client_area_ = gfx::Rect(bounds.size());

  const gfx::Rect old_bounds = bounds_;
  bounds_ = bounds;
  FOR_EACH_OBSERVER(ServerWindowObserver, observers_,
                    OnWindowBoundsChanged(this, old_bounds, bounds));
}

void ServerWindow::SetClientArea(const gfx::Rect& bounds) {
  if (client_area_ == bounds)
    return;

  const gfx::Rect old_client_area = client_area_;
  client_area_ = bounds;
  FOR_EACH_OBSERVER(ServerWindowObserver, observers_,
                    OnWindowClientAreaChanged(this, old_client_area, bounds));
}

const ServerWindow* ServerWindow::GetRoot() const {
  return delegate_->GetRootWindow(this);
}

std::vector<const ServerWindow*> ServerWindow::GetChildren() const {
  std::vector<const ServerWindow*> children;
  children.reserve(children_.size());
  for (size_t i = 0; i < children_.size(); ++i)
    children.push_back(children_[i]);
  return children;
}

std::vector<ServerWindow*> ServerWindow::GetChildren() {
  // TODO(sky): rename to children() and fix return type.
  return children_;
}

ServerWindow* ServerWindow::GetChildWindow(const WindowId& window_id) {
  if (id_ == window_id)
    return this;

  for (ServerWindow* child : children_) {
    ServerWindow* window = child->GetChildWindow(window_id);
    if (window)
      return window;
  }

  return nullptr;
}

bool ServerWindow::Contains(const ServerWindow* window) const {
  for (const ServerWindow* parent = window; parent; parent = parent->parent_) {
    if (parent == this)
      return true;
  }
  return false;
}

void ServerWindow::SetVisible(bool value) {
  if (visible_ == value)
    return;

  FOR_EACH_OBSERVER(ServerWindowObserver, observers_,
                    OnWillChangeWindowVisibility(this));
  visible_ = value;
  FOR_EACH_OBSERVER(ServerWindowObserver, observers_,
                    OnWindowVisibilityChanged(this));
}

void ServerWindow::SetOpacity(float value) {
  if (value == opacity_)
    return;
  opacity_ = value;
  delegate_->OnScheduleWindowPaint(this);
}

void ServerWindow::SetTransform(const gfx::Transform& transform) {
  if (transform_ == transform)
    return;

  transform_ = transform;
  delegate_->OnScheduleWindowPaint(this);
}

void ServerWindow::SetProperty(const std::string& name,
                               const std::vector<uint8_t>* value) {
  auto it = properties_.find(name);
  if (it != properties_.end()) {
    if (value && it->second == *value)
      return;
  } else if (!value) {
    // This property isn't set in |properties_| and |value| is NULL, so there's
    // no change.
    return;
  }

  if (value) {
    properties_[name] = *value;
  } else if (it != properties_.end()) {
    properties_.erase(it);
  }

  FOR_EACH_OBSERVER(ServerWindowObserver, observers_,
                    OnWindowSharedPropertyChanged(this, name, value));
}

void ServerWindow::SetTextInputState(const ui::TextInputState& state) {
  const bool changed = !(text_input_state_ == state);
  if (changed) {
    text_input_state_ = state;
    // keyboard even if the state is not changed. So we have to notify
    // |observers_|.
    FOR_EACH_OBSERVER(ServerWindowObserver, observers_,
                      OnWindowTextInputStateChanged(this, state));
  }
}

bool ServerWindow::IsDrawn() const {
  const ServerWindow* root = delegate_->GetRootWindow(this);
  if (!root || !root->visible())
    return false;
  const ServerWindow* window = this;
  while (window && window != root && window->visible())
    window = window->parent();
  return root == window;
}

ServerWindowSurface* ServerWindow::GetOrCreateSurface() {
  if (!surface_)
    surface_.reset(new ServerWindowSurface(this));
  return surface_.get();
}

#if !defined(NDEBUG)
std::string ServerWindow::GetDebugWindowHierarchy() const {
  std::string result;
  BuildDebugInfo(std::string(), &result);
  return result;
}

void ServerWindow::BuildDebugInfo(const std::string& depth,
                                  std::string* result) const {
  *result += base::StringPrintf(
      "%sid=%d,%d visible=%s bounds=%d,%d %dx%d" PRIu64 "\n", depth.c_str(),
      static_cast<int>(id_.connection_id), static_cast<int>(id_.window_id),
      visible_ ? "true" : "false", bounds_.x(), bounds_.y(), bounds_.width(),
      bounds_.height());
  for (const ServerWindow* child : children_)
    child->BuildDebugInfo(depth + "  ", result);
}
#endif

void ServerWindow::RemoveImpl(ServerWindow* window) {
  window->parent_ = NULL;
  children_.erase(std::find(children_.begin(), children_.end(), window));
}

}  // namespace ws

}  // namespace mus
