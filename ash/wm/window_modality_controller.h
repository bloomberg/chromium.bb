// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_MODALITY_CONTROLLER_H_
#define ASH_WM_WINDOW_MODALITY_CONTROLLER_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/aura/event_filter.h"

namespace ash {
namespace internal {

// WindowModalityController is an event filter that consumes events sent to
// windows that are the transient parents of window-modal windows. This filter
// must be added to the RootWindowEventFilter so that activation works properly.
class WindowModalityController : public aura::EventFilter {
 public:
  WindowModalityController();
  virtual ~WindowModalityController();

  // Returns the window-modal transient child of |window|, or NULL if |window|
  // does not have any window-modal transient children.
  static aura::Window* GetWindowModalTransient(aura::Window* window);

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
  DISALLOW_COPY_AND_ASSIGN(WindowModalityController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WINDOW_MODALITY_CONTROLLER_H_
