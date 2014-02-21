// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_MIRROR_WINDOW_CONTROLLER_H_
#define ASH_DISPLAY_MIRROR_WINDOW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/root_window_observer.h"
#include "ui/gfx/display.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"

namespace aura {
class RootWindowTransformer;
class Window;
class WindowEventDispatcher;
}

namespace ui {
class Reflector;
}

namespace ash {
namespace test{
class MirrorWindowTestApi;
}

namespace internal {
class DisplayInfo;

// An object that copies the content of the primary root window to a
// mirror window. This also draws a mouse cursor as the mouse cursor
// is typically drawn by the window system.
class ASH_EXPORT MirrorWindowController : public aura::RootWindowObserver {
 public:
  MirrorWindowController();
  virtual ~MirrorWindowController();

  // Updates the root window's bounds using |display_info|.
  // Creates the new root window if one doesn't exist.
  void UpdateWindow(const DisplayInfo& display_info);

  // Same as above, but using existing display info
  // for the mirrored display.
  void UpdateWindow();

  // Close the mirror window.
  void Close();

  // aura::RootWindowObserver overrides:
  virtual void OnWindowTreeHostResized(
      const aura::WindowEventDispatcher* root) OVERRIDE;

  // Returns the mirror root window.
  aura::WindowEventDispatcher* dispatcher() const { return dispatcher_.get(); }

 private:
  friend class test::MirrorWindowTestApi;

  // Creates a RootWindowTransformer for current display
  // configuration.
  scoped_ptr<aura::RootWindowTransformer> CreateRootWindowTransformer() const;

  scoped_ptr<aura::WindowEventDispatcher> dispatcher_;
  gfx::Size mirror_window_host_size_;
  scoped_refptr<ui::Reflector> reflector_;

  DISALLOW_COPY_AND_ASSIGN(MirrorWindowController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_DISPLAY_MIRROR_WINDOW_CONTROLLER_H_
