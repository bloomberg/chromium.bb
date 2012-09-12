// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/toplevel_window_event_handler.h"

#include "ash/shell.h"
#include "ash/wm/default_window_resizer.h"
#include "ash/wm/property_util.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_functions.h"
#include "ui/base/gestures/gesture_recognizer.h"
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
  aura::Window::ConvertPointToTarget(window, window->parent(), &result);
  return result;
}

}  // namespace

// ScopedWindowResizer ---------------------------------------------------------

// Wraps a WindowResizer and installs an observer on its target window.  When
// the window is destroyed ResizerWindowDestroyed() is invoked back on the
// ToplevelWindowEventHandler to clean up.
class ToplevelWindowEventHandler::ScopedWindowResizer
    : public aura::WindowObserver {
 public:
  ScopedWindowResizer(ToplevelWindowEventHandler* handler,
                      WindowResizer* resizer);
  virtual ~ScopedWindowResizer();

  WindowResizer* resizer() { return resizer_.get(); }

  // WindowObserver overrides:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

 private:
  ToplevelWindowEventHandler* handler_;
  scoped_ptr<WindowResizer> resizer_;

  DISALLOW_COPY_AND_ASSIGN(ScopedWindowResizer);
};

ToplevelWindowEventHandler::ScopedWindowResizer::ScopedWindowResizer(
    ToplevelWindowEventHandler* handler,
    WindowResizer* resizer)
    : handler_(handler),
      resizer_(resizer) {
  if (resizer_.get())
    resizer_->GetTarget()->AddObserver(this);
}

ToplevelWindowEventHandler::ScopedWindowResizer::~ScopedWindowResizer() {
  if (resizer_.get())
    resizer_->GetTarget()->RemoveObserver(this);
}

void ToplevelWindowEventHandler::ScopedWindowResizer::OnWindowDestroying(
    aura::Window* window) {
  DCHECK(resizer_.get());
  DCHECK_EQ(resizer_->GetTarget(), window);
  handler_->ResizerWindowDestroyed();
}


// ToplevelWindowEventHandler --------------------------------------------------

ToplevelWindowEventHandler::ToplevelWindowEventHandler(aura::Window* owner)
    : in_move_loop_(false),
      move_cancelled_(false) {
  aura::client::SetWindowMoveClient(owner, this);
  Shell::GetInstance()->display_controller()->AddObserver(this);
  owner->AddPreTargetHandler(this);
  owner->AddPostTargetHandler(this);
}

ToplevelWindowEventHandler::~ToplevelWindowEventHandler() {
  Shell::GetInstance()->display_controller()->RemoveObserver(this);
}

ui::EventResult ToplevelWindowEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  if (window_resizer_.get() && event->type() == ui::ET_KEY_PRESSED &&
      event->key_code() == ui::VKEY_ESCAPE) {
    CompleteDrag(DRAG_REVERT, event->flags());
  }
  return ui::ER_UNHANDLED;
}

ui::EventResult ToplevelWindowEventHandler::OnMouseEvent(
    ui::MouseEvent* event) {
  if ((event->flags() &
      (ui::EF_MIDDLE_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON)) != 0)
    return ui::ER_UNHANDLED;

  aura::Window* target = static_cast<aura::Window*>(event->target());
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      return HandleMousePressed(target, event);
    case ui::ET_MOUSE_DRAGGED:
      return HandleDrag(target, event) ? ui::ER_CONSUMED : ui::ER_UNHANDLED;
    case ui::ET_MOUSE_CAPTURE_CHANGED:
    case ui::ET_MOUSE_RELEASED:
      return HandleMouseReleased(target, event);
    case ui::ET_MOUSE_MOVED:
      return HandleMouseMoved(target, event);
    case ui::ET_MOUSE_EXITED:
      return HandleMouseExited(target, event);
    default:
      break;
  }
  return ui::ER_UNHANDLED;
}

ui::EventResult ToplevelWindowEventHandler::OnScrollEvent(
    ui::ScrollEvent* event) {
  return ui::ER_UNHANDLED;
}

