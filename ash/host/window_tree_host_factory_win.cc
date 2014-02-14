// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/window_tree_host_factory.h"

#include "ash/ash_switches.h"
#include "base/command_line.h"
#include "base/win/windows_version.h"
#include "ui/aura/remote_window_tree_host_win.h"
#include "ui/aura/window_tree_host.h"

namespace {

class WindowTreeHostFactoryImpl : public ash::WindowTreeHostFactory {
 public:
  WindowTreeHostFactoryImpl() {}

  // Overridden from WindowTreeHostFactory:
  virtual aura::WindowTreeHost* CreateWindowTreeHost(
      const gfx::Rect& initial_bounds) OVERRIDE {
    if (base::win::GetVersion() >= base::win::VERSION_WIN8 &&
        !CommandLine::ForCurrentProcess()->HasSwitch(
            ash::switches::kForceAshToDesktop))
      return aura::RemoteWindowTreeHostWin::Create(initial_bounds);

    return aura::WindowTreeHost::Create(initial_bounds);
  }
};

}

namespace ash {

// static
WindowTreeHostFactory* WindowTreeHostFactory::Create() {
  return new WindowTreeHostFactoryImpl;
}

}  // namespace ash
