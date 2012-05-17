// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_controller.h"

#include "ash/drag_drop/drag_image_view.h"
#include "ash/shell.h"
#include "base/message_loop.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aura.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget_aura.h"

namespace ash {
namespace internal {

using aura::RootWindow;

namespace {
const base::TimeDelta kDragDropAnimationDuration =
    base::TimeDelta::FromMilliseconds(250);
}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DragDropController, public:

DragDropController::DragDropController()
    : drag_image_(NULL),
      drag_data_(NULL),
      drag_operation_(0),
      drag_window_(NULL),
      drag_drop_in_progress_(false),
      should_block_during_drag_drop_(true) {
  Shell::GetInstance()->AddRootWindowEventFilter(this);
  aura::client::SetDragDropClient(Shell::GetRootWindow(), this);
}

DragDropController::~DragDropController() {
  Shell::GetInstance()->RemoveRootWindowEventFilter(this);
  Cleanup();
  if (drag_image_.get())
    drag_image_.reset();
}

int DragDropController::StartDragAndDrop(const ui::OSExchangeData& data,
                                         const gfx::Point& root_location,
                                         int operation) {
  DCHECK(!drag_drop_in_progress_);
  aura::Window* capture_window = Shell::GetRootWindow()->capture_window();
  if (capture_window)
    Shell::GetRootWindow()->ReleaseCapture(capture_window);
  drag_drop_in_progress_ = true;

  drag_data_ = &data;
  drag_operation_ = operation;
  const ui::OSExchangeDataProviderAura& provider =
      static_cast<const ui::OSExchangeDataProviderAura&>(data.provider());

  drag_image_.reset(new DragImageView);
  drag_image_->SetImage(provider.drag_image());
  drag_image_offset_ = provider.drag_image_offset();
  drag_image_->SetScreenBounds(gfx::Rect(
        root_location.Subtract(drag_image_offset_),
        drag_image_->GetPreferredSize()));
  drag_image_->SetWidgetVisible(true);

  drag_window_ = NULL;
  drag_start_location_ = root_location.Subtract(drag_image_offset_);

#if !defined(OS_MACOSX)
  if (should_block_during_drag_drop_) {
    MessageLoopForUI* loop = MessageLoopForUI::current();
    MessageLoop::ScopedNestableTaskAllower allow_nested(loop);
    loop->RunWithDispatcher(aura::Env::GetInstance()->GetDispatcher());
  }
#endif  // !defined(OS_MACOSX)

  return drag_operation_;
}

void DragDropController::DragUpdate(aura::Window* target,
                                    const aura::LocatedEvent& event) {
  aura::client::DragDropDelegate* delegate = NULL;
  if (target != drag_window_) {
    if (drag_window_) {
      if ((delegate = aura::client::GetDragDropDelegate(drag_window_)))
        delegate->OnDragExited();
      drag_window_->RemoveObserver(this);
    }
    drag_window_ = target;
    drag_window_->AddObserver(this);
    if ((delegate = aura::client::GetDragDropDelegate(drag_window_))) {
      aura::DropTargetEvent e(*drag_data_,
                              event.location(),
                              event.root_location(),
                              drag_operation_);
      e.set_flags(event.flags());
      delegate->OnDragEntered(e);
    }
  } else {
    if ((delegate = aura::client::GetDragDropDelegate(drag_window_))) {
      aura::DropTargetEvent e(*drag_data_,
                              event.location(),
                              event.root_location(),
                              drag_operation_);
      e.set_flags(event.flags());
      int op = delegate->OnDragUpdated(e);
      gfx::NativeCursor cursor = (op == ui::DragDropTypes::DRAG_NONE)?
          ui::kCursorNoDrop : ui::kCursorCopy;
      Shell::GetRootWindow()->SetCursor(cursor);
    }
  }

  DCHECK(drag_image_.get());
  if (drag_image_->visible()) {
    drag_image_->SetScreenPosition(
        event.root_location().Subtract(drag_image_offset_));
  }
}

void DragDropController::Drop(aura::Window* target,
                              const aura::LocatedEvent& event) {
  Shell::GetRootWindow()->SetCursor(ui::kCursorPointer);
  aura::client::DragDropDelegate* delegate = NULL;

  // We must guarantee that a target gets a OnDragEntered before Drop. WebKit
  // depends on not getting a Drop without DragEnter. This behavior is
  // consistent with drag/drop on other platforms.
  if (target != drag_window_)
    DragUpdate(target, event);
  DCHECK(target == drag_window_);

  if ((delegate = aura::client::GetDragDropDelegate(target))) {
    aura::DropTargetEvent e(
        *drag_data_, event.location(), event.root_location(), drag_operation_);
    e.set_flags(event.flags());
    drag_operation_ = delegate->OnPerformDrop(e);
    if (drag_operation_ == 0)
      StartCanceledAnimation();
    else
      drag_image_.reset();
  } else {
    drag_image_.reset();
  }

  Cleanup();
  if (should_block_during_drag_drop_)
    MessageLoop::current()->QuitNow();
}

void DragDropController::DragCancel() {
  Shell::GetRootWindow()->SetCursor(ui::kCursorPointer);

  // |drag_window_| can be NULL if we have just started the drag and have not
  // received any DragUpdates, or, if the |drag_window_| gets destroyed during
  // a drag/drop.
  aura::client::DragDropDelegate* delegate = drag_window_?
      aura::client::GetDragDropDelegate(drag_window_) : NULL;
  if (delegate)
    delegate->OnDragExited();

  Cleanup();
  drag_operation_ = 0;
  StartCanceledAnimation();
  if (should_block_during_drag_drop_)
    MessageLoop::current()->QuitNow();
}

bool DragDropController::IsDragDropInProgress() {
  return drag_drop_in_progress_;
}

bool DragDropController::PreHandleKeyEvent(aura::Window* target,
                                           aura::KeyEvent* event) {
  return false;
}

bool DragDropController::PreHandleMouseEvent(aura::Window* target,
                                             aura::MouseEvent* event) {
  if (!drag_drop_in_progress_)
    return false;
  switch (event->type()) {
    case ui::ET_MOUSE_DRAGGED:
      DragUpdate(target, *event);
      break;
    case ui::ET_MOUSE_RELEASED:
      Drop(target, *event);
      break;
    case ui::ET_MOUSE_EXITED:
      DragCancel();
      break;
    default:
      // We could reach here if the user drops outside the root window.
      // We could also reach here because RootWindow may sometimes generate a
      // bunch of fake mouse events
      // (aura::RootWindow::PostMouseMoveEventAfterWindowChange).
      break;
  }
  return true;
}

ui::TouchStatus DragDropController::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  // TODO(sad): Also check for the touch-id.
  if (!drag_drop_in_progress_)
    return ui::TOUCH_STATUS_UNKNOWN;
  switch (event->type()) {
    case ui::ET_TOUCH_MOVED:
      DragUpdate(target, *event);
      break;
    case ui::ET_TOUCH_RELEASED:
      Drop(target, *event);
      break;
    case ui::ET_TOUCH_CANCELLED:
      DragCancel();
      break;
    default:
      return ui::TOUCH_STATUS_UNKNOWN;
  }
  return ui::TOUCH_STATUS_CONTINUE;
}

ui::GestureStatus DragDropController::PreHandleGestureEvent(
    aura::Window* target,
    aura::GestureEvent* event) {
  return ui::GESTURE_STATUS_UNKNOWN;
}

void DragDropController::OnWindowDestroyed(aura::Window* window) {
  if (drag_window_ == window) {
    drag_window_->RemoveObserver(this);
    drag_window_ = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
// DragDropController, private:

void DragDropController::OnImplicitAnimationsCompleted() {
  DCHECK(drag_image_.get());
  drag_image_.reset();
}

void DragDropController::StartCanceledAnimation() {
  aura::Window* window = drag_image_->GetWidget()->GetNativeView();
  ui::LayerAnimator* animator = window->layer()->GetAnimator();
  animator->set_preemption_strategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  // Stop waiting for any as yet unfinished implicit animations.
  StopObservingImplicitAnimations();

  ui::ScopedLayerAnimationSettings animation_setter(animator);
  animation_setter.SetTransitionDuration(kDragDropAnimationDuration);
  animation_setter.AddObserver(this);
  window->SetBounds(gfx::Rect(drag_start_location_, window->bounds().size()));
}

void DragDropController::Cleanup() {
  if (drag_window_)
    drag_window_->RemoveObserver(this);
  drag_window_ = NULL;
  drag_data_ = NULL;
  drag_drop_in_progress_ = false;
}

}  // namespace internal
}  // namespace ash