ui::TouchStatus ToplevelWindowEventHandler::OnTouchEvent(
    ui::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::EventResult ToplevelWindowEventHandler::OnGestureEvent(
    ui::GestureEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN: {
      int component =
          target->delegate()->GetNonClientComponent(event->location());
      if (WindowResizer::GetBoundsChangeForWindowComponent(component) == 0) {
        window_resizer_.reset();
        return ui::ER_UNHANDLED;
      }
      in_gesture_resize_ = true;
      gfx::Point location_in_parent(
          ConvertPointToParent(target, event->location()));
      CreateScopedWindowResizer(target, location_in_parent, component);
      break;
    }
    case ui::ET_GESTURE_SCROLL_UPDATE: {
      if (!in_gesture_resize_)
        return ui::ER_UNHANDLED;
      HandleDrag(target, event);
      break;
    }
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START: {
      ui::EventResult status = ui::ER_UNHANDLED;
      if (in_gesture_resize_) {
        // If the window was being resized, then just complete the resize.
        CompleteDrag(DRAG_COMPLETE, event->flags());
        if (in_move_loop_) {
          quit_closure_.Run();
          in_move_loop_ = false;
        }
        in_gesture_resize_ = false;
        status = ui::ER_CONSUMED;
      }

      if (event->type() == ui::ET_GESTURE_SCROLL_END)
        return status;

      int component =
          target->delegate()->GetNonClientComponent(event->location());
      if (WindowResizer::GetBoundsChangeForWindowComponent(component) == 0)
        return ui::ER_UNHANDLED;
      if (!wm::IsWindowNormal(target))
        return ui::ER_UNHANDLED;

      if (fabs(event->details().velocity_y()) >
          kMinVertVelocityForWindowMinimize) {
        // Minimize/maximize.
        target->SetProperty(aura::client::kShowStateKey,
            event->details().velocity_y() > 0 ? ui::SHOW_STATE_MINIMIZED :
                                      ui::SHOW_STATE_MAXIMIZED);
      } else if (fabs(event->details().velocity_x()) >
                 kMinHorizVelocityForWindowSwipe) {
        // Snap left/right.
        internal::SnapSizer sizer(target,
            gfx::Point(),
            event->details().velocity_x() < 0 ? internal::SnapSizer::LEFT_EDGE :
            internal::SnapSizer::RIGHT_EDGE);

        ui::ScopedLayerAnimationSettings scoped_setter(
            target->layer()->GetAnimator());
        scoped_setter.SetPreemptionStrategy(
            ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
        target->SetBounds(sizer.target_bounds());
      }
      break;
    }
    default:
      return ui::ER_UNHANDLED;
  }

  return ui::ER_CONSUMED;
}

aura::client::WindowMoveResult ToplevelWindowEventHandler::RunMoveLoop(
    aura::Window* source,
    const gfx::Point& drag_offset) {
  DCHECK(!in_move_loop_);  // Can only handle one nested loop at a time.
  in_move_loop_ = true;
  move_cancelled_ = false;
  aura::RootWindow* root_window = source->GetRootWindow();
  DCHECK(root_window);
  gfx::Point drag_location;
  if (aura::Env::GetInstance()->is_touch_down()) {
    in_gesture_resize_ = true;
    bool has_point = root_window->gesture_recognizer()->
        GetLastTouchPointForTarget(source, &drag_location);
    DCHECK(has_point);
  } else {
    drag_location = root_window->GetLastMouseLocationInRoot();
    aura::Window::ConvertPointToTarget(
        root_window, source->parent(), &drag_location);
  }
  CreateScopedWindowResizer(source, drag_location, HTCAPTION);
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client)
    cursor_client->SetCursor(ui::kCursorPointer);
#if !defined(OS_MACOSX)
  MessageLoopForUI* loop = MessageLoopForUI::current();
  MessageLoop::ScopedNestableTaskAllower allow_nested(loop);
  base::RunLoop run_loop(aura::Env::GetInstance()->GetDispatcher());
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
#endif  // !defined(OS_MACOSX)
  in_gesture_resize_ = in_move_loop_ = false;
  return move_cancelled_ ? aura::client::MOVE_CANCELED :
      aura::client::MOVE_SUCCESSFUL;
}

void ToplevelWindowEventHandler::EndMoveLoop() {
  if (!in_move_loop_)
    return;

  in_move_loop_ = false;
  if (window_resizer_.get()) {
    window_resizer_->resizer()->RevertDrag();
    window_resizer_.reset();
  }
  quit_closure_.Run();
}

void ToplevelWindowEventHandler::OnDisplayConfigurationChanging() {
  if (in_move_loop_) {
    move_cancelled_ = true;
    EndMoveLoop();
  } else if (window_resizer_.get()) {
    window_resizer_->resizer()->RevertDrag();
    window_resizer_.reset();
  }
}

// static
WindowResizer* ToplevelWindowEventHandler::CreateWindowResizer(
    aura::Window* window,
    const gfx::Point& point_in_parent,
    int window_component) {
  if (!wm::IsWindowNormal(window))
    return NULL;  // Don't allow resizing/dragging maximized/fullscreen windows.
  return DefaultWindowResizer::Create(
      window, point_in_parent, window_component);
}


void ToplevelWindowEventHandler::CreateScopedWindowResizer(
    aura::Window* window,
    const gfx::Point& point_in_parent,
    int window_component) {
  window_resizer_.reset();
  WindowResizer* resizer =
      CreateWindowResizer(window, point_in_parent, window_component);
  if (resizer)
    window_resizer_.reset(new ScopedWindowResizer(this, resizer));
}

