// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DISPLAY_PRIMARY_DISPLAY_SWITCH_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_DISPLAY_PRIMARY_DISPLAY_SWITCH_OBSERVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/root_window_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
class RootWindow;
}

namespace gfx {
class Point;
}

namespace chromeos {

// PrimaryDisplaySwitchObserver observes the change of the primary/secondary
// displays and store the current primary display ID into the local preference.
class PrimaryDisplaySwitchObserver : public aura::RootWindowObserver,
                                     public aura::WindowObserver {
 public:
  PrimaryDisplaySwitchObserver();
  virtual ~PrimaryDisplaySwitchObserver();

 protected:
  // aura::RootWindowObserver overrides:
  virtual void OnRootWindowMoved(const aura::RootWindow* root_window,
                                 const gfx::Point& new_origin) OVERRIDE;
  // aura::WindowObserver overrides:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  aura::RootWindow* primary_root_;

  DISALLOW_COPY_AND_ASSIGN(PrimaryDisplaySwitchObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DISPLAY_PRIMARY_DISPLAY_SWITCH_OBSERVER_H_
