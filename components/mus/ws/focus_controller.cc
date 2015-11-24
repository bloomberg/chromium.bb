// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/focus_controller.h"

#include "components/mus/ws/focus_controller_delegate.h"
#include "components/mus/ws/focus_controller_observer.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_drawn_tracker.h"

namespace mus {
namespace ws {

namespace {

ServerWindow* GetLeftmostNode(ServerWindow* window) {
  while (!window->children().empty())
    window = window->children()[0];
  return window;
}

}  // namespace

FocusController::FocusController(FocusControllerDelegate* delegate,
                                 ServerWindow* root)
    : delegate_(delegate),
      root_(root),
      focused_window_(nullptr),
      active_window_(nullptr) {
  DCHECK(delegate_);
  DCHECK(root_);
}

FocusController::~FocusController() {}

void FocusController::SetFocusedWindow(ServerWindow* window) {
  if (GetFocusedWindow() == window)
    return;

  SetFocusedWindowImpl(FocusControllerChangeSource::EXPLICIT, window);
}

ServerWindow* FocusController::GetFocusedWindow() {
  return focused_window_;
}

void FocusController::ActivateNextWindow() {
  ServerWindow* current = active_window_;
  if (!current)
    current = GetLeftmostNode(root_);
  ServerWindow* activate = GetNextActivatableWindow(current);
  SetActiveWindow(activate);
  // TODO(sad): This should move focus to a descendant window of |activate|.
}

void FocusController::AddObserver(FocusControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void FocusController::RemoveObserver(FocusControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FocusController::SetActiveWindow(ServerWindow* window) {
  DCHECK(!window || CanBeActivated(window));
  ServerWindow* old_active = active_window_;
  active_window_ = window;
  if (old_active != active_window_) {
    FOR_EACH_OBSERVER(FocusControllerObserver, observers_,
                      OnActivationChanged(old_active, active_window_));
  }
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
  return GetActivatableAncestorOf(window) != nullptr;
}

bool FocusController::CanBeActivated(ServerWindow* window) const {
  DCHECK(window);
  // The parent window must be allowed to have active children.
  if (!delegate_->CanHaveActiveChildren(window->parent()))
    return false;

  // TODO(sad): Implement this.
  return true;
}

ServerWindow* FocusController::GetNextActivatableWindow(
    ServerWindow* window) const {
  DCHECK(window);
  ServerWindow* start = window;
  while (window) {
    window = FindNextWindowInTree(window);
    if (window == start)
      break;
    if (window && CanBeActivated(window))
      return window;
  }
  return nullptr;
}

ServerWindow* FocusController::FindNextWindowInTree(
    ServerWindow* window) const {
  DCHECK(window);
  if (window == root_)
   return GetLeftmostNode(window);

  // Return the next sibling.
  ServerWindow* parent = window->parent();
  if (parent) {
    const ServerWindow::Windows& siblings = parent->children();
    ServerWindow::Windows::const_iterator iter =
        std::find(siblings.begin(), siblings.end(), window);
    DCHECK(iter != siblings.end());
    ++iter;
    if (iter != siblings.end())
      return GetLeftmostNode(*iter);
  }

  // All children and siblings have been explored. Next is the parent.
  return parent;
}

ServerWindow* FocusController::GetActivatableAncestorOf(
    ServerWindow* window) const {
  for (ServerWindow* w = window; w; w = w->parent()) {
    if (CanBeActivated(w))
      return w;
  }
  return nullptr;
}

void FocusController::SetFocusedWindowImpl(
    FocusControllerChangeSource change_source,
    ServerWindow* window) {
  if (window && !CanBeFocused(window))
    return;
  ServerWindow* old_focused = GetFocusedWindow();

  DCHECK(!window || window->IsDrawn());

  // Activate the closest activatable ancestor window.
  // TODO(sad): The window to activate doesn't necessarily have to be a direct
  // ancestor (e.g. could be a transient parent).
  SetActiveWindow(GetActivatableAncestorOf(window));

  FOR_EACH_OBSERVER(FocusControllerObserver, observers_,
                    OnFocusChanged(change_source, old_focused, window));

  focused_window_ = window;
  // We can currently use only a single ServerWindowDrawnTracker since focused
  // window is expected to be a direct descendant of the active window.
  if (focused_window_ && active_window_) {
    DCHECK(active_window_->Contains(focused_window_));
  }
  ServerWindow* track_window = focused_window_;
  if (!track_window)
    track_window = active_window_;
  if (track_window)
    drawn_tracker_.reset(new ServerWindowDrawnTracker(track_window, this));
  else
    drawn_tracker_.reset();
}

void FocusController::OnDrawnStateChanged(ServerWindow* ancestor,
                                          ServerWindow* window,
                                          bool is_drawn) {
  DCHECK(!is_drawn);  // We only observe when drawn.
  // TODO(sad): If |window| is |focused_window_|, then move focus to the next
  // focusable window in |active_window_|, if |active_window_| is still visible.
  // If |active_window_| is invisible, or if |window| is |active_window_|, then
  // activate the next window that can be activated.
  SetFocusedWindowImpl(FocusControllerChangeSource::DRAWN_STATE_CHANGED,
                       ancestor);
}

}  // namespace ws
}  // namespace mus
