// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_PANELS_PANEL_LAYOUT_MANAGER_H_
#define ASH_COMMON_WM_PANELS_PANEL_LAYOUT_MANAGER_H_

#include <list>
#include <memory>

#include "ash/ash_export.h"
#include "ash/common/shelf/wm_shelf_observer.h"
#include "ash/common/shell_observer.h"
#include "ash/common/wm/window_state_observer.h"
#include "ash/common/wm_activation_observer.h"
#include "ash/common/wm_display_observer.h"
#include "ash/common/wm_layout_manager.h"
#include "ash/common/wm_window_observer.h"
#include "ash/common/wm_window_tracker.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_observer.h"

namespace gfx {
class Rect;
}

namespace views {
class Widget;
}

namespace ash {
class PanelCalloutWidget;
class WmShelf;

namespace wm {
class WmRootWindowController;
}

// PanelLayoutManager is responsible for organizing panels within the
// workspace. It is associated with a specific container window (i.e.
// kShellWindowId_PanelContainer) and controls the layout of any windows
// added to that container.
//
// The constructor takes a |panel_container| argument which is expected to set
// its layout manager to this instance, e.g.:
// panel_container->SetLayoutManager(new PanelLayoutManager(panel_container));

class ASH_EXPORT PanelLayoutManager
    : public WmLayoutManager,
      public wm::WindowStateObserver,
      public WmActivationObserver,
      public WmDisplayObserver,
      public ShellObserver,
      public WmWindowObserver,
      public keyboard::KeyboardControllerObserver,
      public WmShelfObserver {
 public:
  explicit PanelLayoutManager(WmWindow* panel_container);
  ~PanelLayoutManager() override;

  // Returns the PanelLayoutManager in the specified hierarchy. This searches
  // from the root of |window|.
  static PanelLayoutManager* Get(WmWindow* window);

  // Call Shutdown() before deleting children of panel_container.
  void Shutdown();

  void StartDragging(WmWindow* panel);
  void FinishDragging();

  void ToggleMinimize(WmWindow* panel);

  // Hide / Show the panel callout widgets.
  void SetShowCalloutWidgets(bool show);

  // Returns the callout widget (arrow) for |panel|.
  views::Widget* GetCalloutWidgetForPanel(WmWindow* panel);

  WmShelf* shelf() { return shelf_; }
  void SetShelf(WmShelf* shelf);

  // Overridden from WmLayoutManager
  void OnWindowResized() override;
  void OnWindowAddedToLayout(WmWindow* child) override;
  void OnWillRemoveWindowFromLayout(WmWindow* child) override;
  void OnWindowRemovedFromLayout(WmWindow* child) override;
  void OnChildWindowVisibilityChanged(WmWindow* child, bool visibile) override;
  void SetChildBounds(WmWindow* child,
                      const gfx::Rect& requested_bounds) override;

  // Overridden from ShellObserver:
  void OnOverviewModeEnded() override;
  void OnShelfAlignmentChanged(WmWindow* root_window) override;

  // Overridden from WmWindowObserver
  void OnWindowPropertyChanged(WmWindow* window,
                               WmWindowProperty property) override;

  // Overridden from wm::WindowStateObserver
  void OnPostWindowStateTypeChange(wm::WindowState* window_state,
                                   wm::WindowStateType old_type) override;

  // Overridden from WmActivationObserver
  void OnWindowActivated(WmWindow* gained_active,
                         WmWindow* lost_active) override;

  // Overridden from WindowTreeHostManager::Observer
  void OnDisplayConfigurationChanged() override;

  // Overridden from WmShelfObserver
  void WillChangeVisibilityState(ShelfVisibilityState new_state) override;
  void OnShelfIconPositionsChanged() override;

 private:
  friend class PanelLayoutManagerTest;
  friend class PanelWindowResizerTest;
  friend class DockedWindowResizerTest;
  friend class DockedWindowLayoutManagerTest;
  friend class WorkspaceControllerTest;
  friend class AcceleratorControllerTest;

  views::Widget* CreateCalloutWidget();

  struct ASH_EXPORT PanelInfo {
    PanelInfo() : window(NULL), callout_widget(NULL), slide_in(false) {}

    bool operator==(const WmWindow* other_window) const {
      return window == other_window;
    }

    // Returns |callout_widget| as a widget. Used by tests.
    views::Widget* CalloutWidget();

    // A weak pointer to the panel window.
    WmWindow* window;

    // The callout widget for this panel. This pointer must be managed
    // manually as this structure is used in a std::list. See
    // http://www.chromium.org/developers/smart-pointer-guidelines
    PanelCalloutWidget* callout_widget;

    // True on new and restored panel windows until the panel has been
    // positioned. The first time Relayout is called the panel will be shown,
    // and slide into position and this will be set to false.
    bool slide_in;
  };

  typedef std::list<PanelInfo> PanelList;

  void MinimizePanel(WmWindow* panel);
  void RestorePanel(WmWindow* panel);

  // Called whenever the panel layout might change.
  void Relayout();

  // Called whenever the panel stacking order needs to be updated (e.g. focus
  // changes or a panel is moved).
  void UpdateStacking(WmWindow* active_panel);

  // Update the callout arrows for all managed panels.
  void UpdateCallouts();

  // Overridden from keyboard::KeyboardControllerObserver:
  void OnKeyboardBoundsChanging(const gfx::Rect& keyboard_bounds) override;
  void OnKeyboardClosed() override;

  // Parent window associated with this layout manager.
  WmWindow* panel_container_;

  WmRootWindowController* root_window_controller_;

  // Protect against recursive calls to OnWindowAddedToLayout().
  bool in_add_window_;
  // Protect against recursive calls to Relayout().
  bool in_layout_;
  // Indicates if the panel callout widget should be created.
  bool show_callout_widgets_;
  // Ordered list of unowned pointers to panel windows.
  PanelList panel_windows_;
  // The panel being dragged.
  WmWindow* dragged_panel_;
  // The shelf we are observing for shelf icon changes.
  WmShelf* shelf_;

  // When not NULL, the shelf is hidden (i.e. full screen) and this tracks the
  // set of panel windows which have been temporarily hidden and need to be
  // restored when the shelf becomes visible again.
  std::unique_ptr<WmWindowTracker> restore_windows_on_shelf_visible_;

  // The last active panel. Used to maintain stacking order even if no panels
  // are currently focused.
  WmWindow* last_active_panel_;
  base::WeakPtrFactory<PanelLayoutManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PanelLayoutManager);
};

}  // namespace ash

#endif  // ASH_COMMON_WM_PANELS_PANEL_LAYOUT_MANAGER_H_
