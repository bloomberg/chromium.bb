// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_controller.h"

#include "ash/drag_drop/drag_drop_tracker.h"
#include "ash/drag_drop/drag_image_view.h"
#include "ash/shell.h"
#include "ash/wm/coordinate_conversion.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/path.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/wm/public/drag_drop_delegate.h"

namespace ash {
namespace {

// The duration of the drag cancel animation in millisecond.
const int kCancelAnimationDuration = 250;
const int kTouchCancelAnimationDuration = 20;
// The frame rate of the drag cancel animation in hertz.
const int kCancelAnimationFrameRate = 60;

// For touch initiated dragging, we scale and shift drag image by the following:
static const float kTouchDragImageScale = 1.2f;
static const int kTouchDragImageVerticalOffset = -25;

// Adjusts the drag image bounds such that the new bounds are scaled by |scale|
// and translated by the |drag_image_offset| and and additional
// |vertical_offset|.
gfx::Rect AdjustDragImageBoundsForScaleAndOffset(
    const gfx::Rect& drag_image_bounds,
    int vertical_offset,
    float scale,
    gfx::Vector2d* drag_image_offset) {
  gfx::PointF final_origin = drag_image_bounds.origin();
  gfx::SizeF final_size = drag_image_bounds.size();
  final_size.Scale(scale);
  drag_image_offset->set_x(drag_image_offset->x() * scale);
  drag_image_offset->set_y(drag_image_offset->y() * scale);
  float total_x_offset = drag_image_offset->x();
  float total_y_offset = drag_image_offset->y() - vertical_offset;
  final_origin.Offset(-total_x_offset, -total_y_offset);
  return gfx::ToEnclosingRect(gfx::RectF(final_origin, final_size));
}

void DispatchGestureEndToWindow(aura::Window* window) {
  if (window && window->delegate()) {
    ui::GestureEvent gesture_end(
        0,
        0,
        0,
        ui::EventTimeForNow(),
        ui::GestureEventDetails(ui::ET_GESTURE_END, 0, 0));
    window->delegate()->OnGestureEvent(&gesture_end);
  }
}
}  // namespace

class DragDropTrackerDelegate : public aura::WindowDelegate {
 public:
  explicit DragDropTrackerDelegate(DragDropController* controller)
      : drag_drop_controller_(controller) {}
  virtual ~DragDropTrackerDelegate() {}

  // Overridden from WindowDelegate:
  virtual gfx::Size GetMinimumSize() const OVERRIDE {
    return gfx::Size();
  }

  virtual gfx::Size GetMaximumSize() const OVERRIDE {
    return gfx::Size();
  }

  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE {}
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) OVERRIDE {
    return gfx::kNullCursor;
  }
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE {
    return HTCAPTION;
  }
  virtual bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) OVERRIDE {
    return true;
  }
  virtual bool CanFocus() OVERRIDE { return true; }
  virtual void OnCaptureLost() OVERRIDE {
    if (drag_drop_controller_->IsDragDropInProgress())
      drag_drop_controller_->DragCancel();
  }
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
  }
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {}
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {}
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE {}
  virtual void OnWindowTargetVisibilityChanged(bool visible) OVERRIDE {}
  virtual bool HasHitTestMask() const OVERRIDE {
    return true;
  }
  virtual void GetHitTestMask(gfx::Path* mask) const OVERRIDE {
    DCHECK(mask->isEmpty());
  }

 private:
  DragDropController* drag_drop_controller_;

  DISALLOW_COPY_AND_ASSIGN(DragDropTrackerDelegate);
};

////////////////////////////////////////////////////////////////////////////////
// DragDropController, public:

DragDropController::DragDropController()
    : drag_data_(NULL),
      drag_operation_(0),
      drag_window_(NULL),
      drag_source_window_(NULL),
      should_block_during_drag_drop_(true),
      drag_drop_window_delegate_(new DragDropTrackerDelegate(this)),
      current_drag_event_source_(ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE),
      weak_factory_(this) {
  Shell::GetInstance()->PrependPreTargetHandler(this);
}

