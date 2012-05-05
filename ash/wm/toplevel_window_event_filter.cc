// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/toplevel_window_event_filter.h"

#include "ash/shell.h"
#include "ash/wm/default_window_resizer.h"
#include "ash/wm/property_util.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "base/message_loop.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"

namespace {
const double kMinHorizVelocityForWindowSwipe = 1100;
const double kMinVertVelocityForWindowMinimize = 1000;
}

namespace ash {

namespace {

gfx::Point ConvertPointToParent(aura::Window* window,
                                const gfx::Point& point) {
  gfx::Point result(point);
  aura::Window::ConvertPointToWindow(window, window->parent(), &result);
  return result;
}

}

ToplevelWindowEventFilter::ToplevelWindowEventFilter(aura::Window* owner)
    : in_move_loop_(false),
      in_gesture_resize_(false),
      grid_size_(0) {
  aura::client::SetWindowMoveClient(owner, this);
}

ToplevelWindowEventFilter::~ToplevelWindowEventFilter() {
}

bool ToplevelWindowEventFilter::PreHandleKeyEvent(aura::Window* target,
                                                  aura::KeyEvent* event) {
  if (window_resizer_.get() && event->type() == ui::ET_KEY_PRESSED &&
      event->key_code() == ui::VKEY_ESCAPE) {
    CompleteDrag(DRAG_REVERT, event->flags());
  }
  return false;
}

bool ToplevelWindowEventFilter::PreHandleMouseEvent(aura::Window* target,
                                                    aura::MouseEvent* event) {
  if ((event->flags() &
      (ui::EF_MIDDLE_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON)) != 0)
    return false;

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED: {
      // We also update the current window component here because for the
      // mouse-drag-release-press case, where the mouse is released and
      // pressed without mouse move event.
      int component =
          target->delegate()->GetNonClientComponent(event->location());
      if ((event->flags() &
           (ui::EF_IS_DOUBLE_CLICK | ui::EF_IS_TRIPLE_CLICK)) == 0 &&
          WindowResizer::GetBoundsChangeForWindowComponent(component)) {
        gfx::Point parent_location(
            ConvertPointToParent(target, event->location()));
        window_resizer_.reset(
            CreateWindowResizer(target, parent_location, component));
      } else {
        window_resizer_.reset();
      }
      if (component == HTCAPTION && event->flags() & ui::EF_IS_DOUBLE_CLICK)
        ToggleMaximizedState(target);
      return WindowResizer::GetBoundsChangeForWindowComponent(component) != 0;
    }
    case ui::ET_MOUSE_DRAGGED:
      return HandleDrag(target, event);
    case ui::ET_MOUSE_CAPTURE_CHANGED:
    case ui::ET_MOUSE_RELEASED:
      CompleteDrag(event->type() == ui::ET_MOUSE_RELEASED ?
                       DRAG_COMPLETE : DRAG_REVERT,
                   event->flags());
      if (in_move_loop_) {
        MessageLoop::current()->Quit();
        in_move_loop_ = false;
      }
      // Completing the drag may result in hiding the window. If this happens
      // return true so no other filters/observers see the event. Otherwise they
      // see the event on a hidden window.
      if (event->type() == ui::ET_MOUSE_CAPTURE_CHANGED &&
          !target->IsVisible()) {
        return true;
      }
      break;
    case ui::ET_MOUSE_MOVED:
      return HandleMouseMoved(target, event);
    case ui::ET_MOUSE_EXITED:
      return HandleMouseExited(target, event);
    default:
      break;
  }
  return false;
}

ui::TouchStatus ToplevelWindowEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus ToplevelWindowEventFilter::PreHandleGestureEvent(
    aura::Window* target,
    aura::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN: {
      int component =
          target->delegate()->GetNonClientComponent(event->location());
      if (WindowResizer::GetBoundsChangeForWindowComponent(component) == 0) {
        window_resizer_.reset();
        return ui::GESTURE_STATUS_UNKNOWN;
      }
      in_gesture_resize_ = true;
      gfx::Point parent_location(
          ConvertPointToParent(target, event->location()));
      window_resizer_.reset(
          CreateWindowResizer(target, parent_location, component));
      break;
    }
    case ui::ET_GESTURE_SCROLL_UPDATE: {
      if (!in_gesture_resize_)
        return ui::GESTURE_STATUS_UNKNOWN;
      HandleDrag(target, event);
      break;
    }
    case ui::ET_GESTURE_SCROLL_END: {
      if (!in_gesture_resize_)
        return ui::GESTURE_STATUS_UNKNOWN;
      DefaultWindowResizer* default_resizer =
          static_cast<DefaultWindowResizer*>(window_resizer_.get());
      bool changed_size =
          default_resizer ?  default_resizer->changed_size() : false;
      aura::Window* window =
          default_resizer ?  default_resizer->target_window() : NULL;
      bool drag_done = false;

      if (window && !changed_size) {
        if (fabs(event->delta_y()) > kMinVertVelocityForWindowMinimize) {
          // Minimize/maximize.
          window->SetProperty(aura::client::kShowStateKey,
              event->delta_y() > 0 ? ui::SHOW_STATE_MINIMIZED :
                                     ui::SHOW_STATE_MAXIMIZED);
          drag_done = true;
        } else if (fabs(event->delta_x()) > kMinHorizVelocityForWindowSwipe) {
          // Snap left/right.
          internal::SnapSizer sizer(window,
              gfx::Point(),
              event->delta_x() < 0 ? internal::SnapSizer::LEFT_EDGE :
              internal::SnapSizer::RIGHT_EDGE,
              Shell::GetInstance()->GetGridSize());

          ui::ScopedLayerAnimationSettings scoped_setter(
              window->layer()->GetAnimator());
          scoped_setter.SetPreemptionStrategy(
              ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
          window->SetBounds(sizer.target_bounds());
          drag_done = true;
        }
      }

      if (!drag_done)
        CompleteDrag(DRAG_COMPLETE, event->flags());
      in_gesture_resize_ = false;
      break;
    }
    default:
      return ui::GESTURE_STATUS_UNKNOWN;
  }

  return ui::GESTURE_STATUS_CONSUMED;
}

