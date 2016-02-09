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

namespace ui {
class EventSource;
class KeyEvent;
class LocatedEvent;
}

namespace ash {
struct AshWindowTreeHostInitParams;
class InputMethodEventHandler;
class RootWindowTransformer;

class ASH_EXPORT AshWindowTreeHost {
 public:
  AshWindowTreeHost();
  virtual ~AshWindowTreeHost() {}

  // Creates a new AshWindowTreeHost. The caller owns the returned value.
  static AshWindowTreeHost* Create(
      const AshWindowTreeHostInitParams& init_params);

  void set_input_method_handler(InputMethodEventHandler* input_method_handler) {
    input_method_handler_ = input_method_handler;
  }

  InputMethodEventHandler* input_method_handler() {
    return input_method_handler_;
  }

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

  // Stop listening for events in preparation for shutdown.
  virtual void PrepareForShutdown() {}

  virtual void RegisterMirroringHost(AshWindowTreeHost* mirroring_ash_host) {}

 protected:
  // Translates the native mouse location into screen coordinates.
  void TranslateLocatedEvent(ui::LocatedEvent* event);

 private:
  InputMethodEventHandler* input_method_handler_;
};

}  // namespace ash

#endif  // ASH_HOST_ASH_WINDOW_TREE_HOST_H_