DragDropController::~DragDropController() {
  Shell::GetInstance()->RemovePreTargetHandler(this);
  Cleanup();
  if (cancel_animation_)
    cancel_animation_->End();
  if (drag_image_)
    drag_image_.reset();
}

int DragDropController::StartDragAndDrop(
    const ui::OSExchangeData& data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& root_location,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  if (IsDragDropInProgress())
    return 0;

  const ui::OSExchangeData::Provider* provider = &data.provider();
  // We do not support touch drag/drop without a drag image.
  if (source == ui::DragDropTypes::DRAG_EVENT_SOURCE_TOUCH &&
      provider->GetDragImage().size().IsEmpty())
    return 0;

  current_drag_event_source_ = source;
  DragDropTracker* tracker =
      new DragDropTracker(root_window, drag_drop_window_delegate_.get());
  if (source == ui::DragDropTypes::DRAG_EVENT_SOURCE_TOUCH) {
    // We need to transfer the current gesture sequence and the GR's touch event
    // queue to the |drag_drop_tracker_|'s capture window so that when it takes
    // capture, it still gets a valid gesture state.
    ui::GestureRecognizer::Get()->TransferEventsTo(source_window,
        tracker->capture_window());
    // We also send a gesture end to the source window so it can clear state.
    // TODO(varunjain): Remove this whole block when gesture sequence
    // transferring is properly done in the GR (http://crbug.com/160558)
    DispatchGestureEndToWindow(source_window);
  }
  tracker->TakeCapture();
  drag_drop_tracker_.reset(tracker);
  drag_source_window_ = source_window;
  if (drag_source_window_)
    drag_source_window_->AddObserver(this);
  pending_long_tap_.reset();

  drag_data_ = &data;
  drag_operation_ = operation;

  float drag_image_scale = 1;
  int drag_image_vertical_offset = 0;
  if (source == ui::DragDropTypes::DRAG_EVENT_SOURCE_TOUCH) {
    drag_image_scale = kTouchDragImageScale;
    drag_image_vertical_offset = kTouchDragImageVerticalOffset;
  }
  gfx::Point start_location = root_location;
  ash::wm::ConvertPointToScreen(root_window, &start_location);
  drag_image_final_bounds_for_cancel_animation_ = gfx::Rect(
      start_location - provider->GetDragImageOffset(),
      provider->GetDragImage().size());
  drag_image_.reset(new DragImageView(source_window->GetRootWindow(), source));
  drag_image_->SetImage(provider->GetDragImage());
  drag_image_offset_ = provider->GetDragImageOffset();
  gfx::Rect drag_image_bounds(start_location, drag_image_->GetPreferredSize());
  drag_image_bounds = AdjustDragImageBoundsForScaleAndOffset(drag_image_bounds,
      drag_image_vertical_offset, drag_image_scale, &drag_image_offset_);
  drag_image_->SetBoundsInScreen(drag_image_bounds);
  drag_image_->SetWidgetVisible(true);
  if (source == ui::DragDropTypes::DRAG_EVENT_SOURCE_TOUCH) {
    drag_image_->SetTouchDragOperationHintPosition(gfx::Point(
        drag_image_offset_.x(),
        drag_image_offset_.y() + drag_image_vertical_offset));
  }

  drag_window_ = NULL;

  // Ends cancel animation if it's in progress.
  if (cancel_animation_)
    cancel_animation_->End();

  if (should_block_during_drag_drop_) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoop::ScopedNestableTaskAllower allow_nested(loop);
    run_loop.Run();
  }

  if (!cancel_animation_.get() || !cancel_animation_->is_animating() ||
      !pending_long_tap_.get()) {
    // If drag cancel animation is running, this cleanup is done when the
    // animation completes.
    if (drag_source_window_)
      drag_source_window_->RemoveObserver(this);
    drag_source_window_ = NULL;
  }

  return drag_operation_;
}

