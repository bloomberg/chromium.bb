// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/toplevel_window_event_handler.h"

#include "ash/shell.h"
#include "ash/wm/aura/wm_window_aura.h"
#include "ash/wm/common/window_resizer.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/common/window_state_observer.h"
#include "ash/wm/common/wm_event.h"
#include "ash/wm/common/wm_window_observer.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/screen.h"

namespace {
const double kMinHorizVelocityForWindowSwipe = 1100;
const double kMinVertVelocityForWindowMinimize = 1000;
}

namespace ash {

namespace {

// Returns whether |window| can be moved via a two finger drag given
// the hittest results of the two fingers.
bool CanStartTwoFingerMove(aura::Window* window,
                           int window_component1,
                           int window_component2) {
  // We allow moving a window via two fingers when the hittest components are
  // HTCLIENT. This is done so that a window can be dragged via two fingers when
  // the tab strip is full and hitting the caption area is difficult. We check
  // the window type and the state type so that we do not steal touches from the
  // web contents.
  if (!wm::GetWindowState(window)->IsNormalOrSnapped() ||
      window->type() != ui::wm::WINDOW_TYPE_NORMAL) {
    return false;
  }
  int component1_behavior =
      WindowResizer::GetBoundsChangeForWindowComponent(window_component1);
  int component2_behavior =
      WindowResizer::GetBoundsChangeForWindowComponent(window_component2);
  return (component1_behavior & WindowResizer::kBoundsChange_Resizes) == 0 &&
      (component2_behavior & WindowResizer::kBoundsChange_Resizes) == 0;
}

// Returns whether |window| can be moved or resized via one finger given
// |window_component|.
bool CanStartOneFingerDrag(int window_component) {
  return WindowResizer::GetBoundsChangeForWindowComponent(
      window_component) != 0;
}

gfx::Point ConvertPointToParent(aura::Window* window,
                                const gfx::Point& point) {
  gfx::Point result(point);
  aura::Window::ConvertPointToTarget(window, window->parent(), &result);
  return result;
}

// Returns the window component containing |event|'s location.
int GetWindowComponent(aura::Window* window, const ui::LocatedEvent& event) {
  return window->delegate()->GetNonClientComponent(event.location());
}

}  // namespace

// ScopedWindowResizer ---------------------------------------------------------

// Wraps a WindowResizer and installs an observer on its target window.  When
// the window is destroyed ResizerWindowDestroyed() is invoked back on the
// ToplevelWindowEventHandler to clean up.
class ToplevelWindowEventHandler::ScopedWindowResizer
    : public wm::WmWindowObserver,
      public wm::WindowStateObserver {
 public:
  ScopedWindowResizer(ToplevelWindowEventHandler* handler,
                      WindowResizer* resizer);
  ~ScopedWindowResizer() override;

  // Returns true if the drag moves the window and does not resize.
  bool IsMove() const;

  WindowResizer* resizer() { return resizer_.get(); }

  // WindowObserver overrides:
  void OnWindowDestroying(wm::WmWindow* window) override;

  // WindowStateObserver overrides:
  void OnPreWindowStateTypeChange(wm::WindowState* window_state,
                                  wm::WindowStateType type) override;

 private:
  ToplevelWindowEventHandler* handler_;
  std::unique_ptr<WindowResizer> resizer_;

  // Whether ScopedWindowResizer grabbed capture.
  bool grabbed_capture_;

  DISALLOW_COPY_AND_ASSIGN(ScopedWindowResizer);
};

ToplevelWindowEventHandler::ScopedWindowResizer::ScopedWindowResizer(
    ToplevelWindowEventHandler* handler,
    WindowResizer* resizer)
    : handler_(handler),
      resizer_(resizer),
      grabbed_capture_(false) {
  wm::WmWindow* target = resizer_->GetTarget();
  target->AddObserver(this);
  target->GetWindowState()->AddObserver(this);

  if (!target->HasCapture()) {
    grabbed_capture_ = true;
    target->SetCapture();
  }
}

ToplevelWindowEventHandler::ScopedWindowResizer::~ScopedWindowResizer() {
  wm::WmWindow* target = resizer_->GetTarget();
  target->RemoveObserver(this);
  target->GetWindowState()->RemoveObserver(this);
  if (grabbed_capture_)
    target->ReleaseCapture();
}

bool ToplevelWindowEventHandler::ScopedWindowResizer::IsMove() const {
  return resizer_->details().bounds_change ==
      WindowResizer::kBoundsChange_Repositions;
}

void
ToplevelWindowEventHandler::ScopedWindowResizer::OnPreWindowStateTypeChange(
    wm::WindowState* window_state,
    wm::WindowStateType old) {
  handler_->CompleteDrag(DRAG_COMPLETE);
}

void ToplevelWindowEventHandler::ScopedWindowResizer::OnWindowDestroying(
    wm::WmWindow* window) {
  DCHECK_EQ(resizer_->GetTarget(), window);
  handler_->ResizerWindowDestroyed();
}

// ToplevelWindowEventHandler --------------------------------------------------

ToplevelWindowEventHandler::ToplevelWindowEventHandler()
    : first_finger_hittest_(HTNOWHERE),
      in_move_loop_(false),
      in_gesture_drag_(false),
      drag_reverted_(false),
      destroyed_(NULL) {
  Shell::GetInstance()->window_tree_host_manager()->AddObserver(this);
}

ToplevelWindowEventHandler::~ToplevelWindowEventHandler() {
  Shell::GetInstance()->window_tree_host_manager()->RemoveObserver(this);
  if (destroyed_)
    *destroyed_ = true;
}

void ToplevelWindowEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  if (window_resizer_.get() && event->type() == ui::ET_KEY_PRESSED &&
      event->key_code() == ui::VKEY_ESCAPE) {
    CompleteDrag(DRAG_REVERT);
  }
}