void ToplevelWindowEventHandler::CompleteDrag(DragCompletionStatus status,
                                              int event_flags) {
  scoped_ptr<ScopedWindowResizer> resizer(window_resizer_.release());
  if (resizer.get()) {
    if (status == DRAG_COMPLETE)
      resizer->resizer()->CompleteDrag(event_flags);
    else
      resizer->resizer()->RevertDrag();
  }
}

ui::EventResult ToplevelWindowEventHandler::HandleMousePressed(
    aura::Window* target,
    ui::MouseEvent* event) {
  // Move/size operations are initiated post-target handling to give the target
  // an opportunity to cancel this default behavior by returning ER_HANDLED.
  if (ui::EventCanceledDefaultHandling(*event))
    return ui::ER_UNHANDLED;

  // We also update the current window component here because for the
  // mouse-drag-release-press case, where the mouse is released and
  // pressed without mouse move event.
  int component =
      target->delegate()->GetNonClientComponent(event->location());
  if ((event->flags() &
        (ui::EF_IS_DOUBLE_CLICK | ui::EF_IS_TRIPLE_CLICK)) == 0 &&
      WindowResizer::GetBoundsChangeForWindowComponent(component)) {
    gfx::Point location_in_parent(
        ConvertPointToParent(target, event->location()));
    CreateScopedWindowResizer(target, location_in_parent, component);
  } else {
    window_resizer_.reset();
  }
  return WindowResizer::GetBoundsChangeForWindowComponent(component) != 0 ?
      ui::ER_CONSUMED : ui::ER_UNHANDLED;
}

ui::EventResult ToplevelWindowEventHandler::HandleMouseReleased(
    aura::Window* target,
    ui::MouseEvent* event) {
  if (event->phase() != ui::EP_PRETARGET)
    return ui::ER_UNHANDLED;

  CompleteDrag(event->type() == ui::ET_MOUSE_RELEASED ?
                    DRAG_COMPLETE : DRAG_REVERT,
                event->flags());
  if (in_move_loop_) {
    quit_closure_.Run();
    in_move_loop_ = false;
  }
  // Completing the drag may result in hiding the window. If this happens
  // return true so no other handlers/observers see the event. Otherwise
  // they see the event on a hidden window.
  if (event->type() == ui::ET_MOUSE_CAPTURE_CHANGED &&
      !target->IsVisible()) {
    return ui::ER_CONSUMED;
  }
  return ui::ER_UNHANDLED;
}

ui::EventResult ToplevelWindowEventHandler::HandleDrag(
    aura::Window* target,
    ui::LocatedEvent* event) {
  // This function only be triggered to move window
  // by mouse drag or touch move event.
  DCHECK(event->type() == ui::ET_MOUSE_DRAGGED ||
         event->type() == ui::ET_TOUCH_MOVED ||
         event->type() == ui::ET_GESTURE_SCROLL_UPDATE);

  // Drag actions are performed pre-target handling to prevent spurious mouse
  // moves from the move/size operation from being sent to the target.
  if (event->phase() != ui::EP_PRETARGET)
    return ui::ER_UNHANDLED;

  if (!window_resizer_.get())
    return ui::ER_UNHANDLED;
  window_resizer_->resizer()->Drag(
      ConvertPointToParent(target, event->location()), event->flags());
  return ui::ER_CONSUMED;
}

ui::EventResult ToplevelWindowEventHandler::HandleMouseMoved(
    aura::Window* target,
    ui::LocatedEvent* event) {
  // Shadow effects are applied after target handling. Note that we don't
  // respect ER_HANDLED here right now since we have not had a reason to allow
  // the target to cancel shadow rendering.
  if (event->phase() != ui::EP_POSTTARGET)
    return ui::ER_UNHANDLED;

  // TODO(jamescook): Move the resize cursor update code into here from
  // CompoundEventFilter?
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
  return ui::ER_UNHANDLED;
}

ui::EventResult ToplevelWindowEventHandler::HandleMouseExited(
    aura::Window* target,
    ui::LocatedEvent* event) {
  // Shadow effects are applied after target handling. Note that we don't
  // respect ER_HANDLED here right now since we have not had a reason to allow
  // the target to cancel shadow rendering.
  if (event->phase() != ui::EP_POSTTARGET)
    return ui::ER_UNHANDLED;

  internal::ResizeShadowController* controller =
      Shell::GetInstance()->resize_shadow_controller();
  if (controller)
    controller->HideShadow(target);
  return ui::ER_UNHANDLED;
}

void ToplevelWindowEventHandler::ResizerWindowDestroyed() {
  // We explicitly don't invoke RevertDrag() since that may do things to window.
  // Instead we destroy the resizer.
  window_resizer_.reset();

  // End the move loop. This does nothing if we're not in a move loop.
  EndMoveLoop();
}

}  // namespace ash
