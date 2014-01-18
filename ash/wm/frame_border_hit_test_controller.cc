// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/frame_border_hit_test_controller.h"

#include "ash/ash_constants.h"
#include "ash/wm/header_painter.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_observer.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/hit_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"

namespace ash {

namespace {

// To allow easy resize, the resize handles should slightly overlap the content
// area of non-maximized and non-fullscreen windows.
class ResizeHandleWindowTargeter : public wm::WindowStateObserver,
                                   public aura::WindowObserver,
                                   public aura::WindowTargeter {
 public:
  explicit ResizeHandleWindowTargeter(aura::Window* window)
      : window_(window) {
    wm::WindowState* window_state = wm::GetWindowState(window_);
    OnWindowShowTypeChanged(window_state, wm::SHOW_TYPE_DEFAULT);
    window_state->AddObserver(this);
    window_->AddObserver(this);
  }

  virtual ~ResizeHandleWindowTargeter() {
    if (window_) {
      window_->RemoveObserver(this);
      wm::GetWindowState(window_)->RemoveObserver(this);
    }
  }

 private:
  // wm::WindowStateObserver:
  virtual void OnWindowShowTypeChanged(wm::WindowState* window_state,
                                       wm::WindowShowType old_type) OVERRIDE {
    if (window_state->IsMaximizedOrFullscreen()) {
      frame_border_inset_ = gfx::Insets();
    } else {
      frame_border_inset_ = gfx::Insets(kResizeInsideBoundsSize,
                                        kResizeInsideBoundsSize,
                                        kResizeInsideBoundsSize,
                                        kResizeInsideBoundsSize);
    }
  }

  // aura::WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    CHECK_EQ(window_, window);
    wm::GetWindowState(window_)->RemoveObserver(this);
    window_ = NULL;
  }

  // aura::WindowTargeter:
  virtual ui::EventTarget* FindTargetForLocatedEvent(
      ui::EventTarget* root,
      ui::LocatedEvent* event) OVERRIDE {
    // If the event falls very close to the inside of the frame border, then
    // target the window itself, so that the window can be resized easily.
    aura::Window* window = static_cast<aura::Window*>(root);
    if (window == window_ && !frame_border_inset_.empty()) {
      gfx::Rect bounds = gfx::Rect(window_->bounds().size());
      bounds.Inset(frame_border_inset_);
      if (!bounds.Contains(event->location()))
        return window_;
    }
    return aura::WindowTargeter::FindTargetForLocatedEvent(root, event);
  }

  virtual bool SubtreeShouldBeExploredForEvent(
      ui::EventTarget* target,
      const ui::LocatedEvent& event) OVERRIDE {
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

  aura::Window* window_;
  gfx::Insets frame_border_inset_;

  DISALLOW_COPY_AND_ASSIGN(ResizeHandleWindowTargeter);
};

}  // namespace

FrameBorderHitTestController::FrameBorderHitTestController(views::Widget* frame)
    : frame_window_(frame->GetNativeWindow()) {
  gfx::Insets mouse_outer_insets(-kResizeOutsideBoundsSize,
                                 -kResizeOutsideBoundsSize,
                                 -kResizeOutsideBoundsSize,
                                 -kResizeOutsideBoundsSize);
  gfx::Insets touch_outer_insets = mouse_outer_insets.Scale(
      kResizeOutsideBoundsScaleForTouch);
  // Ensure we get resize cursors for a few pixels outside our bounds.
  frame_window_->SetHitTestBoundsOverrideOuter(mouse_outer_insets,
                                               touch_outer_insets);

  frame_window_->set_event_targeter(scoped_ptr<ui::EventTargeter>(
      new ResizeHandleWindowTargeter(frame_window_)));
}

FrameBorderHitTestController::~FrameBorderHitTestController() {
}

// static
int FrameBorderHitTestController::NonClientHitTest(
    views::NonClientFrameView* view,
    HeaderPainter* header_painter,
    const gfx::Point& point) {
  gfx::Rect expanded_bounds = view->bounds();
  int outside_bounds = kResizeOutsideBoundsSize;

  if (aura::Env::GetInstance()->is_touch_down())
    outside_bounds *= kResizeOutsideBoundsScaleForTouch;
  expanded_bounds.Inset(-outside_bounds, -outside_bounds);

  if (!expanded_bounds.Contains(point))
    return HTNOWHERE;

  // Check the frame first, as we allow a small area overlapping the contents
  // to be used for resize handles.
  views::Widget* frame = view->GetWidget();
  bool can_ever_resize = frame->widget_delegate()->CanResize();
  // Don't allow overlapping resize handles when the window is maximized or
  // fullscreen, as it can't be resized in those states.
  int resize_border =
      frame->IsMaximized() || frame->IsFullscreen() ? 0 :
      kResizeInsideBoundsSize;
  int frame_component = view->GetHTComponentForFrame(point,
                                                     resize_border,
                                                     resize_border,
                                                     kResizeAreaCornerSize,
                                                     kResizeAreaCornerSize,
                                                     can_ever_resize);
  if (frame_component != HTNOWHERE)
    return frame_component;

  int client_component = frame->client_view()->NonClientHitTest(point);
  if (client_component != HTNOWHERE)
    return client_component;

  return header_painter->NonClientHitTest(point);
}

}  // namespace ash
