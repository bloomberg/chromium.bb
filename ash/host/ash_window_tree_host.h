// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_ASH_WINDOW_TREE_HOST_H_
#define ASH_HOST_ASH_WINDOW_TREE_HOST_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/display/display.h"

namespace aura {
class WindowTreeHost;
}

namespace gfx {
class Insets;
}

namespace ui {
class LocatedEvent;
}

namespace ash {
struct AshWindowTreeHostInitParams;
class RootWindowTransformer;

class ASH_EXPORT AshWindowTreeHost {
 public:
  AshWindowTreeHost();
  virtual ~AshWindowTreeHost();

  static std::unique_ptr<AshWindowTreeHost> Create(
      const AshWindowTreeHostInitParams& init_params);

  // Clips the cursor to the bounds of the root window.
  virtual bool ConfineCursorToRootWindow() = 0;

  virtual void SetRootWindowTransformer(
      std::unique_ptr<RootWindowTransformer> transformer) = 0;

  virtual gfx::Insets GetHostInsets() const = 0;

  virtual aura::WindowTreeHost* AsWindowTreeHost() = 0;

  // Stop listening for events in preparation for shutdown.
  virtual void PrepareForShutdown() {}

  virtual void RegisterMirroringHost(AshWindowTreeHost* mirroring_ash_host) {}

  virtual void SetCursorConfig(const display::Display& display,
                               display::Display::Rotation rotation) = 0;
  virtual void ClearCursorConfig() = 0;

 protected:
  // Translates the native mouse location into screen coordinates.
  void TranslateLocatedEvent(ui::LocatedEvent* event);
};

}  // namespace ash

#endif  // ASH_HOST_ASH_WINDOW_TREE_HOST_H_
