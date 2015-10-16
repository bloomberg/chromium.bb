// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/server_view_drawn_tracker.h"

#include "components/mus/ws/server_view.h"
#include "components/mus/ws/server_view_drawn_tracker_observer.h"

namespace mus {

ServerViewDrawnTracker::ServerViewDrawnTracker(
    ServerView* view,
    ServerViewDrawnTrackerObserver* observer)
    : view_(view), observer_(observer), drawn_(view->IsDrawn()) {
  AddObservers();
}

ServerViewDrawnTracker::~ServerViewDrawnTracker() {
  RemoveObservers();
}

void ServerViewDrawnTracker::SetDrawn(ServerView* ancestor, bool drawn) {
  if (drawn == drawn_)
    return;

  drawn_ = drawn;
  observer_->OnDrawnStateChanged(ancestor, view_, drawn);
}

void ServerViewDrawnTracker::AddObservers() {
  if (!view_)
    return;

  for (ServerView* v = view_; v; v = v->parent()) {
    v->AddObserver(this);
    views_.insert(v);
  }
}

void ServerViewDrawnTracker::RemoveObservers() {
  for (ServerView* view : views_)
    view->RemoveObserver(this);

  views_.clear();
}

void ServerViewDrawnTracker::OnViewDestroyed(ServerView* view) {
  // As views are removed before being destroyed, resulting in
  // OnViewHierarchyChanged() and us removing ourself as an observer, the only
  // view we should ever get notified of destruction on is |view_|.
  DCHECK_EQ(view, view_);
  RemoveObservers();
  view_ = nullptr;
  SetDrawn(nullptr, false);
}

void ServerViewDrawnTracker::OnViewHierarchyChanged(ServerView* view,
                                                    ServerView* new_parent,
                                                    ServerView* old_parent) {
  RemoveObservers();
  AddObservers();
  const bool is_drawn = view_->IsDrawn();
  SetDrawn(is_drawn ? nullptr : old_parent, is_drawn);
}

void ServerViewDrawnTracker::OnViewVisibilityChanged(ServerView* view) {
  const bool is_drawn = view_->IsDrawn();
  SetDrawn(is_drawn ? nullptr : view->parent(), is_drawn);
}

}  // namespace mus
