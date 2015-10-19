// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/server_window_drawn_tracker.h"

#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_drawn_tracker_observer.h"

namespace mus {

ServerWindowDrawnTracker::ServerWindowDrawnTracker(
    ServerWindow* window,
    ServerWindowDrawnTrackerObserver* observer)
    : window_(window), observer_(observer), drawn_(window->IsDrawn()) {
  AddObservers();
}

ServerWindowDrawnTracker::~ServerWindowDrawnTracker() {
  RemoveObservers();
}

void ServerWindowDrawnTracker::SetDrawn(ServerWindow* ancestor, bool drawn) {
  if (drawn == drawn_)
    return;

  drawn_ = drawn;
  observer_->OnDrawnStateChanged(ancestor, window_, drawn);
}

void ServerWindowDrawnTracker::AddObservers() {
  if (!window_)
    return;

  for (ServerWindow* v = window_; v; v = v->parent()) {
    v->AddObserver(this);
    windows_.insert(v);
  }
}

void ServerWindowDrawnTracker::RemoveObservers() {
  for (ServerWindow* window : windows_)
    window->RemoveObserver(this);

  windows_.clear();
}

void ServerWindowDrawnTracker::OnWindowDestroyed(ServerWindow* window) {
  // As windows are removed before being destroyed, resulting in
  // OnWindowHierarchyChanged() and us removing ourself as an observer, the only
  // window we should ever get notified of destruction on is |window_|.
  DCHECK_EQ(window, window_);
  RemoveObservers();
  window_ = nullptr;
  SetDrawn(nullptr, false);
}

void ServerWindowDrawnTracker::OnWindowHierarchyChanged(
    ServerWindow* window,
    ServerWindow* new_parent,
    ServerWindow* old_parent) {
  RemoveObservers();
  AddObservers();
  const bool is_drawn = window_->IsDrawn();
  SetDrawn(is_drawn ? nullptr : old_parent, is_drawn);
}

void ServerWindowDrawnTracker::OnWindowVisibilityChanged(ServerWindow* window) {
  const bool is_drawn = window_->IsDrawn();
  SetDrawn(is_drawn ? nullptr : window->parent(), is_drawn);
}

}  // namespace mus
