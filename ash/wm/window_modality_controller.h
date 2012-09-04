// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_MODALITY_CONTROLLER_H_
#define ASH_WM_WINDOW_MODALITY_CONTROLLER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/event_filter.h"
#include "ui/aura/window_observer.h"

namespace ui {
class LocatedEvent;
}

namespace ash {

namespace wm {
// Returns the window-modal transient child of |window|, or NULL if |window|
// does not have any window-modal transient children.
ASH_EXPORT aura::Window* GetWindowModalTransient(aura::Window* window);
}

namespace internal {

// WindowModalityController is an event filter that consumes events sent to
// windows that are the transient parents of window-modal windows. This filter
// must be added to the CompoundEventFilter so that activation works properly.
class WindowModalityController : public aura::EventFilter,
                                 public aura::EnvObserver,
                                 public aura::WindowObserver {
 public:
  WindowModalityController();
  virtual ~WindowModalityController();

  // Overridden from aura::EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 ui::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   ui::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(
      aura::Window* target,
      ui::TouchEvent* event) OVERRIDE;
  virtual ui::EventResult PreHandleGestureEvent(
      aura::Window* target,
      ui::GestureEvent* event) OVERRIDE;

  // Overridden from aura::EnvObserver:
  virtual void OnWindowInitialized(aura::Window* window) OVERRIDE;

  // Overridden from aura::WindowObserver:
  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visible) OVERRIDE;
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

 private:
  // Processes a mouse/touch event, and returns true if the event should be
  // consumed.
  bool ProcessLocatedEvent(aura::Window* target,
                           ui::LocatedEvent* event);

  std::vector<aura::Window*> windows_;

  DISALLOW_COPY_AND_ASSIGN(WindowModalityController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WINDOW_MODALITY_CONTROLLER_H_
