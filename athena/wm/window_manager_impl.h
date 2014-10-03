// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_WM_WINDOW_MANAGER_IMPL_H_
#define ATHENA_WM_WINDOW_MANAGER_IMPL_H_

#include "athena/athena_export.h"
#include "athena/input/public/accelerator_manager.h"
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
class WindowListProvider;
class WindowManagerObserver;

class ATHENA_EXPORT WindowManagerImpl : public WindowManager,
                                        public WindowOverviewModeDelegate,
                                        public aura::WindowObserver,
                                        public AcceleratorHandler,
                                        public TitleDragControllerDelegate {
 public:
  WindowManagerImpl();
  virtual ~WindowManagerImpl();

  void ToggleSplitView();

  // WindowManager:
  virtual void ToggleOverview() override;
  virtual bool IsOverviewModeActive() override;

 private:
  friend class test::WindowManagerImplTestApi;
  friend class AthenaContainerLayoutManager;

  enum Command {
    CMD_TOGGLE_OVERVIEW,
    CMD_TOGGLE_SPLIT_VIEW,
  };

  // Sets whether overview mode is active.
  void SetInOverview(bool active);

  void InstallAccelerators();

  // WindowManager:
  virtual void AddObserver(WindowManagerObserver* observer) override;
  virtual void RemoveObserver(WindowManagerObserver* observer) override;
  virtual void ToggleSplitViewForTest() override;
  virtual WindowListProvider* GetWindowListProvider() override;

  // WindowOverviewModeDelegate:
  virtual void OnSelectWindow(aura::Window* window) override;
  virtual void OnSelectSplitViewWindow(aura::Window* left,
                                       aura::Window* right,
                                       aura::Window* to_activate) override;

  // aura::WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) override;

  // AcceleratorHandler:
  virtual bool IsCommandEnabled(int command_id) const override;
  virtual bool OnAcceleratorFired(int command_id,
                                  const ui::Accelerator& accelerator) override;

  // TitleDragControllerDelegate:
  virtual aura::Window* GetWindowBehind(aura::Window* window) override;
  virtual void OnTitleDragStarted(aura::Window* window) override;
  virtual void OnTitleDragCompleted(aura::Window* window) override;
  virtual void OnTitleDragCanceled(aura::Window* window) override;

  scoped_ptr<aura::Window> container_;
  scoped_ptr<WindowListProvider> window_list_provider_;
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
