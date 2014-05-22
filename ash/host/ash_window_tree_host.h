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
class Insets;
class Rect;
}

namespace ash {
struct AshWindowTreeHostInitParams;
class RootWindowTransformer;

class ASH_EXPORT AshWindowTreeHost {
 public:
  virtual ~AshWindowTreeHost() {}

  // Creates a new AshWindowTreeHost. The caller owns the returned value.
  static AshWindowTreeHost* Create(
      const AshWindowTreeHostInitParams& init_params);

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

  virtual gfx::Insets GetHostInsets() const = 0;

  virtual aura::WindowTreeHost* AsWindowTreeHost() = 0;

  // Updates the display IDs associated with this root window.
  // A root window can be associated with up to 2 display IDs (e.g. in mirror
  // mode dual monitors case). If the root window is only associated with one
  // display id, then the other id should be set to
  // gfx::Display::kInvalidDisplayID.
  virtual void UpdateDisplayID(int64 id1, int64 id2) {}

  // Stop listening for events in preparation for shutdown.
  virtual void PrepareForShutdown() {}
};

}  // namespace ash

#endif  // ASH_HOST_ASH_WINDOW_TREE_HOST_H_