void ToplevelWindowEventFilter::RunMoveLoop(aura::Window* source) {
  DCHECK(!in_move_loop_);  // Can only handle one nested loop at a time.
  in_move_loop_ = true;
  gfx::Point parent_mouse_location(gfx::Screen::GetCursorScreenPoint());
  aura::RootWindow* root_window = source->GetRootWindow();
  DCHECK(root_window);
  aura::Window::ConvertPointToWindow(
      root_window, source->parent(), &parent_mouse_location);
  window_resizer_.reset(
      CreateWindowResizer(source, parent_mouse_location, HTCAPTION));
  source->GetRootWindow()->SetCursor(ui::kCursorPointer);
#if !defined(OS_MACOSX)
  MessageLoopForUI* loop = MessageLoopForUI::current();
  MessageLoop::ScopedNestableTaskAllower allow_nested(loop);
  loop->RunWithDispatcher(aura::Env::GetInstance()->GetDispatcher());
#endif  // !defined(OS_MACOSX)
  in_move_loop_ = false;
}

void ToplevelWindowEventFilter::EndMoveLoop() {
  if (!in_move_loop_)
    return;

  in_move_loop_ = false;
  if (window_resizer_.get()) {
    window_resizer_->RevertDrag();
    window_resizer_.reset();
  }
  MessageLoopForUI::current()->Quit();
}

// static
WindowResizer* ToplevelWindowEventFilter::CreateWindowResizer(
    aura::Window* window,
    const gfx::Point& point,
    int window_component) {
  if (!wm::IsWindowNormal(window))
    return NULL;  // Don't allow resizing/dragging maximized/fullscreen windows.
  return DefaultWindowResizer::Create(window, point, window_component);
}

void ToplevelWindowEventFilter::CompleteDrag(DragCompletionStatus status,
                                             int event_flags) {
  scoped_ptr<WindowResizer> resizer(window_resizer_.release());
  if (resizer.get()) {
    if (status == DRAG_COMPLETE)
      resizer->CompleteDrag(event_flags);
    else
      resizer->RevertDrag();
  }
}

bool ToplevelWindowEventFilter::HandleDrag(aura::Window* target,
                                           aura::LocatedEvent* event) {
  // This function only be triggered to move window
  // by mouse drag or touch move event.
  DCHECK(event->type() == ui::ET_MOUSE_DRAGGED ||
         event->type() == ui::ET_TOUCH_MOVED ||
         event->type() == ui::ET_GESTURE_SCROLL_UPDATE);

  if (!window_resizer_.get())
    return false;
  window_resizer_->Drag(ConvertPointToParent(target, event->location()),
                        event->flags());
  return true;
}

bool ToplevelWindowEventFilter::HandleMouseMoved(aura::Window* target,
                                                 aura::LocatedEvent* event) {
  // TODO(jamescook): Move the resize cursor update code into here from
  // RootWindowEventFilter?
  internal::ResizeShadowController* controller =
      Shell::GetInstance()->resize_shadow_controller();
  if (controller) {
    if (event->flags() & ui::EF_IS_NON_CLIENT) {
      int component =
          target->delegate()->GetNonClientComponent(event->location());
        controller->ShowShadow(target, component);
    } else {
      controller->HideShadow(target);
    }
  }
  return false;
}

bool ToplevelWindowEventFilter::HandleMouseExited(aura::Window* target,
                                                  aura::LocatedEvent* event) {
  internal::ResizeShadowController* controller =
      Shell::GetInstance()->resize_shadow_controller();
  if (controller)
    controller->HideShadow(target);
  return false;
}

}  // namespace ash