void ToplevelWindowEventHandler::OnMouseEvent(
    ui::MouseEvent* event) {
  if (event->handled())
    return;
  if ((event->flags() &
      (ui::EF_MIDDLE_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON)) != 0)
    return;

  if (event->type() == ui::ET_MOUSE_CAPTURE_CHANGED) {
    // Capture is grabbed when both gesture and mouse drags start. Handle
    // capture loss regardless of which type of drag is in progress.
    HandleCaptureLost(event);
    return;
  }

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
  if (event->handled())
    return;
  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (!target->delegate())
    return;

  if (window_resizer_.get() && !in_gesture_drag_)
    return;

  if (window_resizer_.get() &&
      wm::WmWindowAura::GetAuraWindow(
          window_resizer_->resizer()->GetTarget()) != target) {
    return;
  }

  if (event->details().touch_points() > 2) {
    if (CompleteDrag(DRAG_COMPLETE))
      event->StopPropagation();
    return;
  }

  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN: {
      int component = GetWindowComponent(target, *event);
      if (!(WindowResizer::GetBoundsChangeForWindowComponent(component) &
            WindowResizer::kBoundsChange_Resizes))
        return;
      ResizeShadowController* controller =
          Shell::GetInstance()->resize_shadow_controller();
      if (controller)
        controller->ShowShadow(target, component);
      return;
    }
    case ui::ET_GESTURE_END: {
      ResizeShadowController* controller =
          Shell::GetInstance()->resize_shadow_controller();
      if (controller)
        controller->HideShadow(target);

      if (window_resizer_.get() &&
          (event->details().touch_points() == 1 ||
           !CanStartOneFingerDrag(first_finger_hittest_))) {
        CompleteDrag(DRAG_COMPLETE);
        event->StopPropagation();
      }
      return;
    }
    case ui::ET_GESTURE_BEGIN: {
      if (event->details().touch_points() == 1) {
        first_finger_hittest_ = GetWindowComponent(target, *event);
      } else if (window_resizer_.get()) {
        if (!window_resizer_->IsMove()) {
          // The transition from resizing with one finger to resizing with two
          // fingers causes unintended resizing because the location of
          // ET_GESTURE_SCROLL_UPDATE jumps from the position of the first
          // finger to the position in the middle of the two fingers. For this
          // reason two finger resizing is not supported.
          CompleteDrag(DRAG_COMPLETE);
          event->StopPropagation();
        }
      } else {
        int second_finger_hittest = GetWindowComponent(target, *event);
        if (CanStartTwoFingerMove(
                target, first_finger_hittest_, second_finger_hittest)) {
          gfx::Point location_in_parent =
              event->details().bounding_box().CenterPoint();
          AttemptToStartDrag(target, location_in_parent, HTCAPTION,
                             aura::client::WINDOW_MOVE_SOURCE_TOUCH);
          event->StopPropagation();
        }
      }
      return;
    }
    case ui::ET_GESTURE_SCROLL_BEGIN: {
      // The one finger drag is not started in ET_GESTURE_BEGIN to avoid the
      // window jumping upon initiating a two finger drag. When a one finger
      // drag is converted to a two finger drag, a jump occurs because the
      // location of the ET_GESTURE_SCROLL_UPDATE event switches from the single
      // finger's position to the position in the middle of the two fingers.
      if (window_resizer_.get())
        return;
      int component = GetWindowComponent(target, *event);
      if (!CanStartOneFingerDrag(component))
        return;
      gfx::Point location_in_parent(
          ConvertPointToParent(target, event->location()));
      AttemptToStartDrag(target, location_in_parent, component,
                         aura::client::WINDOW_MOVE_SOURCE_TOUCH);
      event->StopPropagation();
      return;
    }
    default:
      break;
  }

  if (!window_resizer_.get())
    return;

  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_UPDATE:
      HandleDrag(target, event);
      event->StopPropagation();
      return;
    case ui::ET_GESTURE_SCROLL_END:
      // We must complete the drag here instead of as a result of ET_GESTURE_END
      // because otherwise the drag will be reverted when EndMoveLoop() is
      // called.
      // TODO(pkotwicz): Pass drag completion status to
      // WindowMoveClient::EndMoveLoop().
      CompleteDrag(DRAG_COMPLETE);
      event->StopPropagation();
      return;
    case ui::ET_SCROLL_FLING_START:
      CompleteDrag(DRAG_COMPLETE);

      // TODO(pkotwicz): Fix tests which inadvertantly start flings and check
      // window_resizer_->IsMove() instead of the hittest component at |event|'s
      // location.
      if (GetWindowComponent(target, *event) != HTCAPTION ||
          !wm::GetWindowState(target)->IsNormalOrSnapped()) {
        return;
      }

      if (event->details().velocity_y() > kMinVertVelocityForWindowMinimize) {
        SetWindowStateTypeFromGesture(target, wm::WINDOW_STATE_TYPE_MINIMIZED);
      } else if (event->details().velocity_y() <
                 -kMinVertVelocityForWindowMinimize) {
        SetWindowStateTypeFromGesture(target, wm::WINDOW_STATE_TYPE_MAXIMIZED);
      } else if (event->details().velocity_x() >
                 kMinHorizVelocityForWindowSwipe) {
        SetWindowStateTypeFromGesture(target,
                                      wm::WINDOW_STATE_TYPE_RIGHT_SNAPPED);
      } else if (event->details().velocity_x() <
                 -kMinHorizVelocityForWindowSwipe) {
        SetWindowStateTypeFromGesture(target,
                                      wm::WINDOW_STATE_TYPE_LEFT_SNAPPED);
      }
      event->StopPropagation();
      return;
    case ui::ET_GESTURE_SWIPE:
      DCHECK_GT(event->details().touch_points(), 0);
      if (event->details().touch_points() == 1)
        return;
      if (!wm::GetWindowState(target)->IsNormalOrSnapped())
        return;

      CompleteDrag(DRAG_COMPLETE);

      if (event->details().swipe_down()) {
        SetWindowStateTypeFromGesture(target, wm::WINDOW_STATE_TYPE_MINIMIZED);
      } else if (event->details().swipe_up()) {
        SetWindowStateTypeFromGesture(target, wm::WINDOW_STATE_TYPE_MAXIMIZED);
      } else if (event->details().swipe_right()) {
        SetWindowStateTypeFromGesture(target,
                                      wm::WINDOW_STATE_TYPE_RIGHT_SNAPPED);
      } else {
        SetWindowStateTypeFromGesture(target,
                                      wm::WINDOW_STATE_TYPE_LEFT_SNAPPED);
      }
      event->StopPropagation();
      return;
    default:
      return;
  }
}

