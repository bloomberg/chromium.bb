// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_ACTIVATION_CONTROLLER_H_
#define ASH_WM_ACTIVATION_CONTROLLER_H_
#pragma once

#include "ash/wm/scoped_observer.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/root_window_observer.h"
#include "ui/aura/window_observer.h"
#include "ash/ash_export.h"

namespace ash {
namespace internal {

// Exported for unit tests.
class ASH_EXPORT ActivationController
    : public aura::client::ActivationClient,
      public aura::WindowObserver,
      public aura::EnvObserver,
      public aura::RootWindowObserver {
 public:
  ActivationController();
  virtual ~ActivationController();

  // Returns true if |window| exists within a container that supports
  // activation. |event| is the revent responsible for initiating the change, or
  // NULL if there is no event.
  static aura::Window* GetActivatableWindow(aura::Window* window,
                                            const aura::Event* event);

  // Overridden from aura::client::ActivationClient:
  virtual void ActivateWindow(aura::Window* window) OVERRIDE;
  virtual void DeactivateWindow(aura::Window* window) OVERRIDE;
  virtual aura::Window* GetActiveWindow() OVERRIDE;
  virtual bool OnWillFocusWindow(aura::Window* window,
                                 const aura::Event* event) OVERRIDE;
  virtual bool CanActivateWindow(aura::Window* window) const OVERRIDE;

  // Overridden from aura::WindowObserver:
  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visible) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  // Overridden from aura::EnvObserver:
  virtual void OnWindowInitialized(aura::Window* window) OVERRIDE;

  // Overridden from aura::RootWindowObserver:
  virtual void OnWindowFocused(aura::Window* window) OVERRIDE;

 private:
  // Implementation of ActivateWindow() with an Event.
  void ActivateWindowWithEvent(aura::Window* window,
                               const aura::Event* event);

  // Shifts activation to the next window, ignoring |window|. Returns the next
  // window.
  aura::Window* ActivateNextWindow(aura::Window* window);

  // Returns the next window that should be activated, ignoring |ignore|.
  aura::Window* GetTopmostWindowToActivate(aura::Window* ignore) const;

  // Returns the next window that should be activated in |container| ignoring
  // the window |ignore|.
  aura::Window* GetTopmostWindowToActivateInContainer(
      aura::Window* container,
      aura::Window* ignore) const;

  // True inside ActivateWindow(). Used to prevent recursion of focus
  // change notifications causing activation.
  bool updating_activation_;

  ScopedObserver<aura::Window, aura::WindowObserver> observer_manager_;

  DISALLOW_COPY_AND_ASSIGN(ActivationController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_ACTIVATION_CONTROLLER_H_
