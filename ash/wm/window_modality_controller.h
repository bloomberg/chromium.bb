// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_MODALITY_CONTROLLER_H_
#define ASH_WM_WINDOW_MODALITY_CONTROLLER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/base/events/event_handler.h"

namespace ui {
class LocatedEvent;
}

namespace ash {

namespace wm {

// Sets the modal parent for the child.
ASH_EXPORT void SetModalParent(aura::Window* child, aura::Window* parent);

// Returns the modal transient child of |window|, or NULL if |window| does not
// have any modal transient children.
ASH_EXPORT aura::Window* GetModalTransient(aura::Window* window);

}

namespace internal {

// WindowModalityController is an event filter that consumes events sent to
// windows that are the transient parents of window-modal windows. This filter
// must be added to the CompoundEventFilter so that activation works properly.
class WindowModalityController : public ui::EventHandler,
                                 public aura::EnvObserver,
                                 public aura::WindowObserver {
 public:
  WindowModalityController();
  virtual ~WindowModalityController();

  // Overridden from ui::EventHandler:
  virtual ui::EventResult OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual ui::EventResult OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual ui::EventResult OnTouchEvent(ui::TouchEvent* event) OVERRIDE;

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