aura::client::WindowMoveResult ToplevelWindowEventHandler::RunMoveLoop(
    aura::Window* source,
    const gfx::Vector2d& drag_offset,
    aura::client::WindowMoveSource move_source) {
  DCHECK(!in_move_loop_);  // Can only handle one nested loop at a time.
  aura::Window* root_window = source->GetRootWindow();
  DCHECK(root_window);
  gfx::Point drag_location;
  if (move_source == aura::client::WINDOW_MOVE_SOURCE_TOUCH &&
      aura::Env::GetInstance()->is_touch_down()) {
    gfx::PointF drag_location_f;
    bool has_point = ui::GestureRecognizer::Get()->
        GetLastTouchPointForTarget(source, &drag_location_f);
    drag_location = gfx::ToFlooredPoint(drag_location_f);
    DCHECK(has_point);
  } else {
    drag_location =
        root_window->GetHost()->dispatcher()->GetLastMouseLocationInRoot();
    aura::Window::ConvertPointToTarget(
        root_window, source->parent(), &drag_location);
  }
  // Set the cursor before calling AttemptToStartDrag(), as that will
  // eventually call LockCursor() and prevent the cursor from changing.
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client)
    cursor_client->SetCursor(ui::kCursorPointer);
  if (!AttemptToStartDrag(source, drag_location, HTCAPTION, move_source))
    return aura::client::MOVE_CANCELED;

  in_move_loop_ = true;
  bool destroyed = false;
  destroyed_ = &destroyed;
  base::MessageLoop* loop = base::MessageLoop::current();
  base::MessageLoop::ScopedNestableTaskAllower allow_nested(loop);
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
  if (destroyed)
    return aura::client::MOVE_CANCELED;
  destroyed_ = NULL;
  in_move_loop_ = false;
  return drag_reverted_ ? aura::client::MOVE_CANCELED :
      aura::client::MOVE_SUCCESSFUL;
}

