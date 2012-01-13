// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_ACTIVATION_CONTROLLER_H_
#define ASH_WM_ACTIVATION_CONTROLLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/root_window_observer.h"
#include "ui/aura/window_observer.h"
#include "ash/ash_export.h"

namespace ash {
namespace internal {

// Exported for unit tests.
class ASH_EXPORT ActivationController
    : public aura::client::ActivationClient,
      public aura::WindowObserver,
      public aura::RootWindowObserver {
 public:
  ActivationController();
  virtual ~ActivationController();

  // Returns true if |window| exists within a container that supports
  // activation.
  static aura::Window* GetActivatableWindow(aura::Window* window);

  // Overridden from aura::client::ActivationClient:
  virtual void ActivateWindow(aura::Window* window) OVERRIDE;
  virtual void DeactivateWindow(aura::Window* window) OVERRIDE;
  virtual aura::Window* GetActiveWindow() OVERRIDE;
  virtual bool CanFocusWindow(aura::Window* window) const OVERRIDE;

  // Overridden from aura::WindowObserver:
  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visible) OVERRIDE;
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

  // Overridden from aura::RootWindowObserver:
  virtual void OnWindowInitialized(aura::Window* window) OVERRIDE;
  virtual void OnWindowFocused(aura::Window* window) OVERRIDE;

  void set_default_container_for_test(aura::Window* window) {
    default_container_for_test_ = window;
  }

 private:
  // Shifts activation to the next window, ignoring |window|.
  void ActivateNextWindow(aura::Window* window);

  // Returns the next window that should be activated, ignoring |ignore|.
  aura::Window* GetTopmostWindowToActivate(aura::Window* ignore) const;

  // True inside ActivateWindow(). Used to prevent recursion of focus
  // change notifications causing activation.
  bool updating_activation_;

  // For tests that are not running with a Shell instance,
  // ActivationController's attempts to locate the next active window in
  // GetTopmostWindowToActivate() will crash, so we provide this way for such
  // tests to specify a default container.
  aura::Window* default_container_for_test_;

  DISALLOW_COPY_AND_ASSIGN(ActivationController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_ACTIVATION_CONTROLLER_H_
