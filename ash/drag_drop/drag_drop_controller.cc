// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_controller.h"

#include "ash/drag_drop/drag_drop_tracker.h"
#include "ash/drag_drop/drag_image_view.h"
#include "ash/shell.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/cursor_manager.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aura.h"
#include "ui/base/events/event.h"
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
      should_block_during_drag_drop_(true) {
  Shell::GetInstance()->AddEnvEventFilter(this);
}

DragDropController::~DragDropController() {
  Shell::GetInstance()->RemoveEnvEventFilter(this);
  Cleanup();
  if (drag_image_.get())
    drag_image_.reset();
}

int DragDropController::StartDragAndDrop(const ui::OSExchangeData& data,
                                         aura::RootWindow* root_window,
                                         const gfx::Point& root_location,
                                         int operation) {
  DCHECK(!IsDragDropInProgress());

  drag_drop_tracker_.reset(new DragDropTracker(root_window));

  drag_data_ = &data;
  drag_operation_ = operation;
  const ui::OSExchangeDataProviderAura& provider =
      static_cast<const ui::OSExchangeDataProviderAura&>(data.provider());

  drag_image_.reset(new DragImageView);
  drag_image_->SetImage(provider.drag_image());
  drag_image_offset_ = provider.drag_image_offset();
  drag_image_->SetBoundsInScreen(gfx::Rect(
        root_location.Subtract(drag_image_offset_),
        drag_image_->GetPreferredSize()));
  drag_image_->SetWidgetVisible(true);

  drag_window_ = NULL;
  drag_start_location_ = root_location.Subtract(drag_image_offset_);

#if !defined(OS_MACOSX)
  if (should_block_during_drag_drop_) {
    base::RunLoop run_loop(aura::Env::GetInstance()->GetDispatcher());
    quit_closure_ = run_loop.QuitClosure();
    MessageLoopForUI* loop = MessageLoopForUI::current();
    MessageLoop::ScopedNestableTaskAllower allow_nested(loop);
    run_loop.Run();
  }
#endif  // !defined(OS_MACOSX)

  return drag_operation_;
}

void DragDropController::DragUpdate(aura::Window* target,
                                    const ui::LocatedEvent& event) {
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
      ui::DropTargetEvent e(*drag_data_,
                            event.location(),
                            event.root_location(),
                            drag_operation_);
      e.set_flags(event.flags());
      delegate->OnDragEntered(e);
    }
  } else {
    if ((delegate = aura::client::GetDragDropDelegate(drag_window_))) {
      ui::DropTargetEvent e(*drag_data_,
                            event.location(),
                            event.root_location(),
                            drag_operation_);
      e.set_flags(event.flags());
      int op = delegate->OnDragUpdated(e);
      gfx::NativeCursor cursor = ui::kCursorNoDrop;
      if (op & ui::DragDropTypes::DRAG_COPY)
        cursor = ui::kCursorCopy;
      else if (op & ui::DragDropTypes::DRAG_LINK)
        cursor = ui::kCursorAlias;
      else if (op & ui::DragDropTypes::DRAG_MOVE)
        cursor = ui::kCursorMove;
      ash::Shell::GetInstance()->cursor_manager()->SetCursor(cursor);
    }
  }

  DCHECK(drag_image_.get());
  if (drag_image_->visible()) {
    gfx::Point root_location_in_screen = event.root_location();
    ash::wm::ConvertPointToScreen(target->GetRootWindow(),
                                  &root_location_in_screen);
    drag_image_->SetScreenPosition(
        root_location_in_screen.Subtract(drag_image_offset_));
  }
}

void DragDropController::Drop(aura::Window* target,
                              const ui::LocatedEvent& event) {
  ash::Shell::GetInstance()->cursor_manager()->SetCursor(ui::kCursorPointer);
  aura::client::DragDropDelegate* delegate = NULL;

  // We must guarantee that a target gets a OnDragEntered before Drop. WebKit
  // depends on not getting a Drop without DragEnter. This behavior is
  // consistent with drag/drop on other platforms.
  if (target != drag_window_)
    DragUpdate(target, event);
  DCHECK(target == drag_window_);

  if ((delegate = aura::client::GetDragDropDelegate(target))) {
    ui::DropTargetEvent e(
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
    quit_closure_.Run();
}

void DragDropController::DragCancel() {
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
  StartCanceledAnimation();
  if (should_block_during_drag_drop_)
    quit_closure_.Run();
}

bool DragDropController::IsDragDropInProgress() {
  return !!drag_drop_tracker_.get();
}

bool DragDropController::PreHandleKeyEvent(aura::Window* target,
                                           ui::KeyEvent* event) {
  if (IsDragDropInProgress() && event->key_code() == ui::VKEY_ESCAPE) {
    DragCancel();
    return true;
  }
  return false;
}

bool DragDropController::PreHandleMouseEvent(aura::Window* target,
                                             ui::MouseEvent* event) {
  if (!IsDragDropInProgress())
    return false;
  aura::Window* translated_target = drag_drop_tracker_->GetTarget(*event);
  if (!translated_target) {
    DragCancel();
    return true;
  }
  scoped_ptr<ui::MouseEvent> translated_event(
      drag_drop_tracker_->ConvertMouseEvent(translated_target, *event));
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
  return true;
}

ui::TouchStatus DragDropController::PreHandleTouchEvent(
    aura::Window* target,
    ui::TouchEvent* event) {
  // TODO(sad): Also check for the touch-id.
  // TODO(varunjain): Add code for supporting drag-and-drop across displays
  // (http://crbug.com/114755).
  if (!IsDragDropInProgress())
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

ui::EventResult DragDropController::PreHandleGestureEvent(
    aura::Window* target,
    ui::GestureEvent* event) {
  return ui::ER_UNHANDLED;
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

  // By the time we finish animation, another drag/drop session may have
  // started. We do not want to destroy the drag image in that case.
  if (!IsDragDropInProgress())
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
  // Cleanup can be called again while deleting DragDropTracker, so use Pass
  // instead of reset to avoid double free.
  drag_drop_tracker_.Pass();
}

}  // namespace internal
}  // namespace ash
