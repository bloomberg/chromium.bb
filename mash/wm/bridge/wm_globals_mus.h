// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_BRIDGE_WM_GLOBALS_MUS_H_
#define MASH_WM_BRIDGE_WM_GLOBALS_MUS_H_

#include <stdint.h>

#include <vector>

#include "ash/wm/common/wm_globals.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "components/mus/public/cpp/window_tree_connection_observer.h"

namespace mus {
class WindowTreeConnection;
}

namespace mash {
namespace wm {

class WmRootWindowControllerMus;
class WmWindowMus;

// WmGlobals implementation for mus.
class WmGlobalsMus : public ash::wm::WmGlobals,
                     public mus::WindowTreeConnectionObserver {
 public:
  explicit WmGlobalsMus(mus::WindowTreeConnection* connection);
  ~WmGlobalsMus() override;

  static WmGlobalsMus* Get();

  void AddRootWindowController(WmRootWindowControllerMus* controller);
  void RemoveRootWindowController(WmRootWindowControllerMus* controller);

  // Returns the ancestor of |window| (including |window|) that is considered
  // toplevel. |window| may be null.
  static WmWindowMus* GetToplevelAncestor(mus::Window* window);

  WmRootWindowControllerMus* GetRootWindowControllerWithDisplayId(int64_t id);

  // WmGlobals:
  ash::wm::WmWindow* GetFocusedWindow() override;
  ash::wm::WmWindow* GetActiveWindow() override;
  ash::wm::WmWindow* GetRootWindowForDisplayId(int64_t display_id) override;
  ash::wm::WmWindow* GetRootWindowForNewWindows() override;
  std::vector<ash::wm::WmWindow*> GetMruWindowListIgnoreModals() override;
  bool IsForceMaximizeOnFirstRun() override;
  bool IsUserSessionBlocked() override;
  bool IsScreenLocked() override;
  void LockCursor() override;
  void UnlockCursor() override;
  std::vector<ash::wm::WmWindow*> GetAllRootWindows() override;
  void RecordUserMetricsAction(ash::wm::WmUserMetricsAction action) override;
  std::unique_ptr<ash::WindowResizer> CreateDragWindowResizer(
      std::unique_ptr<ash::WindowResizer> next_window_resizer,
      ash::wm::WindowState* window_state) override;
  bool IsOverviewModeSelecting() override;
  bool IsOverviewModeRestoringMinimizedWindows() override;
  void AddActivationObserver(ash::wm::WmActivationObserver* observer) override;
  void RemoveActivationObserver(
      ash::wm::WmActivationObserver* observer) override;
  void AddDisplayObserver(ash::wm::WmDisplayObserver* observer) override;
  void RemoveDisplayObserver(ash::wm::WmDisplayObserver* observer) override;
  void AddOverviewModeObserver(
      ash::wm::WmOverviewModeObserver* observer) override;
  void RemoveOverviewModeObserver(
      ash::wm::WmOverviewModeObserver* observer) override;

 private:
  // Returns true if |window| is a window that can have active children.
  static bool IsActivationParent(mus::Window* window);

  // mus::WindowTreeConnectionObserver:
  void OnWindowTreeFocusChanged(mus::Window* gained_focus,
                                mus::Window* lost_focus) override;

  mus::WindowTreeConnection* connection_;

  std::vector<WmRootWindowControllerMus*> root_window_controllers_;

  base::ObserverList<ash::wm::WmActivationObserver> activation_observers_;

  DISALLOW_COPY_AND_ASSIGN(WmGlobalsMus);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_BRIDGE_WM_GLOBALS_MUS_H_
