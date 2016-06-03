// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BRIDGE_WM_GLOBALS_MUS_H_
#define ASH_MUS_BRIDGE_WM_GLOBALS_MUS_H_

#include <stdint.h>

#include <vector>

#include "ash/common/wm/wm_globals.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "components/mus/public/cpp/window_tree_client_observer.h"

namespace mus {
class WindowTreeClient;
}

namespace ash {
namespace mus {

class WmRootWindowControllerMus;
class WmWindowMus;

// WmGlobals implementation for mus.
class WmGlobalsMus : public wm::WmGlobals,
                     public ::mus::WindowTreeClientObserver {
 public:
  explicit WmGlobalsMus(::mus::WindowTreeClient* client);
  ~WmGlobalsMus() override;

  static WmGlobalsMus* Get();

  void AddRootWindowController(WmRootWindowControllerMus* controller);
  void RemoveRootWindowController(WmRootWindowControllerMus* controller);

  // Returns the ancestor of |window| (including |window|) that is considered
  // toplevel. |window| may be null.
  static WmWindowMus* GetToplevelAncestor(::mus::Window* window);

  WmRootWindowControllerMus* GetRootWindowControllerWithDisplayId(int64_t id);

  // WmGlobals:
  wm::WmWindow* NewContainerWindow() override;
  wm::WmWindow* GetFocusedWindow() override;
  wm::WmWindow* GetActiveWindow() override;
  wm::WmWindow* GetPrimaryRootWindow() override;
  wm::WmWindow* GetRootWindowForDisplayId(int64_t display_id) override;
  wm::WmWindow* GetRootWindowForNewWindows() override;
  std::vector<wm::WmWindow*> GetMruWindowList() override;
  std::vector<wm::WmWindow*> GetMruWindowListIgnoreModals() override;
  bool IsForceMaximizeOnFirstRun() override;
  bool IsUserSessionBlocked() override;
  bool IsScreenLocked() override;
  void LockCursor() override;
  void UnlockCursor() override;
  std::vector<wm::WmWindow*> GetAllRootWindows() override;
  void RecordUserMetricsAction(wm::WmUserMetricsAction action) override;
  std::unique_ptr<WindowResizer> CreateDragWindowResizer(
      std::unique_ptr<WindowResizer> next_window_resizer,
      wm::WindowState* window_state) override;
  bool IsOverviewModeSelecting() override;
  bool IsOverviewModeRestoringMinimizedWindows() override;
  void AddActivationObserver(wm::WmActivationObserver* observer) override;
  void RemoveActivationObserver(wm::WmActivationObserver* observer) override;
  void AddDisplayObserver(wm::WmDisplayObserver* observer) override;
  void RemoveDisplayObserver(wm::WmDisplayObserver* observer) override;
  void AddOverviewModeObserver(wm::WmOverviewModeObserver* observer) override;
  void RemoveOverviewModeObserver(
      wm::WmOverviewModeObserver* observer) override;

 private:
  // Returns true if |window| is a window that can have active children.
  static bool IsActivationParent(::mus::Window* window);

  void RemoveClientObserver();

  // ::mus::WindowTreeClientObserver:
  void OnWindowTreeFocusChanged(::mus::Window* gained_focus,
                                ::mus::Window* lost_focus) override;
  void OnWillDestroyClient(::mus::WindowTreeClient* client) override;

  ::mus::WindowTreeClient* client_;

  std::vector<WmRootWindowControllerMus*> root_window_controllers_;

  base::ObserverList<wm::WmActivationObserver> activation_observers_;

  DISALLOW_COPY_AND_ASSIGN(WmGlobalsMus);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_WM_GLOBALS_MUS_H_
