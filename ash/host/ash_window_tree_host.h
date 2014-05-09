// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_ASH_WINDOW_TREE_HOST_H_
#define ASH_HOST_ASH_WINDOW_TREE_HOST_H_

#include "ash/ash_export.h"
#include "base/memory/scoped_ptr.h"

namespace aura {
class Window;
class WindowTreeHost;
}

namespace gfx {
class Rect;
}

namespace ash {
class RootWindowTransformer;

class ASH_EXPORT AshWindowTreeHost {
 public:
  // Creates a new AshWindowTreeHost. The caller owns the returned value.
  static AshWindowTreeHost* Create(const gfx::Rect& initial_bounds);

  virtual ~AshWindowTreeHost() {}

  // Toggles the host's full screen state.
  virtual void ToggleFullScreen() = 0;

  // Clips the cursor to the bounds of the root window until UnConfineCursor().
  // We would like to be able to confine the cursor to that window. However,
  // currently, we do not have such functionality in X. So we just confine
  // to the root window. This is ok because this option is currently only
  // being used in fullscreen mode, so root_window bounds = window bounds.
  virtual bool ConfineCursorToRootWindow() = 0;
  virtual void UnConfineCursor() = 0;

  virtual void SetRootWindowTransformer(
      scoped_ptr<RootWindowTransformer> transformer) = 0;

  virtual aura::WindowTreeHost* AsWindowTreeHost() = 0;
};

}  // namespace ash

#endif  // ASH_HOST_ASH_WINDOW_TREE_HOST_H_