void DragDropController::DragUpdate(aura::Window* target,
                                    const ui::LocatedEvent& event) {
  int op = ui::DragDropTypes::DRAG_NONE;
  if (target != drag_window_) {
    if (drag_window_) {
      aura::client::DragDropDelegate* delegate =
          aura::client::GetDragDropDelegate(drag_window_);
      if (delegate)
        delegate->OnDragExited();
      if (drag_window_ != drag_source_window_)
        drag_window_->RemoveObserver(this);
    }
    drag_window_ = target;
    // We are already an observer of |drag_source_window_| so no need to add.
    if (drag_window_ != drag_source_window_)
      drag_window_->AddObserver(this);
    aura::client::DragDropDelegate* delegate =
        aura::client::GetDragDropDelegate(drag_window_);
    if (delegate) {
      ui::DropTargetEvent e(*drag_data_,
                            event.location(),
                            event.root_location(),
                            drag_operation_);
      e.set_flags(event.flags());
      delegate->OnDragEntered(e);
    }
  } else {
    aura::client::DragDropDelegate* delegate =
        aura::client::GetDragDropDelegate(drag_window_);
    if (delegate) {
      ui::DropTargetEvent e(*drag_data_,
                            event.location(),
                            event.root_location(),
                            drag_operation_);
      e.set_flags(event.flags());
      op = delegate->OnDragUpdated(e);
      gfx::NativeCursor cursor = ui::kCursorNoDrop;
      if (op & ui::DragDropTypes::DRAG_COPY)
        cursor = ui::kCursorCopy;
      else if (op & ui::DragDropTypes::DRAG_LINK)
        cursor = ui::kCursorAlias;
      else if (op & ui::DragDropTypes::DRAG_MOVE)
        cursor = ui::kCursorGrabbing;
      ash::Shell::GetInstance()->cursor_manager()->SetCursor(cursor);
    }
  }

  DCHECK(drag_image_.get());
  if (drag_image_->visible()) {
    gfx::Point root_location_in_screen = event.root_location();
    ash::wm::ConvertPointToScreen(target->GetRootWindow(),
                                  &root_location_in_screen);
    drag_image_->SetScreenPosition(
        root_location_in_screen - drag_image_offset_);
    drag_image_->SetTouchDragOperation(op);
  }
}

void DragDropController::Drop(aura::Window* target,
                              const ui::LocatedEvent& event) {
  ash::Shell::GetInstance()->cursor_manager()->SetCursor(ui::kCursorPointer);

  // We must guarantee that a target gets a OnDragEntered before Drop. WebKit
  // depends on not getting a Drop without DragEnter. This behavior is
  // consistent with drag/drop on other platforms.
  if (target != drag_window_)
    DragUpdate(target, event);
  DCHECK(target == drag_window_);

  aura::client::DragDropDelegate* delegate =
      aura::client::GetDragDropDelegate(target);
  if (delegate) {
    ui::DropTargetEvent e(
        *drag_data_, event.location(), event.root_location(), drag_operation_);
    e.set_flags(event.flags());
    drag_operation_ = delegate->OnPerformDrop(e);
    if (drag_operation_ == 0)
      StartCanceledAnimation(kCancelAnimationDuration);
    else
      drag_image_.reset();
  } else {
    drag_image_.reset();
  }

  Cleanup();
  if (should_block_during_drag_drop_)
    quit_closure_.Run();
}

void DragDropController::DragCancel() {
  DoDragCancel(kCancelAnimationDuration);
}

bool DragDropController::IsDragDropInProgress() {
  return !!drag_drop_tracker_.get();
}

void DragDropController::OnKeyEvent(ui::KeyEvent* event) {
  if (IsDragDropInProgress() && event->key_code() == ui::VKEY_ESCAPE) {
    DragCancel();
    event->StopPropagation();
  }
}

