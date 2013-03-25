// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/root_window_host_factory.h"

#include "ash/ash_switches.h"
#include "base/command_line.h"
#include "base/win/windows_version.h"
#include "ui/aura/remote_root_window_host_win.h"
#include "ui/aura/root_window_host.h"

namespace {

class RootWindowHostFactoryImpl : public ash::RootWindowHostFactory {
 public:
  RootWindowHostFactoryImpl() {}

  // Overridden from RootWindowHostFactory:
  virtual aura::RootWindowHost* CreateRootWindowHost(
      const gfx::Rect& initial_bounds) OVERRIDE {
    if (base::win::GetVersion() >= base::win::VERSION_WIN8 &&
        !CommandLine::ForCurrentProcess()->HasSwitch(
            ash::switches::kForceAshToDesktop))
      return aura::RemoteRootWindowHostWin::Create(initial_bounds);

    return aura::RootWindowHost::Create(initial_bounds);
  }
};

}

namespace ash {

// static
RootWindowHostFactory* RootWindowHostFactory::Create() {
  return new RootWindowHostFactoryImpl;
}

}  // namespace ash
