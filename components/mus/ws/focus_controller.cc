// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/focus_controller.h"

#include "components/mus/ws/focus_controller_observer.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_drawn_tracker.h"

namespace mus {
namespace ws {

FocusController::FocusController() {}

FocusController::~FocusController() {}

void FocusController::SetFocusedWindow(ServerWindow* window) {
  if (GetFocusedWindow() == window)
    return;

  SetFocusedWindowImpl(FocusControllerChangeSource::EXPLICIT, window);
}

ServerWindow* FocusController::GetFocusedWindow() {
  return drawn_tracker_ ? drawn_tracker_->window() : nullptr;
}

void FocusController::AddObserver(FocusControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void FocusController::RemoveObserver(FocusControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool FocusController::CanBeFocused(ServerWindow* window) const {
  // All ancestors of |window| must be drawn, and be focusable.
  for (ServerWindow* w = window; w; w = w->parent()) {
    if (!w->IsDrawn())
      return false;
    if (!w->can_focus())
      return false;
  }

  // |window| must be a descendent of an activatable window.
  for (ServerWindow* w = window; w; w = w->parent()) {
    if (CanBeActivated(w))
      return true;
  }

  return false;
}

bool FocusController::CanBeActivated(ServerWindow* window) const {
  // TODO(sad): Implement this.
  return true;
}

void FocusController::SetFocusedWindowImpl(
    FocusControllerChangeSource change_source,
    ServerWindow* window) {
  if (window && !CanBeFocused(window))
    return;
  ServerWindow* old = GetFocusedWindow();

  DCHECK(!window || window->IsDrawn());

  // TODO(sad): Activate the closest activatable ancestor window.
  if (window)
    drawn_tracker_.reset(new ServerWindowDrawnTracker(window, this));
  else
    drawn_tracker_.reset();

  FOR_EACH_OBSERVER(FocusControllerObserver, observers_,
                    OnFocusChanged(change_source, old, window));
}

void FocusController::OnDrawnStateChanged(ServerWindow* ancestor,
                                          ServerWindow* window,
                                          bool is_drawn) {
  DCHECK(!is_drawn);  // We only observe when drawn.
  SetFocusedWindowImpl(FocusControllerChangeSource::DRAWN_STATE_CHANGED,
                       ancestor);
}

}  // namespace ws
}  // namespace mus
