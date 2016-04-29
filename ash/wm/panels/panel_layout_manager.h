// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PANELS_PANEL_LAYOUT_MANAGER_H_
#define ASH_WM_PANELS_PANEL_LAYOUT_MANAGER_H_

#include <list>
#include <memory>

#include "ash/ash_export.h"
#include "ash/shelf/shelf_icon_observer.h"
#include "ash/shell_observer.h"
#include "ash/wm/common/shelf/wm_shelf_observer.h"
#include "ash/wm/common/window_state_observer.h"
#include "ash/wm/common/wm_activation_observer.h"
#include "ash/wm/common/wm_display_observer.h"
#include "ash/wm/common/wm_layout_manager.h"
#include "ash/wm/common/wm_overview_mode_observer.h"
#include "ash/wm/common/wm_root_window_controller_observer.h"
#include "ash/wm/common/wm_window_observer.h"
#include "ash/wm/common/wm_window_tracker.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_observer.h"

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
class PanelCalloutWidget;
class Shelf;
class ShelfLayoutManager;

namespace wm {
class WmRootWindowController;
class WmShelf;
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
    : public wm::WmLayoutManager,
      public ShelfIconObserver,
      public wm::WindowStateObserver,
      public wm::WmActivationObserver,
      public wm::WmDisplayObserver,
      public wm::WmOverviewModeObserver,
      public wm::WmRootWindowControllerObserver,
      public wm::WmWindowObserver,
      public keyboard::KeyboardControllerObserver,
      public wm::WmShelfObserver {
 public:
  explicit PanelLayoutManager(wm::WmWindow* panel_container);
  ~PanelLayoutManager() override;

  // Returns the PanelLayoutManager in the specified hierarchy. This searches
  // from the root of |window|.
  static PanelLayoutManager* Get(wm::WmWindow* window);

  // Call Shutdown() before deleting children of panel_container.
  void Shutdown();

  void StartDragging(wm::WmWindow* panel);
  void FinishDragging();

  void ToggleMinimize(wm::WmWindow* panel);

  // Hide / Show the panel callout widgets.
  void SetShowCalloutWidgets(bool show);

  // Returns the callout widget (arrow) for |panel|.
  views::Widget* GetCalloutWidgetForPanel(wm::WmWindow* panel);

  wm::WmShelf* shelf() { return shelf_; }
  void SetShelf(wm::WmShelf* shelf);

  // Overridden from wm::WmLayoutManager
  void OnWindowResized() override;
  void OnWindowAddedToLayout(wm::WmWindow* child) override;
  void OnWillRemoveWindowFromLayout(wm::WmWindow* child) override;
  void OnWindowRemovedFromLayout(wm::WmWindow* child) override;
  void OnChildWindowVisibilityChanged(wm::WmWindow* child,
                                      bool visibile) override;
  void SetChildBounds(wm::WmWindow* child,
                      const gfx::Rect& requested_bounds) override;

  // Overridden from wm::WmOverviewModeObserver
  void OnOverviewModeEnded() override;

  // Overridden from wm::WmRootWindowControllerObserver
  void OnShelfAlignmentChanged() override;

  // Overridden from wm::WmWindowObserver
  void OnWindowPropertyChanged(wm::WmWindow* window,
                               wm::WmWindowProperty property,
                               intptr_t old) override;

  // Overridden from wm::WindowStateObserver
  void OnPostWindowStateTypeChange(wm::WindowState* window_state,
                                   wm::WindowStateType old_type) override;

  // Overridden from wm::WmActivationObserver
  void OnWindowActivated(wm::WmWindow* gained_active,
                         wm::WmWindow* lost_active) override;

  // Overridden from WindowTreeHostManager::Observer
  void OnDisplayConfigurationChanged() override;

  // Overridden from wm::WmShelfObserver
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

    bool operator==(const wm::WmWindow* other_window) const {
      return window == other_window;
    }

    // Returns |callout_widget| as a widget. Used by tests.
    views::Widget* CalloutWidget();

    // A weak pointer to the panel window.
    wm::WmWindow* window;

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

  void MinimizePanel(wm::WmWindow* panel);
  void RestorePanel(wm::WmWindow* panel);

  // Called whenever the panel layout might change.
  void Relayout();

  // Called whenever the panel stacking order needs to be updated (e.g. focus
  // changes or a panel is moved).
  void UpdateStacking(wm::WmWindow* active_panel);

  // Update the callout arrows for all managed panels.
  void UpdateCallouts();

  // Overridden from keyboard::KeyboardControllerObserver:
  void OnKeyboardBoundsChanging(const gfx::Rect& keyboard_bounds) override;

  // Parent window associated with this layout manager.
  wm::WmWindow* panel_container_;

  wm::WmRootWindowController* root_window_controller_;

  // Protect against recursive calls to OnWindowAddedToLayout().
  bool in_add_window_;
  // Protect against recursive calls to Relayout().
  bool in_layout_;
  // Indicates if the panel callout widget should be created.
  bool show_callout_widgets_;
  // Ordered list of unowned pointers to panel windows.
  PanelList panel_windows_;
  // The panel being dragged.
  wm::WmWindow* dragged_panel_;
  // The shelf we are observing for shelf icon changes.
  wm::WmShelf* shelf_;

  // When not NULL, the shelf is hidden (i.e. full screen) and this tracks the
  // set of panel windows which have been temporarily hidden and need to be
  // restored when the shelf becomes visible again.
  std::unique_ptr<wm::WmWindowTracker> restore_windows_on_shelf_visible_;

  // The last active panel. Used to maintain stacking order even if no panels
  // are currently focused.
  wm::WmWindow* last_active_panel_;
  base::WeakPtrFactory<PanelLayoutManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PanelLayoutManager);
};

}  // namespace ash

#endif  // ASH_WM_PANELS_PANEL_LAYOUT_MANAGER_H_
