// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/window_tree_host_factory.h"

#include "ui/aura/window_tree_host.h"

namespace {

class WindowTreeHostFactoryImpl : public ash::WindowTreeHostFactory {
 public:
  WindowTreeHostFactoryImpl() {}

  // Overridden from WindowTreeHostFactory:
  virtual aura::WindowTreeHost* CreateWindowTreeHost(
      const gfx::Rect& initial_bounds) OVERRIDE {
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
