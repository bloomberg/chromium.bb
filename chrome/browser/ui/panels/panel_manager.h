// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_MANAGER_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_MANAGER_H_
#pragma once

#include <vector>
#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/panels/auto_hiding_desktop_bar.h"
#include "chrome/browser/ui/panels/panel.h"
#include "ui/gfx/rect.h"

// TODO(jennb): Clean up by removing functions below that cause this
// to be required.
#ifdef UNIT_TEST
#include "chrome/browser/ui/panels/panel_strip.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher.h"
#endif

class Browser;
class PanelMouseWatcher;
class PanelStrip;

// This class manages a set of panels.
class PanelManager : public AutoHidingDesktopBar::Observer {
 public:
  typedef std::vector<Panel*> Panels;

  // Returns a single instance.
  static PanelManager* GetInstance();

  // Called when the display is changed, i.e. work area is updated.
  void OnDisplayChanged();

  // Creates a panel and returns it. The panel might be queued for display
  // later.
  Panel* CreatePanel(Browser* browser);

  void Remove(Panel* panel);
  void RemoveAll();

  // Asynchronous confirmation of panel having been removed.
  void OnPanelRemoved(Panel* panel);

  // Drags the given panel.
  void StartDragging(Panel* panel);
  void Drag(int delta_x);
  void EndDragging(bool cancelled);

  // Invoked when a panel's expansion state changes.
  void OnPanelExpansionStateChanged(Panel::ExpansionState old_state,
                                    Panel::ExpansionState new_state);

  // Invoked when the preferred window size of the given panel might need to
  // get changed.
  void OnPreferredWindowSizeChanged(
      Panel* panel, const gfx::Size& preferred_window_size);

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

  // Returns the next browser window which could be either panel window or
  // tabbed window, to switch to if the given panel is going to be deactivated.
  // Returns NULL if such window cannot be found.
  BrowserWindow* GetNextBrowserWindowToActivate(Panel* panel) const;

  int num_panels() const;
  bool is_dragging_panel() const;
  int StartingRightPosition() const;
  const Panels& panels() const;

  // Moves a panel to the overflow strip. The panel does not currently
  // belong in any other strip.
  // |is_new| is true if the panel was just created.
  void MoveToOverflowStrip(Panel* panel, bool is_new);

  AutoHidingDesktopBar* auto_hiding_desktop_bar() const {
    return auto_hiding_desktop_bar_;
  }

  PanelMouseWatcher* mouse_watcher() const {
    return panel_mouse_watcher_.get();
  }

  PanelStrip* panel_strip() const {
    return panel_strip_.get();
  }

#ifdef UNIT_TEST
  static int horizontal_spacing() { return PanelStrip::horizontal_spacing(); }

  const gfx::Rect& work_area() const {
    return work_area_;
  }

  void set_auto_hiding_desktop_bar(
      AutoHidingDesktopBar* auto_hiding_desktop_bar) {
    auto_hiding_desktop_bar_ = auto_hiding_desktop_bar;
  }

  void set_mouse_watcher(PanelMouseWatcher* watcher) {
    panel_mouse_watcher_.reset(watcher);
  }

  void enable_auto_sizing(bool enabled) {
    auto_sizing_enabled_ = enabled;
  }

  void SetWorkAreaForTesting(const gfx::Rect& work_area) {
    SetWorkArea(work_area);
  }

  void remove_delays_for_testing() {
    panel_strip_->remove_delays_for_testing();
  }

  int minimized_panel_count() {
    return panel_strip_->minimized_panel_count();
  }
#endif

 private:
  friend struct base::DefaultLazyInstanceTraits<PanelManager>;

  PanelManager();
  virtual ~PanelManager();

  // Overridden from AutoHidingDesktopBar::Observer:
  virtual void OnAutoHidingDesktopBarThicknessChanged() OVERRIDE;
  virtual void OnAutoHidingDesktopBarVisibilityChanged(
      AutoHidingDesktopBar::Alignment alignment,
      AutoHidingDesktopBar::Visibility visibility) OVERRIDE;

  // Applies the new work area. This is called by OnDisplayChanged and the test
  // code.
  void SetWorkArea(const gfx::Rect& work_area);

  // Adjusts the work area to exclude the influence of auto-hiding desktop bars.
  void AdjustWorkAreaForAutoHidingDesktopBars();

  // Positions the various groupings of panels.
  void Layout();

  scoped_ptr<PanelStrip> panel_strip_;

  // Use a mouse watcher to know when to bring up titlebars to "peek" at
  // minimized panels. Mouse movement is only tracked when there is a minimized
  // panel.
  scoped_ptr<PanelMouseWatcher> panel_mouse_watcher_;

  // The maximum work area avaialble. This area does not include the area taken
  // by the always-visible (non-auto-hiding) desktop bars.
  gfx::Rect work_area_;

  // The useable work area for computing the panel bounds. This area excludes
  // the potential area that could be taken by the auto-hiding desktop
  // bars (we only consider those bars that are aligned to bottom, left, and
  // right of the screen edges) when they become fully visible.
  gfx::Rect adjusted_work_area_;

  scoped_refptr<AutoHidingDesktopBar> auto_hiding_desktop_bar_;

  // Whether or not bounds will be updated when the preferred content size is
  // changed. The testing code could set this flag to false so that other tests
  // will not be affected.
  bool auto_sizing_enabled_;

  DISALLOW_COPY_AND_ASSIGN(PanelManager);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_MANAGER_H_
