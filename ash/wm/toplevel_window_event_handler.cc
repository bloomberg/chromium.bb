// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/toplevel_window_event_handler.h"

#include "ash/shell.h"
#include "ash/wm/property_util.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/window_properties.h"
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
#include "ui/base/events/event_utils.h"
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
  virtual void OnWindowHierarchyChanging(
      const HierarchyChangeParams& params) OVERRIDE;
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

 private:
  void AddHandlers(aura::Window* container);
  void RemoveHandlers();

  ToplevelWindowEventHandler* handler_;
  scoped_ptr<WindowResizer> resizer_;

  // If not NULL, this is an additional container that the dragged window has
  // moved to which ScopedWindowResizer has temporarily added observers on.
  aura::Window* target_container_;

  DISALLOW_COPY_AND_ASSIGN(ScopedWindowResizer);
};

ToplevelWindowEventHandler::ScopedWindowResizer::ScopedWindowResizer(
    ToplevelWindowEventHandler* handler,
    WindowResizer* resizer)
    : handler_(handler),
      resizer_(resizer),
      target_container_(NULL) {
  if (resizer_.get())
    resizer_->GetTarget()->AddObserver(this);
}

ToplevelWindowEventHandler::ScopedWindowResizer::~ScopedWindowResizer() {
  RemoveHandlers();
  if (resizer_.get())
    resizer_->GetTarget()->RemoveObserver(this);
}

void ToplevelWindowEventHandler::ScopedWindowResizer::OnWindowHierarchyChanging(
    const HierarchyChangeParams& params) {
  if (params.receiver != resizer_->GetTarget())
    return;

  if (params.receiver->GetProperty(internal::kContinueDragAfterReparent)) {
    params.receiver->SetProperty(internal::kContinueDragAfterReparent, false);
    AddHandlers(params.new_parent);
  } else {
    handler_->CompleteDrag(DRAG_COMPLETE, 0);
  }
}

void ToplevelWindowEventHandler::ScopedWindowResizer::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (!wm::IsWindowNormal(window))
    handler_->CompleteDrag(DRAG_COMPLETE, 0);
}

void ToplevelWindowEventHandler::ScopedWindowResizer::OnWindowDestroying(
    aura::Window* window) {
  DCHECK(resizer_.get());
  DCHECK_EQ(resizer_->GetTarget(), window);
  handler_->ResizerWindowDestroyed();
}

void ToplevelWindowEventHandler::ScopedWindowResizer::AddHandlers(
    aura::Window* container) {
  RemoveHandlers();
  if (!handler_->owner()->Contains(container)) {
    container->AddPreTargetHandler(handler_);
    container->AddPostTargetHandler(handler_);
    target_container_ = container;
  }
}

void ToplevelWindowEventHandler::ScopedWindowResizer::RemoveHandlers() {
  if (target_container_) {
    target_container_->RemovePreTargetHandler(handler_);
    target_container_->RemovePostTargetHandler(handler_);
    target_container_ = NULL;
  }
}


// ToplevelWindowEventHandler --------------------------------------------------

ToplevelWindowEventHandler::ToplevelWindowEventHandler(aura::Window* owner)
    : owner_(owner),
      in_move_loop_(false),
      move_cancelled_(false),
      in_gesture_drag_(false),
      destroyed_(NULL) {
  aura::client::SetWindowMoveClient(owner, this);
  Shell::GetInstance()->display_controller()->AddObserver(this);
  owner->AddPreTargetHandler(this);
  owner->AddPostTargetHandler(this);
}

ToplevelWindowEventHandler::~ToplevelWindowEventHandler() {
  Shell::GetInstance()->display_controller()->RemoveObserver(this);
  if (destroyed_)
    *destroyed_ = true;
}

void ToplevelWindowEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  if (window_resizer_.get() && event->type() == ui::ET_KEY_PRESSED &&
      event->key_code() == ui::VKEY_ESCAPE) {
    CompleteDrag(DRAG_REVERT, event->flags());
  }
}

void ToplevelWindowEventHandler::OnMouseEvent(
    ui::MouseEvent* event) {
  if ((event->flags() &
      (ui::EF_MIDDLE_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON)) != 0)
    return;

  if (in_gesture_drag_)
    return;

  aura::Window* target = static_cast<aura::Window*>(event->target());
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      HandleMousePressed(target, event);
      break;
    case ui::ET_MOUSE_DRAGGED:
      HandleDrag(target, event);
      break;
    case ui::ET_MOUSE_CAPTURE_CHANGED:
    case ui::ET_MOUSE_RELEASED:
      HandleMouseReleased(target, event);
      break;
    case ui::ET_MOUSE_MOVED:
      HandleMouseMoved(target, event);
      break;
    case ui::ET_MOUSE_EXITED:
      HandleMouseExited(target, event);
      break;
    default:
      break;
  }
}

void ToplevelWindowEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (!target->delegate())
    return;

  if (in_move_loop_ && !in_gesture_drag_)
    return;

  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN: {
      if (in_gesture_drag_)
        return;
      int component =
          target->delegate()->GetNonClientComponent(event->location());
      if (WindowResizer::GetBoundsChangeForWindowComponent(component) == 0) {
        window_resizer_.reset();
        return;
      }
      in_gesture_drag_ = true;
      pre_drag_window_bounds_ = target->bounds();
      gfx::Point location_in_parent(
          ConvertPointToParent(target, event->location()));
      CreateScopedWindowResizer(target, location_in_parent, component);
      break;
    }
    case ui::ET_GESTURE_SCROLL_UPDATE: {
      if (!in_gesture_drag_)
        return;
      if (window_resizer_.get() &&
          window_resizer_->resizer()->GetTarget() != target) {
        return;
      }
      HandleDrag(target, event);
      break;
    }
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START: {
      if (!in_gesture_drag_)
        return;
      if (window_resizer_.get() &&
          window_resizer_->resizer()->GetTarget() != target) {
        return;
      }

      CompleteDrag(DRAG_COMPLETE, event->flags());
      if (in_move_loop_) {
        quit_closure_.Run();
        in_move_loop_ = false;
      }
      in_gesture_drag_ = false;

      if (event->type() == ui::ET_GESTURE_SCROLL_END) {
        event->StopPropagation();
        return;
      }

      int component =
          target->delegate()->GetNonClientComponent(event->location());
      if (WindowResizer::GetBoundsChangeForWindowComponent(component) == 0)
        return;
      if (!wm::IsWindowNormal(target))
        return;

      if (fabs(event->details().velocity_y()) >
          kMinVertVelocityForWindowMinimize) {
        // Minimize/maximize.
        if (event->details().velocity_y() > 0 &&
            wm::CanMinimizeWindow(target)) {
          wm::MinimizeWindow(target);
          SetWindowAlwaysRestoresToRestoreBounds(target, true);
          SetRestoreBoundsInParent(target, pre_drag_window_bounds_);
        } else if (wm::CanMaximizeWindow(target)) {
          SetRestoreBoundsInParent(target, pre_drag_window_bounds_);
          wm::MaximizeWindow(target);
        }
      } else if (wm::CanSnapWindow(target) &&
                 fabs(event->details().velocity_x()) >
                 kMinHorizVelocityForWindowSwipe) {
        // Snap left/right.
        ui::ScopedLayerAnimationSettings scoped_setter(
            target->layer()->GetAnimator());
        scoped_setter.SetPreemptionStrategy(
            ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
        internal::SnapSizer::SnapWindow(target,
            event->details().velocity_x() < 0 ?
            internal::SnapSizer::LEFT_EDGE : internal::SnapSizer::RIGHT_EDGE);
      }
      break;
    }
    default:
      return;
  }

  event->StopPropagation();
}

aura::client::WindowMoveResult ToplevelWindowEventHandler::RunMoveLoop(
    aura::Window* source,
    const gfx::Vector2d& drag_offset,
    aura::client::WindowMoveSource move_source) {
  DCHECK(!in_move_loop_);  // Can only handle one nested loop at a time.
  in_move_loop_ = true;
  move_cancelled_ = false;
  aura::RootWindow* root_window = source->GetRootWindow();
  DCHECK(root_window);
  gfx::Point drag_location;
  if (move_source == aura::client::WINDOW_MOVE_SOURCE_TOUCH &&
      aura::Env::GetInstance()->is_touch_down()) {
    in_gesture_drag_ = true;
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
  bool destroyed = false;
  destroyed_ = &destroyed;
#if !defined(OS_MACOSX)
  MessageLoopForUI* loop = MessageLoopForUI::current();
  MessageLoop::ScopedNestableTaskAllower allow_nested(loop);
  base::RunLoop run_loop(aura::Env::GetInstance()->GetDispatcher());
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
#endif  // !defined(OS_MACOSX)
  if (destroyed)
    return aura::client::MOVE_CANCELED;
  destroyed_ = NULL;
  in_gesture_drag_ = in_move_loop_ = false;
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

void ToplevelWindowEventHandler::CreateScopedWindowResizer(
    aura::Window* window,
    const gfx::Point& point_in_parent,
    int window_component) {
  window_resizer_.reset();
  WindowResizer* resizer =
      CreateWindowResizer(window, point_in_parent, window_component).release();
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

void ToplevelWindowEventHandler::HandleMousePressed(
    aura::Window* target,
    ui::MouseEvent* event) {
  // Move/size operations are initiated post-target handling to give the target
  // an opportunity to cancel this default behavior by returning ER_HANDLED.
  if (ui::EventCanceledDefaultHandling(*event))
    return;

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
  if (WindowResizer::GetBoundsChangeForWindowComponent(component) != 0)
    event->StopPropagation();
}

void ToplevelWindowEventHandler::HandleMouseReleased(
    aura::Window* target,
    ui::MouseEvent* event) {
  if (event->phase() != ui::EP_PRETARGET)
    return;

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
    event->StopPropagation();
  }
}

void ToplevelWindowEventHandler::HandleDrag(
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
    return;

  if (!window_resizer_.get())
    return;
  window_resizer_->resizer()->Drag(
      ConvertPointToParent(target, event->location()), event->flags());
  event->StopPropagation();
}

void ToplevelWindowEventHandler::HandleMouseMoved(
    aura::Window* target,
    ui::LocatedEvent* event) {
  // Shadow effects are applied after target handling. Note that we don't
  // respect ER_HANDLED here right now since we have not had a reason to allow
  // the target to cancel shadow rendering.
  if (event->phase() != ui::EP_POSTTARGET)
    return;

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
}

void ToplevelWindowEventHandler::HandleMouseExited(
    aura::Window* target,
    ui::LocatedEvent* event) {
  // Shadow effects are applied after target handling. Note that we don't
  // respect ER_HANDLED here right now since we have not had a reason to allow
  // the target to cancel shadow rendering.
  if (event->phase() != ui::EP_POSTTARGET)
    return;

  internal::ResizeShadowController* controller =
      Shell::GetInstance()->resize_shadow_controller();
  if (controller)
    controller->HideShadow(target);
}

void ToplevelWindowEventHandler::ResizerWindowDestroyed() {
  // We explicitly don't invoke RevertDrag() since that may do things to window.
  // Instead we destroy the resizer.
  window_resizer_.reset();

  // End the move loop. This does nothing if we're not in a move loop.
  EndMoveLoop();
}

}  // namespace ash