void DragDropController::OnMouseEvent(ui::MouseEvent* event) {
  if (!IsDragDropInProgress())
    return;

  // If current drag session was not started by mouse, dont process this mouse
  // event, but consume it so it does not interfere with current drag session.
  if (current_drag_event_source_ !=
      ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE) {
    event->StopPropagation();
    return;
  }

  aura::Window* translated_target = drag_drop_tracker_->GetTarget(*event);
  if (!translated_target) {
    DragCancel();
    event->StopPropagation();
    return;
  }
  scoped_ptr<ui::LocatedEvent> translated_event(
      drag_drop_tracker_->ConvertEvent(translated_target, *event));
  switch (translated_event->type()) {
    case ui::ET_MOUSE_DRAGGED:
      DragUpdate(translated_target, *translated_event.get());
      break;
    case ui::ET_MOUSE_RELEASED:
      Drop(translated_target, *translated_event.get());
      break;
    default:
      // We could also reach here because RootWindow may sometimes generate a
      // bunch of fake mouse events
      // (aura::RootWindow::PostMouseMoveEventAfterWindowChange).
      break;
  }
  event->StopPropagation();
}

void DragDropController::OnTouchEvent(ui::TouchEvent* event) {
  if (!IsDragDropInProgress())
    return;

  // If current drag session was not started by touch, dont process this touch
  // event, but consume it so it does not interfere with current drag session.
  if (current_drag_event_source_ != ui::DragDropTypes::DRAG_EVENT_SOURCE_TOUCH)
    event->StopPropagation();

  if (event->handled())
    return;

  if (event->type() == ui::ET_TOUCH_CANCELLED)
    DragCancel();
}

void DragDropController::OnGestureEvent(ui::GestureEvent* event) {
  if (!IsDragDropInProgress())
    return;

  // No one else should handle gesture events when in drag drop. Note that it is
  // not enough to just set ER_HANDLED because the dispatcher only stops
  // dispatching when the event has ER_CONSUMED. If we just set ER_HANDLED, the
  // event will still be dispatched to other handlers and we depend on
  // individual handlers' kindness to not touch events marked ER_HANDLED (not
  // all handlers are so kind and may cause bugs like crbug.com/236493).
  event->StopPropagation();

  // If current drag session was not started by touch, dont process this event.
  if (current_drag_event_source_ != ui::DragDropTypes::DRAG_EVENT_SOURCE_TOUCH)
    return;

  // Apply kTouchDragImageVerticalOffset to the location.
  ui::GestureEvent touch_offset_event(*event,
                                      static_cast<aura::Window*>(NULL),
                                      static_cast<aura::Window*>(NULL));
  gfx::Point touch_offset_location = touch_offset_event.location();
  gfx::Point touch_offset_root_location = touch_offset_event.root_location();
  touch_offset_location.Offset(0, kTouchDragImageVerticalOffset);
  touch_offset_root_location.Offset(0, kTouchDragImageVerticalOffset);
  touch_offset_event.set_location(touch_offset_location);
  touch_offset_event.set_root_location(touch_offset_root_location);

  aura::Window* translated_target =
      drag_drop_tracker_->GetTarget(touch_offset_event);
  if (!translated_target) {
    DragCancel();
    event->SetHandled();
    return;
  }
  scoped_ptr<ui::LocatedEvent> translated_event(
      drag_drop_tracker_->ConvertEvent(translated_target, touch_offset_event));

  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_UPDATE:
      DragUpdate(translated_target, *translated_event.get());
      break;
    case ui::ET_GESTURE_SCROLL_END:
      Drop(translated_target, *translated_event.get());
      break;
    case ui::ET_SCROLL_FLING_START:
    case ui::ET_GESTURE_LONG_TAP:
      // Ideally we would want to just forward this long tap event to the
      // |drag_source_window_|. However, webkit does not accept events while a
      // drag drop is still in progress. The drag drop ends only when the nested
      // message loop ends. Due to this stupidity, we have to defer forwarding
      // the long tap.
      pending_long_tap_.reset(
          new ui::GestureEvent(*event,
              static_cast<aura::Window*>(drag_drop_tracker_->capture_window()),
              static_cast<aura::Window*>(drag_source_window_)));
      DoDragCancel(kTouchCancelAnimationDuration);
      break;
    default:
      break;
  }
  event->SetHandled();
}

