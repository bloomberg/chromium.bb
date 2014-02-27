// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/resize_handle_window_targeter.h"

#include "ash/ash_constants.h"
#include "ash/wm/immersive_fullscreen_controller.h"
#include "ash/wm/window_state.h"
#include "ui/aura/window.h"

namespace ash {

ResizeHandleWindowTargeter::ResizeHandleWindowTargeter(
    aura::Window* window,
    ImmersiveFullscreenController* controller)
    : window_(window),
      immersive_controller_(controller) {
  wm::WindowState* window_state = wm::GetWindowState(window_);
  OnPostWindowStateTypeChange(window_state, wm::WINDOW_STATE_TYPE_DEFAULT);
  window_state->AddObserver(this);
  window_->AddObserver(this);
}

ResizeHandleWindowTargeter::~ResizeHandleWindowTargeter() {
  if (window_) {
    window_->RemoveObserver(this);
    wm::GetWindowState(window_)->RemoveObserver(this);
  }
}

void ResizeHandleWindowTargeter::OnPostWindowStateTypeChange(
    wm::WindowState* window_state,
    wm::WindowStateType old_type) {
  if (window_state->IsMaximizedOrFullscreen()) {
    frame_border_inset_ = gfx::Insets();
  } else {
    frame_border_inset_ = gfx::Insets(kResizeInsideBoundsSize,
                                      kResizeInsideBoundsSize,
                                      kResizeInsideBoundsSize,
                                      kResizeInsideBoundsSize);
  }
}

void ResizeHandleWindowTargeter::OnWindowDestroying(aura::Window* window) {
  CHECK_EQ(window_, window);
  wm::GetWindowState(window_)->RemoveObserver(this);
  window_ = NULL;
}

ui::EventTarget* ResizeHandleWindowTargeter::FindTargetForLocatedEvent(
    ui::EventTarget* root,
    ui::LocatedEvent* event) {
  aura::Window* window = static_cast<aura::Window*>(root);
  if (window == window_) {
    gfx::Insets insets;
    if (immersive_controller_ && immersive_controller_->IsEnabled() &&
        !immersive_controller_->IsRevealed() &&
        event->IsTouchEvent()) {
      // If the window is in immersive fullscreen, and top-of-window views are
      // not revealed, then touch events towards the top of the window
      // should not reach the child window so that touch gestures can be used to
      // reveal the top-of-windows views. This is needed because the child
      // window may consume touch events and prevent touch-scroll gesture from
      // being generated.
      insets = gfx::Insets(kImmersiveFullscreenTopEdgeInset, 0, 0, 0);
    } else {
      // If the event falls very close to the inside of the frame border, then
      // target the window itself, so that the window can be resized easily.
      insets = frame_border_inset_;
    }

    if (!insets.empty()) {
      gfx::Rect bounds = gfx::Rect(window_->bounds().size());
      bounds.Inset(insets);
      if (!bounds.Contains(event->location()))
        return window_;
    }
  }
  return aura::WindowTargeter::FindTargetForLocatedEvent(root, event);
}

bool ResizeHandleWindowTargeter::SubtreeShouldBeExploredForEvent(
    ui::EventTarget* target,
    const ui::LocatedEvent& event) {
  if (target == window_) {
    // Defer to the parent's targeter on whether |window_| should be able to
    // receive the event.
    ui::EventTarget* parent = target->GetParentTarget();
    if (parent) {
      ui::EventTargeter* targeter = parent->GetEventTargeter();
      if (targeter)
        return targeter->SubtreeShouldBeExploredForEvent(target, event);
    }
  }
  return aura::WindowTargeter::SubtreeShouldBeExploredForEvent(target, event);
}

}  // namespace ash
