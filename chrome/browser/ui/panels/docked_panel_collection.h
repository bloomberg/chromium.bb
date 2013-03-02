// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_DOCKED_PANEL_COLLECTION_H_
#define CHROME_BROWSER_UI_PANELS_DOCKED_PANEL_COLLECTION_H_

#include <list>
#include <set>
#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/panels/display_settings_provider.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_collection.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher_observer.h"
#include "ui/gfx/rect.h"

class PanelManager;

// This class manages a group of panels that could be docked to the bottom of
// screen.
class DockedPanelCollection :
    public PanelCollection,
    public PanelMouseWatcherObserver,
    public DisplaySettingsProvider::DesktopBarObserver {
 public:
  typedef std::list<Panel*> Panels;

  explicit DockedPanelCollection(PanelManager* panel_manager);
  virtual ~DockedPanelCollection();

  // PanelCollection OVERRIDES:
  virtual void OnDisplayChanged() OVERRIDE;

  // Rearranges the positions of the panels in the collection
  // and reduces their width when there is not enough room.
  // This is called when the display space has been changed, i.e. working
  // area being changed or a panel being closed.
  virtual void RefreshLayout() OVERRIDE;

  // Adds a panel to the collection. The panel may be a newly created panel or
  // one that is transitioning from another grouping of panels.
  virtual void AddPanel(Panel* panel,
                        PositioningMask positioning_mask) OVERRIDE;
  virtual void RemovePanel(Panel* pane, RemovalReason reasonl) OVERRIDE;
  virtual void CloseAll() OVERRIDE;
  virtual void ResizePanelWindow(
      Panel* panel,
      const gfx::Size& preferred_window_size) OVERRIDE;
  virtual panel::Resizability GetPanelResizability(
      const Panel* panel) const OVERRIDE;
  virtual void OnPanelResizedByMouse(Panel* panel,
                                     const gfx::Rect& new_bounds) OVERRIDE;
  virtual void OnPanelAttentionStateChanged(Panel* panel) OVERRIDE;
  virtual void OnPanelTitlebarClicked(Panel* panel,
                                      panel::ClickModifier modifier) OVERRIDE;
  virtual void ActivatePanel(Panel* panel) OVERRIDE;
  virtual void MinimizePanel(Panel* panel) OVERRIDE;
  virtual void RestorePanel(Panel* panel) OVERRIDE;
  virtual void OnMinimizeButtonClicked(Panel* panel,
                                       panel::ClickModifier modifier) OVERRIDE;
  virtual void OnRestoreButtonClicked(Panel* panel,
                                      panel::ClickModifier modifier) OVERRIDE;
  virtual bool CanShowMinimizeButton(const Panel* panel) const OVERRIDE;
  virtual bool CanShowRestoreButton(const Panel* panel) const OVERRIDE;
  virtual bool IsPanelMinimized(const Panel* panel) const OVERRIDE;
  virtual void SavePanelPlacement(Panel* panel) OVERRIDE;
  virtual void RestorePanelToSavedPlacement() OVERRIDE;
  virtual void DiscardSavedPanelPlacement() OVERRIDE;
  virtual void UpdatePanelOnCollectionChange(Panel* panel) OVERRIDE;
  virtual void OnPanelExpansionStateChanged(Panel* panel) OVERRIDE;
  virtual void OnPanelActiveStateChanged(Panel* panel) OVERRIDE;

  // Returns true if we should bring up the titlebars, given the current mouse
  // point.
  bool ShouldBringUpTitlebars(int mouse_x, int mouse_y) const;

  // Brings up or down the titlebars for all minimized panels.
  void BringUpOrDownTitlebars(bool bring_up);

  // Compute default bounds for a panel of |full_size| that would be used
  // when adding the panel to the collection.
  gfx::Point GetDefaultPositionForPanel(const gfx::Size& full_size) const;

  // Returns the bottom position for the panel per its expansion state. If auto-
  // hide bottom bar is present, we want to move the minimized panel to the
  // bottom of the screen, not the bottom of the work area.
  int GetBottomPositionForExpansionState(
      Panel::ExpansionState expansion_state) const;

  // Returns panel width to be used, taking into account possible "squeezing"
  // due to lack of space in the collection.
  int WidthToDisplayPanelInCollection(bool is_for_active_panel,
                                      double squeeze_factor,
                                      int full_width) const;

  bool HasPanel(Panel* panel) const;

  // num_panels() and panels() only includes panels in the collection that
  // do NOT have a temporary layout.
  int num_panels() const { return panels_.size(); }
  const Panels& panels() const { return panels_; }
  Panel* last_panel() const { return panels_.empty() ? NULL : panels_.back(); }

  gfx::Rect work_area() const { return work_area_; }

  int StartingRightPosition() const;

  void OnFullScreenModeChanged(bool is_full_screen);

#ifdef UNIT_TEST
  int minimized_panel_count() const {return minimized_panel_count_; }
#endif

 private:
  friend class DockedPanelDragHandler;

  enum TitlebarAction {
    NO_ACTION,
    BRING_UP,
    BRING_DOWN
  };

  struct PanelPlacement {
    Panel* panel;
    // Used to remember the panel to the left of |panel|, if any, for use when
    // restoring the position of |panel|.
    Panel* left_panel;

    PanelPlacement() : panel(NULL), left_panel(NULL) { }
  };

  // Overridden from PanelMouseWatcherObserver:
  virtual void OnMouseMove(const gfx::Point& mouse_position) OVERRIDE;

  // Overridden from DisplaySettingsProvider::DesktopBarObserver:
  virtual void OnAutoHidingDesktopBarVisibilityChanged(
      DisplaySettingsProvider::DesktopBarAlignment alignment,
      DisplaySettingsProvider::DesktopBarVisibility visibility) OVERRIDE;
  virtual void OnAutoHidingDesktopBarThicknessChanged(
      DisplaySettingsProvider::DesktopBarAlignment alignment,
      int thickness) OVERRIDE;

  // Schedules a layout refresh with a short delay to avoid too much flicker.
  void ScheduleLayoutRefresh();

  // Keep track of the minimized panels to control mouse watching.
  void UpdateMinimizedPanelCount();

  // Minimizes or restores all panels in the collection.
  void MinimizeAll();
  void RestoreAll();

  // Makes sure the panel's bounds reflect its expansion state and the
  // panel is aligned at the bottom of the screen. Does not touch the x
  // coordinate.
  void AdjustPanelBoundsPerExpansionState(Panel* panel,
      gfx::Rect* panel_bounds);

  // Does the real job of bringing up or down the titlebars.
  void DoBringUpOrDownTitlebars(bool bring_up);
  // The callback for a delyed task, checks if it still need to perform
  // the delayed action.
  void DelayedBringUpOrDownTitlebarsCheck();

  int GetRightMostAvailablePosition() const;

  PanelManager* panel_manager_;  // Weak, owns us.

  // All panels in the collection must fit within this area.
  gfx::Rect work_area_;

  Panels panels_;

  int minimized_panel_count_;
  bool are_titlebars_up_;

  bool minimizing_all_;  // True while minimizing all panels.

  // Delayed transitions support. Sometimes transitions between minimized and
  // title-only states are delayed, for better usability with Taskbars/Docks.
  TitlebarAction delayed_titlebar_action_;

  // Owned by MessageLoop after posting.
  base::WeakPtrFactory<DockedPanelCollection> titlebar_action_factory_;

  // Owned by MessageLoop after posting.
  base::WeakPtrFactory<DockedPanelCollection> refresh_action_factory_;

  // Used to save the placement information for a panel.
  PanelPlacement saved_panel_placement_;

  static const int kPanelsHorizontalSpacing = 4;

  DISALLOW_COPY_AND_ASSIGN(DockedPanelCollection);
};

#endif  // CHROME_BROWSER_UI_PANELS_DOCKED_PANEL_COLLECTION_H_
