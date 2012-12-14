// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/root_window_host_factory.h"

#include "ui/aura/root_window_host.h"

namespace {

class RootWindowHostFactoryImpl : public ash::RootWindowHostFactory {
 public:
  RootWindowHostFactoryImpl() {}

  // Overridden from RootWindowHostFactory:
  virtual aura::RootWindowHost* CreateRootWindowHost(
      const gfx::Rect& initial_bounds) OVERRIDE {
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