void DragDropController::OnWindowDestroyed(aura::Window* window) {
  if (drag_window_ == window)
    drag_window_ = NULL;
  if (drag_source_window_ == window)
    drag_source_window_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// DragDropController, protected:

gfx::LinearAnimation* DragDropController::CreateCancelAnimation(
    int duration,
    int frame_rate,
    gfx::AnimationDelegate* delegate) {
  return new gfx::LinearAnimation(duration, frame_rate, delegate);
}

////////////////////////////////////////////////////////////////////////////////
// DragDropController, private:

void DragDropController::AnimationEnded(const gfx::Animation* animation) {
  cancel_animation_.reset();

  // By the time we finish animation, another drag/drop session may have
  // started. We do not want to destroy the drag image in that case.
  if (!IsDragDropInProgress())
    drag_image_.reset();
  if (pending_long_tap_) {
    // If not in a nested message loop, we can forward the long tap right now.
    if (!should_block_during_drag_drop_)
      ForwardPendingLongTap();
    else {
      // See comment about this in OnGestureEvent().
      base::MessageLoopForUI::current()->PostTask(
          FROM_HERE,
          base::Bind(&DragDropController::ForwardPendingLongTap,
                     weak_factory_.GetWeakPtr()));
    }
  }
}

void DragDropController::DoDragCancel(int drag_cancel_animation_duration_ms) {
  ash::Shell::GetInstance()->cursor_manager()->SetCursor(ui::kCursorPointer);

  // |drag_window_| can be NULL if we have just started the drag and have not
  // received any DragUpdates, or, if the |drag_window_| gets destroyed during
  // a drag/drop.
  aura::client::DragDropDelegate* delegate = drag_window_?
      aura::client::GetDragDropDelegate(drag_window_) : NULL;
  if (delegate)
    delegate->OnDragExited();

  Cleanup();
  drag_operation_ = 0;
  StartCanceledAnimation(drag_cancel_animation_duration_ms);
  if (should_block_during_drag_drop_)
    quit_closure_.Run();
}

void DragDropController::AnimationProgressed(const gfx::Animation* animation) {
  gfx::Rect current_bounds = animation->CurrentValueBetween(
      drag_image_initial_bounds_for_cancel_animation_,
      drag_image_final_bounds_for_cancel_animation_);
  drag_image_->SetBoundsInScreen(current_bounds);
}

void DragDropController::AnimationCanceled(const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void DragDropController::StartCanceledAnimation(int animation_duration_ms) {
  DCHECK(drag_image_.get());
  drag_image_->SetTouchDragOperationHintOff();
  drag_image_initial_bounds_for_cancel_animation_ =
      drag_image_->GetBoundsInScreen();
  cancel_animation_.reset(CreateCancelAnimation(animation_duration_ms,
                                                kCancelAnimationFrameRate,
                                                this));
  cancel_animation_->Start();
}

void DragDropController::ForwardPendingLongTap() {
  if (drag_source_window_ && drag_source_window_->delegate()) {
    drag_source_window_->delegate()->OnGestureEvent(pending_long_tap_.get());
    DispatchGestureEndToWindow(drag_source_window_);
  }
  pending_long_tap_.reset();
  if (drag_source_window_)
    drag_source_window_->RemoveObserver(this);
  drag_source_window_ = NULL;
}

void DragDropController::Cleanup() {
  if (drag_window_)
    drag_window_->RemoveObserver(this);
  drag_window_ = NULL;
  drag_data_ = NULL;
  // Cleanup can be called again while deleting DragDropTracker, so use Pass
  // instead of reset to avoid double free.
  drag_drop_tracker_.Pass();
}

}  // namespace ash
