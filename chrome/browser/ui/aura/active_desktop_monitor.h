// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AURA_ACTIVE_DESKTOP_MONITOR_H_
#define CHROME_BROWSER_UI_AURA_ACTIVE_DESKTOP_MONITOR_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/host_desktop.h"
#include "ui/aura/env_observer.h"

// Tracks the most-recently activated host desktop type by observing
// WindowTreeHost activations.
class ActiveDesktopMonitor : public aura::EnvObserver {
 public:
  // Constructs an ActiveDesktopMonitor which initially uses |initial_desktop|
  // as the |last_activated_desktop_| until a root window is activated.
  explicit ActiveDesktopMonitor(chrome::HostDesktopType initial_desktop);
  virtual ~ActiveDesktopMonitor();

  // Returns the host desktop type of the most-recently activated
  // WindowTreeHost. This desktop type may no longer exist (e.g., the Ash
  // desktop may have closed since being active, and no RWHs on the native
  // desktop have yet been activated).
  static chrome::HostDesktopType GetLastActivatedDesktopType();

 private:
  // Returns true if |host| is a DesktopWindowTreeHost.
  static bool IsDesktopWindow(aura::WindowTreeHost* host);

  // aura::EnvObserver methods.
  virtual void OnWindowInitialized(aura::Window* window) OVERRIDE;
  virtual void OnHostActivated(aura::WindowTreeHost* host) OVERRIDE;

  static ActiveDesktopMonitor* g_instance_;
  chrome::HostDesktopType last_activated_desktop_;

  DISALLOW_COPY_AND_ASSIGN(ActiveDesktopMonitor);
};

#endif  // CHROME_BROWSER_UI_AURA_ACTIVE_DESKTOP_MONITOR_H_
