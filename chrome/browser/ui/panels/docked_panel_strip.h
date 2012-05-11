// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_DOCKED_PANEL_STRIP_H_
#define CHROME_BROWSER_UI_PANELS_DOCKED_PANEL_STRIP_H_
#pragma once

#include <list>
#include <set>
#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/panels/display_settings_provider.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_strip.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher_observer.h"
#include "ui/gfx/rect.h"

class Browser;
class PanelManager;

// This class manages a group of panels displayed in a horizontal strip,
// positioning the panels and controlling how they are displayed.
// Panels in the strip appear minimized, showing title-only or expanded.
// All panels in the strip are contained within the bounds of the strip.
class DockedPanelStrip : public PanelStrip,
                         public PanelMouseWatcherObserver,
                         public DisplaySettingsProvider::DesktopBarObserver {
 public:
  typedef std::list<Panel*> Panels;

  explicit DockedPanelStrip(PanelManager* panel_manager);
  virtual ~DockedPanelStrip();

  // PanelStrip OVERRIDES:
  virtual gfx::Rect GetDisplayArea() const OVERRIDE;
  virtual void SetDisplayArea(const gfx::Rect& display_area) OVERRIDE;

  // Rearranges the positions of the panels in the strip
  // and reduces their width when there is not enough room.
  // This is called when the display space has been changed, i.e. working
  // area being changed or a panel being closed.
  virtual void RefreshLayout() OVERRIDE;

  // Adds a panel to the strip. The panel may be a newly created panel or one
  // that is transitioning from another grouping of panels.
  virtual void AddPanel(Panel* panel,
                        PositioningMask positioning_mask) OVERRIDE;
  virtual void RemovePanel(Panel* panel) OVERRIDE;
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
  virtual void MinimizeAll() OVERRIDE;
  virtual void RestoreAll() OVERRIDE;
  virtual bool CanMinimizePanel(const Panel* panel) const OVERRIDE;
  virtual bool IsPanelMinimized(const Panel* panel) const OVERRIDE;
  virtual void SavePanelPlacement(Panel* panel) OVERRIDE;
  virtual void RestorePanelToSavedPlacement() OVERRIDE;
  virtual void DiscardSavedPanelPlacement() OVERRIDE;
  virtual void StartDraggingPanelWithinStrip(Panel* panel) OVERRIDE;
  virtual void DragPanelWithinStrip(Panel* panel,
                                    const gfx::Point& target_position) OVERRIDE;
  virtual void EndDraggingPanelWithinStrip(Panel* panel,
                                           bool aborted) OVERRIDE;
  virtual void ClearDraggingStateWhenPanelClosed() OVERRIDE;
  virtual void UpdatePanelOnStripChange(Panel* panel) OVERRIDE;
  virtual void OnPanelActiveStateChanged(Panel* panel) OVERRIDE;

  // Invoked when a panel's expansion state changes.
  void OnPanelExpansionStateChanged(Panel* panel);

  // Returns true if we should bring up the titlebars, given the current mouse
  // point.
  bool ShouldBringUpTitlebars(int mouse_x, int mouse_y) const;

  // Brings up or down the titlebars for all minimized panels.
  void BringUpOrDownTitlebars(bool bring_up);

  // Returns the bottom position for the panel per its expansion state. If auto-
  // hide bottom bar is present, we want to move the minimized panel to the
  // bottom of the screen, not the bottom of the work area.
  int GetBottomPositionForExpansionState(
      Panel::ExpansionState expansion_state) const;

  // Returns panel width to be used, taking into account possible "squeezing"
  // due to lack of space in the strip.
  int WidthToDisplayPanelInStrip(bool is_for_active_panel,
                                 double squeeze_factor,
                                 int full_width) const;

  bool HasPanel(Panel* panel) const;

  // num_panels() and panels() only includes panels in the panel strip that
  // do NOT have a temporary layout.
  int num_panels() const { return panels_.size(); }
  const Panels& panels() const { return panels_; }
  Panel* last_panel() const { return panels_.empty() ? NULL : panels_.back(); }

  gfx::Rect display_area() const { return display_area_; }

  int StartingRightPosition() const;

  void OnFullScreenModeChanged(bool is_full_screen);

#ifdef UNIT_TEST
  int minimized_panel_count() const {return minimized_panel_count_; }
#endif

 private:
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

  // Helper methods to put the panel to the collection.
  gfx::Point GetDefaultPositionForPanel(const gfx::Size& full_size) const;
  void InsertNewlyCreatedPanel(Panel* panel);
  void InsertExistingPanelAtKnownPosition(Panel* panel);
  void InsertExistingPanelAtDefaultPosition(Panel* panel);

  // Schedules a layout refresh with a short delay to avoid too much flicker.
  void ScheduleLayoutRefresh();

  // Keep track of the minimized panels to control mouse watching.
  void UpdateMinimizedPanelCount();

  // Makes sure the panel's bounds reflect its expansion state and the
  // panel is aligned at the bottom of the strip. Does not touch the x
  // coordinate.
  void AdjustPanelBoundsPerExpansionState(Panel* panel,
      gfx::Rect* panel_bounds);

  // Help functions to drag the given panel.
  void DragLeft(Panel* dragging_panel);
  void DragRight(Panel* dragging_panel);

  // Does the real job of bringing up or down the titlebars.
  void DoBringUpOrDownTitlebars(bool bring_up);
  // The callback for a delyed task, checks if it still need to perform
  // the delayed action.
  void DelayedBringUpOrDownTitlebarsCheck();

  int GetRightMostAvailablePosition() const;

  PanelManager* panel_manager_;  // Weak, owns us.

  // All panels in the panel strip must fit within this area.
  gfx::Rect display_area_;

  Panels panels_;

  int minimized_panel_count_;
  bool are_titlebars_up_;

  bool minimizing_all_;  // True while minimizing all panels.

  // Referring to current position in |panels_| where the dragging panel
  // resides.
  Panels::iterator dragging_panel_current_iterator_;

  // Delayed transitions support. Sometimes transitions between minimized and
  // title-only states are delayed, for better usability with Taskbars/Docks.
  TitlebarAction delayed_titlebar_action_;

  // Owned by MessageLoop after posting.
  base::WeakPtrFactory<DockedPanelStrip> titlebar_action_factory_;

  // Owned by MessageLoop after posting.
  base::WeakPtrFactory<DockedPanelStrip> refresh_action_factory_;

  // Used to save the placement information for a panel.
  PanelPlacement saved_panel_placement_;

  static const int kPanelsHorizontalSpacing = 4;

  // Absolute minimum width and height for panels, including non-client area.
  // Should only be big enough to accomodate a close button on the reasonably
  // recognisable titlebar.
  static const int kPanelMinWidth;
  static const int kPanelMinHeight;

  DISALLOW_COPY_AND_ASSIGN(DockedPanelStrip);
};

#endif  // CHROME_BROWSER_UI_PANELS_DOCKED_PANEL_STRIP_H_
