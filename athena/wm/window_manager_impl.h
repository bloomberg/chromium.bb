// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_WM_WINDOW_MANAGER_IMPL_H_
#define ATHENA_WM_WINDOW_MANAGER_IMPL_H_

#include "athena/athena_export.h"
#include "athena/input/public/accelerator_manager.h"
#include "athena/wm/public/window_list_provider_observer.h"
#include "athena/wm/public/window_manager.h"
#include "athena/wm/title_drag_controller.h"
#include "athena/wm/window_overview_mode.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "ui/aura/window_observer.h"

namespace wm {
class ShadowController;
class WMState;
}

namespace athena {

namespace test {
class WindowManagerImplTestApi;
}

class BezelController;
class SplitViewController;
class WindowListProviderImpl;
class WindowManagerObserver;

class ATHENA_EXPORT WindowManagerImpl : public WindowManager,
                                        public WindowOverviewModeDelegate,
                                        public WindowListProviderObserver,
                                        public aura::WindowObserver,
                                        public AcceleratorHandler,
                                        public TitleDragControllerDelegate {
 public:
  WindowManagerImpl();
  ~WindowManagerImpl() override;

  void ToggleSplitView();

  // WindowManager:
  void EnterOverview() override;
  // Exits overview and activates the previously active activity
  void ExitOverview() override;
  bool IsOverviewModeActive() override;

 private:
  friend class test::WindowManagerImplTestApi;
  friend class AthenaContainerLayoutManager;

  enum Command {
    CMD_EXIT_OVERVIEW,
    CMD_TOGGLE_OVERVIEW,
    CMD_TOGGLE_SPLIT_VIEW,
  };

  const AcceleratorData kEscAcceleratorData = {TRIGGER_ON_PRESS,
                                               ui::VKEY_ESCAPE,
                                               ui::EF_NONE,
                                               CMD_EXIT_OVERVIEW,
                                               AF_NONE};

  // Exits overview mode without changing activation.  The caller should
  // ensure that a window is active after exiting overview mode.
  void ExitOverviewNoActivate();

  void InstallAccelerators();

  // WindowManager:
  void AddObserver(WindowManagerObserver* observer) override;
  void RemoveObserver(WindowManagerObserver* observer) override;
  void ToggleSplitViewForTest() override;
  WindowListProvider* GetWindowListProvider() override;

  // WindowOverviewModeDelegate:
  void OnSelectWindow(aura::Window* window) override;
  void OnSelectSplitViewWindow(aura::Window* left,
                               aura::Window* right,
                               aura::Window* to_activate) override;

  // WindowListProviderObserver:
  void OnWindowStackingChangedInList() override;
  void OnWindowAddedToList(aura::Window* window) override;
  void OnWindowRemovedFromList(aura::Window* removed_window,
                               int index) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // AcceleratorHandler:
  bool IsCommandEnabled(int command_id) const override;
  bool OnAcceleratorFired(int command_id,
                          const ui::Accelerator& accelerator) override;

  // TitleDragControllerDelegate:
  aura::Window* GetWindowBehind(aura::Window* window) override;
  void OnTitleDragStarted(aura::Window* window) override;
  void OnTitleDragCompleted(aura::Window* window) override;
  void OnTitleDragCanceled(aura::Window* window) override;

  scoped_ptr<aura::Window> container_;
  scoped_ptr<WindowListProviderImpl> window_list_provider_;
  scoped_ptr<WindowOverviewMode> overview_;
  scoped_ptr<BezelController> bezel_controller_;
  scoped_ptr<SplitViewController> split_view_controller_;
  scoped_ptr<wm::WMState> wm_state_;
  scoped_ptr<TitleDragController> title_drag_controller_;
  scoped_ptr<wm::ShadowController> shadow_controller_;
  ObserverList<WindowManagerObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerImpl);
};

}  // namespace athena

#endif  // ATHENA_WM_WINDOW_MANAGER_IMPL_H_
