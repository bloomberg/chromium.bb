// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_DRAG_DROP_CONTROLLER_H_
#define ASH_DRAG_DROP_DRAG_DROP_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/window_observer.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/events/event_handler.h"
#include "ui/gfx/rect.h"

namespace aura {
class RootWindow;
class Window;
}

namespace ui {
class LinearAnimation;
}

namespace ash {

namespace test {
class DragDropControllerTest;
}

namespace internal {

class DragDropTracker;
class DragDropTrackerDelegate;
class DragImageView;

class ASH_EXPORT DragDropController
    : public aura::client::DragDropClient,
      public ui::EventHandler,
      public ui::AnimationDelegate,
      public aura::WindowObserver {
 public:
  DragDropController();
  virtual ~DragDropController();

  void set_should_block_during_drag_drop(bool should_block_during_drag_drop) {
    should_block_during_drag_drop_ = should_block_during_drag_drop;
  }

  // Overridden from aura::client::DragDropClient:
  virtual int StartDragAndDrop(
      const ui::OSExchangeData& data,
      aura::RootWindow* root_window,
      aura::Window* source_window,
      const gfx::Point& root_location,
      int operation,
      ui::DragDropTypes::DragEventSource source) OVERRIDE;
  virtual void DragUpdate(aura::Window* target,
                          const ui::LocatedEvent& event) OVERRIDE;
  virtual void Drop(aura::Window* target,
                    const ui::LocatedEvent& event) OVERRIDE;
  virtual void DragCancel() OVERRIDE;
  virtual bool IsDragDropInProgress() OVERRIDE;

  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // Overridden from aura::WindowObserver.
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

 protected:
  // Helper method to create a LinearAnimation object that will run the drag
  // cancel animation. Caller take ownership of the returned object. Protected
  // for testing.
  virtual ui::LinearAnimation* CreateCancelAnimation(
      int duration,
      int frame_rate,
      ui::AnimationDelegate* delegate);

  // Actual implementation of |DragCancel()|. protected for testing.
  virtual void DoDragCancel(int drag_cancel_animation_duration_ms);

 private:
  friend class ash::test::DragDropControllerTest;

  // Overridden from ui::AnimationDelegate:
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationCanceled(const ui::Animation* animation) OVERRIDE;

  // Helper method to start drag widget flying back animation.
  void StartCanceledAnimation(int animation_duration_ms);

  // Helper method to forward |pending_log_tap_| event to |drag_source_window_|.
  void ForwardPendingLongTap();

  // Helper method to reset everything.
  void Cleanup();

  scoped_ptr<DragImageView> drag_image_;
  gfx::Vector2d drag_image_offset_;
  const ui::OSExchangeData* drag_data_;
  int drag_operation_;

  // Window that is currently under the drag cursor.
  aura::Window* drag_window_;

  // Starting and final bounds for the drag image for the drag cancel animation.
  gfx::Rect drag_image_initial_bounds_for_cancel_animation_;
  gfx::Rect drag_image_final_bounds_for_cancel_animation_;

  scoped_ptr<ui::LinearAnimation> cancel_animation_;

  // Window that started the drag.
  aura::Window* drag_source_window_;

  // Indicates whether the caller should be blocked on a drag/drop session.
  // Only be used for tests.
  bool should_block_during_drag_drop_;

  // Closure for quitting nested message loop.
  base::Closure quit_closure_;

  scoped_ptr<ash::internal::DragDropTracker> drag_drop_tracker_;
  scoped_ptr<DragDropTrackerDelegate> drag_drop_window_delegate_;

  ui::DragDropTypes::DragEventSource current_drag_event_source_;

  // Holds a synthetic long tap event to be sent to the |drag_source_window_|.
  // See comment in OnGestureEvent() on why we need this.
  scoped_ptr<ui::GestureEvent> pending_long_tap_;

  base::WeakPtrFactory<DragDropController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DragDropController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_DRAG_DROP_DRAG_DROP_CONTROLLER_H_
