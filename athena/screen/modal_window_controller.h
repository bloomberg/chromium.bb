// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SCREEN_MODAL_WINDOW_CONTROLLER_H_
#define ATHENA_SCREEN_MODAL_WINDOW_CONTROLLER_H_

#include "athena/athena_export.h"
#include "ui/aura/window_observer.h"

namespace athena {

// ModalWindow controller manages the modal window and
// its container. This gets created when a modal window is opened
// and deleted when all modal windows are deleted.
class ATHENA_EXPORT ModalWindowController : public aura::WindowObserver {
 public:
  // Returns the ModalWindowController associated with the container.
  static ModalWindowController* Get(aura::Window* container);

  explicit ModalWindowController(int container_priority);
  ~ModalWindowController() override;

  aura::Window* modal_container() { return modal_container_; }

  bool dimmed() const { return dimmed_; }

 private:
  // aura::WindowObserver:
  virtual void OnWindowAdded(aura::Window* child) override;
  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visible) override;
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) override;
  virtual void OnWindowDestroyed(aura::Window* window) override;

  // Tells if the child is not a dimmer window and a child of the modal
  // container.
  bool IsChildWindow(aura::Window* child) const;

  void UpdateDimmerWindowBounds();

  // Change dimming state based on the visible window in the container.
  void UpdateDimming(aura::Window* ignore);

  // Note: changing true -> false will delete the modal_container_.
  void SetDimmed(bool dimmed);

  aura::Window* modal_container_;  // not owned.
  aura::Window* dimmer_window_;    // not owned.

  bool dimmed_;
  DISALLOW_COPY_AND_ASSIGN(ModalWindowController);
};

}  // namespace athena

#endif  // ATHENA_SCREEN_MODAL_WINDOW_CONTROLLER_H_