void ToplevelWindowEventHandler::EndMoveLoop() {
  if (in_move_loop_)
    CompleteDrag(DRAG_REVERT);
}

void ToplevelWindowEventHandler::OnDisplayConfigurationChanging() {
  CompleteDrag(DRAG_REVERT);
}

bool ToplevelWindowEventHandler::AttemptToStartDrag(
    aura::Window* window,
    const gfx::Point& point_in_parent,
    int window_component,
    aura::client::WindowMoveSource source) {
  if (window_resizer_.get())
    return false;
  WindowResizer* resizer =
      CreateWindowResizer(wm::WmWindowAura::Get(window), point_in_parent,
                          window_component, source)
          .release();
  if (!resizer)
    return false;

  window_resizer_.reset(new ScopedWindowResizer(this, resizer));

  pre_drag_window_bounds_ = window->bounds();
  in_gesture_drag_ = (source == aura::client::WINDOW_MOVE_SOURCE_TOUCH);
  return true;
}

bool ToplevelWindowEventHandler::CompleteDrag(DragCompletionStatus status) {
  if (!window_resizer_)
    return false;

  std::unique_ptr<ScopedWindowResizer> resizer(window_resizer_.release());
  switch (status) {
    case DRAG_COMPLETE:
      resizer->resizer()->CompleteDrag();
      break;
    case DRAG_REVERT:
      resizer->resizer()->RevertDrag();
      break;
    case DRAG_RESIZER_WINDOW_DESTROYED:
      // We explicitly do not invoke RevertDrag() since that may do things to
      // WindowResizer::GetAuraTarget() which was destroyed.
      break;
  }
  drag_reverted_ = (status != DRAG_COMPLETE);

  first_finger_hittest_ = HTNOWHERE;
  in_gesture_drag_ = false;
  if (in_move_loop_)
    quit_closure_.Run();
  return true;
}

