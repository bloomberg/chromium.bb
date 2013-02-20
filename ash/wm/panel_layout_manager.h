// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PANEL_LAYOUT_MANAGER_H_
#define ASH_WM_PANEL_LAYOUT_MANAGER_H_

#include <list>

#include "ash/ash_export.h"
#include "ash/launcher/launcher_icon_observer.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/client/activation_change_observer.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace views {
class Widget;
}

namespace ash {
class Launcher;

namespace internal {

// PanelLayoutManager is responsible for organizing panels within the
// workspace. It is associated with a specific container window (i.e.
// kShellWindowId_PanelContainer) and controls the layout of any windows
// added to that container.
//
// The constructor takes a |panel_container| argument which is expected to set
// its layout manager to this instance, e.g.:
// panel_container->SetLayoutManager(new PanelLayoutManager(panel_container));

class ASH_EXPORT PanelLayoutManager
    : public aura::LayoutManager,
      public ash::LauncherIconObserver,
      public aura::WindowObserver,
      public aura::client::ActivationChangeObserver {
 public:
  explicit PanelLayoutManager(aura::Window* panel_container);
  virtual ~PanelLayoutManager();

  // Call Shutdown() before deleting children of panel_container.
  void Shutdown();

  void StartDragging(aura::Window* panel);
  void FinishDragging();

  void ToggleMinimize(aura::Window* panel);

  ash::Launcher* launcher() { return launcher_; }
  void SetLauncher(ash::Launcher* launcher);

  // Overridden from aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visibile) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

  // Overridden from ash::LauncherIconObserver
  virtual void OnLauncherIconPositionsChanged() OVERRIDE;

  // Overridden from aura::WindowObserver
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;
  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visible) OVERRIDE;

  // Overridden from aura::client::ActivationChangeObserver
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;

 private:
  friend class PanelLayoutManagerTest;
  friend class PanelWindowResizerTest;

  typedef std::list<aura::Window*> PanelList;

  void MinimizePanel(aura::Window* panel);
  void RestorePanel(aura::Window* panel);

  // Called whenever the panel layout might change.
  void Relayout();

  // Called whenever the panel stacking order needs to be updated (e.g. focus
  // changes or a panel is moved).
  void UpdateStacking(aura::Window* active_panel);

  // Trigger a delayed task to update the callout. We use this because
  // otherwise, ShadowController::OnWindowPropertyChanged may be invoked after
  // we've already updated the callout, causing the drop shadow to be stacked on
  // top of the callout rather than the other way around.
  // TODO(dcheng): Possibly a bug in the shadow controller. If a window is
  // focused but not stacked at the top, I don't think its shadow should be
  // drawn on top of "higher" windows.
  void UpdateCallout(aura::Window* active_window);

  // Don't call this directly. Only UpdateCallout() should call this method.
  void ShowCalloutHelper(aura::Window* active_panel);

  // For testing.
  views::Widget* callout_widget() const { return callout_widget_.get(); }

  // Parent window associated with this layout manager.
  aura::Window* panel_container_;
  // Protect against recursive calls to Relayout().
  bool in_layout_;
  // Ordered list of unowned pointers to panel windows.
  PanelList panel_windows_;
  // The panel being dragged.
  aura::Window* dragged_panel_;
  // The launcher we are observing for launcher icon changes.
  Launcher* launcher_;
  // The last active panel. Used to maintain stacking even if no panels are
  // currently focused.
  aura::Window* last_active_panel_;
  // Manage the callout for the focused panel, if any.
  scoped_ptr<views::Widget> callout_widget_;
  base::WeakPtrFactory<PanelLayoutManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PanelLayoutManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_PANEL_LAYOUT_MANAGER_H_
