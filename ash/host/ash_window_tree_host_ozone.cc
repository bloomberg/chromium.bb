// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host.h"

#include "ash/host/root_window_transformer.h"
#include "base/command_line.h"
#include "ui/aura/window_tree_host_ozone.h"
#include "ui/gfx/geometry/insets.h"

namespace ash {
namespace {

class AshWindowTreeHostOzone : public AshWindowTreeHost,
                               public aura::WindowTreeHostOzone {
 public:
  explicit AshWindowTreeHostOzone(const gfx::Rect& initial_bounds)
      : aura::WindowTreeHostOzone(initial_bounds) {}
  virtual ~AshWindowTreeHostOzone() {}

 private:
  // AshWindowTreeHost:
  virtual void ToggleFullScreen() OVERRIDE { NOTIMPLEMENTED(); }
  virtual bool ConfineCursorToRootWindow() OVERRIDE { return false; }
  virtual void UnConfineCursor() OVERRIDE { NOTIMPLEMENTED(); }
  virtual void SetRootWindowTransformer(
      scoped_ptr<RootWindowTransformer> transformer) OVERRIDE {}
  virtual aura::WindowTreeHost* AsWindowTreeHost() OVERRIDE { return this; }

  DISALLOW_COPY_AND_ASSIGN(AshWindowTreeHostOzone);
};

}  // namespace

AshWindowTreeHost* AshWindowTreeHost::Create(const gfx::Rect& initial_bounds) {
  return new AshWindowTreeHostOzone(initial_bounds);
}

}  // namespace ash