void ToplevelWindowEventHandler::HandleMousePressed(
    aura::Window* target,
    ui::MouseEvent* event) {
  if (event->phase() != ui::EP_PRETARGET || !target->delegate())
    return;

  // We also update the current window component here because for the
  // mouse-drag-release-press case, where the mouse is released and
  // pressed without mouse move event.
  int component = GetWindowComponent(target, *event);
  if ((event->flags() &
        (ui::EF_IS_DOUBLE_CLICK | ui::EF_IS_TRIPLE_CLICK)) == 0 &&
      WindowResizer::GetBoundsChangeForWindowComponent(component)) {
    gfx::Point location_in_parent(
        ConvertPointToParent(target, event->location()));
    AttemptToStartDrag(target, location_in_parent, component,
                       aura::client::WINDOW_MOVE_SOURCE_MOUSE);
    // Set as handled so that other event handlers do no act upon the event
    // but still receive it so that they receive both parts of each pressed/
    // released pair.
    event->SetHandled();
  } else {
    CompleteDrag(DRAG_COMPLETE);
  }
}

void ToplevelWindowEventHandler::HandleMouseReleased(
    aura::Window* target,
    ui::MouseEvent* event) {
  if (event->phase() == ui::EP_PRETARGET)
    CompleteDrag(DRAG_COMPLETE);
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

  if (!window_resizer_)
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
  if (event->phase() != ui::EP_POSTTARGET || !target->delegate())
    return;

  // TODO(jamescook): Move the resize cursor update code into here from
  // CompoundEventFilter?
  ResizeShadowController* controller =
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

  ResizeShadowController* controller =
      Shell::GetInstance()->resize_shadow_controller();
  if (controller)
    controller->HideShadow(target);
}

void ToplevelWindowEventHandler::HandleCaptureLost(ui::LocatedEvent* event) {
  if (event->phase() == ui::EP_PRETARGET) {
    // We complete the drag instead of reverting it, as reverting it will result
    // in a weird behavior when a dragged tab produces a modal dialog while the
    // drag is in progress. crbug.com/558201.
    CompleteDrag(DRAG_COMPLETE);
  }
}

void ToplevelWindowEventHandler::SetWindowStateTypeFromGesture(
    aura::Window* window,
    wm::WindowStateType new_state_type) {
  wm::WindowState* window_state = ash::wm::GetWindowState(window);
  // TODO(oshima): Move extra logic (set_unminimize_to_restore_bounds,
  // SetRestoreBoundsInParent) that modifies the window state
  // into WindowState.
  switch (new_state_type) {
    case wm::WINDOW_STATE_TYPE_MINIMIZED:
      if (window_state->CanMinimize()) {
        window_state->Minimize();
        window_state->set_unminimize_to_restore_bounds(true);
        window_state->SetRestoreBoundsInParent(pre_drag_window_bounds_);
      }
      break;
    case wm::WINDOW_STATE_TYPE_MAXIMIZED:
      if (window_state->CanMaximize()) {
        window_state->SetRestoreBoundsInParent(pre_drag_window_bounds_);
        window_state->Maximize();
      }
      break;
    case wm::WINDOW_STATE_TYPE_LEFT_SNAPPED:
      if (window_state->CanSnap()) {
        window_state->SetRestoreBoundsInParent(pre_drag_window_bounds_);
        const wm::WMEvent event(wm::WM_EVENT_SNAP_LEFT);
        window_state->OnWMEvent(&event);
      }
      break;
    case wm::WINDOW_STATE_TYPE_RIGHT_SNAPPED:
      if (window_state->CanSnap()) {
        window_state->SetRestoreBoundsInParent(pre_drag_window_bounds_);
        const wm::WMEvent event(wm::WM_EVENT_SNAP_RIGHT);
        window_state->OnWMEvent(&event);
      }
      break;
    default:
      NOTREACHED();
  }
}

void ToplevelWindowEventHandler::ResizerWindowDestroyed() {
  CompleteDrag(DRAG_RESIZER_WINDOW_DESTROYED);
}

}  // namespace ash
