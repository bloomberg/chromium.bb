// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_MIRROR_WINDOW_CONTROLLER_H_
#define ASH_DISPLAY_MIRROR_WINDOW_CONTROLLER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace aura {
class Window;
namespace client {
class ScreenPositionClient;
}
}

namespace display {
class Display;
class ManagedDisplayInfo;
}

namespace ui {
class Reflector;
}

namespace ash {
class AshWindowTreeHost;

namespace test {
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
  void UpdateWindow(
      const std::vector<display::ManagedDisplayInfo>& display_info);

  // Same as above, but using existing display info
  // for the mirrored display.
  void UpdateWindow();

  // Close the mirror window if they're not necessary any longer.
  void CloseIfNotNecessary();

  // aura::WindowTreeHostObserver overrides:
  void OnHostResized(const aura::WindowTreeHost* host) override;

  // Return the root window used to mirror the content. NULL if the
  // display is not mirrored by the compositor path.
  aura::Window* GetWindow();

  // Returns the display::Display for the mirroring root window.
  display::Display GetDisplayForRootWindow(const aura::Window* root) const;

  // Returns the AshWindwoTreeHost created for |display_id|.
  AshWindowTreeHost* GetAshWindowTreeHostForDisplayId(int64_t display_id);

  // Returns all root windows hosting mirroring displays.
  aura::Window::Windows GetAllRootWindows() const;

 private:
  friend class test::MirrorWindowTestApi;

  struct MirroringHostInfo;

  // Close the mirror window. When |delay_host_deletion| is true, the window
  // tree host will be deleted in an another task on UI thread. This is
  // necessary to safely delete the WTH that is currently handling input events.
  void Close(bool delay_host_deletion);

  void CloseAndDeleteHost(MirroringHostInfo* host_info,
                          bool delay_host_deletion);

  typedef std::map<int64_t, MirroringHostInfo*> MirroringHostInfoMap;
  MirroringHostInfoMap mirroring_host_info_map_;

  display::DisplayManager::MultiDisplayMode multi_display_mode_;

  std::unique_ptr<aura::client::ScreenPositionClient> screen_position_client_;

  std::unique_ptr<ui::Reflector> reflector_;

  DISALLOW_COPY_AND_ASSIGN(MirrorWindowController);
};

}  // namespace ash

#endif  // ASH_DISPLAY_MIRROR_WINDOW_CONTROLLER_H_
