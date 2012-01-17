// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_DRAG_DROP_CONTROLLER_H_
#define ASH_DRAG_DROP_DRAG_DROP_CONTROLLER_H_
#pragma once

#include "ash/ash_export.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/event.h"
#include "ui/aura/event_filter.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/events.h"
#include "ui/gfx/compositor/layer_animation_observer.h"
#include "ui/gfx/point.h"

namespace aura {
class Window;
}

namespace ui {
class LayerAnimationSequence;
}

namespace ash {

namespace test {
class DragDropControllerTest;
}

namespace internal {

class DragImageView;

class ASH_EXPORT DragDropController
    : public aura::client::DragDropClient,
      public aura::EventFilter,
      public ui::LayerAnimationObserver {
 public:
  DragDropController();
  virtual ~DragDropController();

  void set_should_block_during_drag_drop(bool should_block_during_drag_drop) {
    should_block_during_drag_drop_ = should_block_during_drag_drop;
  }

  // Overridden from aura::client::DragDropClient:
  virtual int StartDragAndDrop(const ui::OSExchangeData& data,
                                int operation) OVERRIDE;
  virtual void DragUpdate(aura::Window* target,
                          const aura::MouseEvent& event) OVERRIDE;
  virtual void Drop(aura::Window* target,
                    const aura::MouseEvent& event) OVERRIDE;
  virtual void DragCancel() OVERRIDE;
  virtual bool IsDragDropInProgress() OVERRIDE;

  // Overridden from aura::EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      aura::Window* target,
      aura::GestureEvent* event) OVERRIDE;

 private:
  friend class ash::test::DragDropControllerTest;

  // Overridden from ui::LayerAnimationObserver:
  virtual void OnLayerAnimationEnded(
      const ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationAborted(
      const ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationScheduled(
      const ui::LayerAnimationSequence* sequence) OVERRIDE {}

  // Helper method to start drag widget flying back animation.
  void StartCanceledAnimation();

  // Helper method to reset everything.
  void Cleanup();

  scoped_ptr<DragImageView> drag_image_;
  const ui::OSExchangeData* drag_data_;
  int drag_operation_;
  aura::Window* dragged_window_;
  gfx::Point drag_start_location_;

  bool drag_drop_in_progress_;

  // Indicates whether the caller should be blocked on a drag/drop session.
  // Only be used for tests.
  bool should_block_during_drag_drop_;

  DISALLOW_COPY_AND_ASSIGN(DragDropController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_DRAG_DROP_DRAG_DROP_CONTROLLER_H_
