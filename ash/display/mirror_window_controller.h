// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_MIRROR_WINDOW_CONTROLLER_H_
#define ASH_DISPLAY_MIRROR_WINDOW_CONTROLLER_H_

#include <map>
#include <vector>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace aura {
class Window;
}

namespace ui {
class Reflector;
}

namespace ash {
class AshWindowTreeHost;
class DisplayInfo;
class RootWindowTransformer;

namespace test{
class MirrorWindowTestApi;
}

// An object that copies the content of the primary root window to a
// mirror window. This also draws a mouse cursor as the mouse cursor
// is typically drawn by the window system.
class ASH_EXPORT MirrorWindowController : public aura::WindowTreeHostObserver {
 public:
  MirrorWindowController();
  ~MirrorWindowController() override;

  // Updates the root window's bounds using |display_info|.
  // Creates the new root window if one doesn't exist.
  void UpdateWindow(const std::vector<DisplayInfo>& display_info);

  // Same as above, but using existing display info
  // for the mirrored display.
  void UpdateWindow();

  // Close the mirror window.
  void Close();

  // aura::WindowTreeHostObserver overrides:
  void OnHostResized(const aura::WindowTreeHost* host) override;

  // Return the root window used to mirror the content. NULL if the
  // display is not mirrored by the compositor path.
  aura::Window* GetWindow();

 private:
  friend class test::MirrorWindowTestApi;

  struct MirroringHostInfo {
    MirroringHostInfo();
    ~MirroringHostInfo();
    scoped_ptr<AshWindowTreeHost> ash_host;
    gfx::Size mirror_window_host_size;
    aura::Window* mirror_window = nullptr;
  };

  void CloseAndDeleteHost(MirroringHostInfo* host_info);

  // Creates a RootWindowTransformer for current display
  // configuration.
  scoped_ptr<RootWindowTransformer> CreateRootWindowTransformer() const;

  typedef std::map<int64_t, MirroringHostInfo*> MirroringHostInfoMap;
  MirroringHostInfoMap mirroring_host_info_map_;

  scoped_ptr<ui::Reflector> reflector_;

  DISALLOW_COPY_AND_ASSIGN(MirrorWindowController);
};

}  // namespace ash

#endif  // ASH_DISPLAY_MIRROR_WINDOW_CONTROLLER_H_
