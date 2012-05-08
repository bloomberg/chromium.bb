// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_MANAGER_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_MANAGER_H_
#pragma once

#include <vector>
#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/panels/display_settings_provider.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_constants.h"
#include "chrome/browser/ui/panels/panel_strip.h"
#include "ui/gfx/rect.h"

class Browser;
class DetachedPanelStrip;
class DockedPanelStrip;
class PanelDragController;
class PanelResizeController;
class PanelMouseWatcher;

// This class manages a set of panels.
class PanelManager : public DisplaySettingsProvider::DisplayAreaObserver,
                     public DisplaySettingsProvider::FullScreenObserver {
 public:
  // Returns a single instance.
  static PanelManager* GetInstance();

  // Returns true if panels should be used for the extension.
  static bool ShouldUsePanels(const std::string& extension_id);

  // Creates a panel and returns it. The panel might be queued for display
  // later.
  Panel* CreatePanel(Browser* browser);

  // Close all panels (asynchronous). Panels will be removed after closing.
  void CloseAll();

  // Asynchronous confirmation of panel having been closed.
  void OnPanelClosed(Panel* panel);

  // Returns the maximum size that panel can be auto-resized or resized by the
  // API.
  int GetMaxPanelWidth() const;
  int GetMaxPanelHeight() const;

  // Drags the given panel.
  // |mouse_location| is in screen coordinate system.
  void StartDragging(Panel* panel, const gfx::Point& mouse_location);
  void Drag(const gfx::Point& mouse_location);
  void EndDragging(bool cancelled);

  // Resizes the given panel.
  // |mouse_location| is in screen coordinate system.
  void StartResizingByMouse(Panel* panel, const gfx::Point& mouse_location,
                            panel::ResizingSides sides);
  void ResizeByMouse(const gfx::Point& mouse_location);
  void EndResizingByMouse(bool cancelled);

  // Resizes the panel and sets the origin.
  void OnPanelResizedByMouse(Panel* panel, const gfx::Rect& new_bounds);

  // Invoked when the preferred window size of the given panel might need to
  // get changed.
  void OnWindowAutoResized(Panel* panel,
                           const gfx::Size& preferred_window_size);

  // Resizes the panel. Explicitly setting the panel size is not allowed
  // for panels that are auto-sized.
  void ResizePanel(Panel* panel, const gfx::Size& new_size);

  // Invoked when a panel's expansion state changes.
  void OnPanelExpansionStateChanged(Panel* panel);

  // Moves the |panel| to a different type of panel strip.
  void MovePanelToStrip(Panel* panel,
                        PanelStrip::Type new_layout,
                        PanelStrip::PositioningMask positioning_mask);

  // Returns true if we should bring up the titlebars, given the current mouse
  // point.
  bool ShouldBringUpTitlebars(int mouse_x, int mouse_y) const;

  // Brings up or down the titlebars for all minimized panels.
  void BringUpOrDownTitlebars(bool bring_up);

  // Returns the next browser window which could be either panel window or
  // tabbed window, to switch to if the given panel is going to be deactivated.
  // Returns NULL if such window cannot be found.
  BrowserWindow* GetNextBrowserWindowToActivate(Panel* panel) const;

  int num_panels() const;
  std::vector<Panel*> panels() const;

  PanelDragController* drag_controller() const {
    return drag_controller_.get();
  }

#ifdef UNIT_TEST
  PanelResizeController* resize_controller() const {
    return resize_controller_.get();
  }
#endif

  DisplaySettingsProvider* display_settings_provider() const {
    return display_settings_provider_.get();
  }

  PanelMouseWatcher* mouse_watcher() const {
    return panel_mouse_watcher_.get();
  }

  DetachedPanelStrip* detached_strip() const {
    return detached_strip_.get();
  }

  DockedPanelStrip* docked_strip() const {
    return docked_strip_.get();
  }

  // Reduces time interval in tests to shorten test run time.
  // Wrapper should be used around all time intervals in panels code.
  static inline double AdjustTimeInterval(double interval) {
    if (shorten_time_intervals_)
      return interval / 500.0;
    else
      return interval;
  }


  bool auto_sizing_enabled() const {
    return auto_sizing_enabled_;
  }

  // Called from native level when panel animation ends.
  void OnPanelAnimationEnded(Panel* panel);

#ifdef UNIT_TEST
  static void shorten_time_intervals_for_testing() {
    shorten_time_intervals_ = true;
  }

  void set_display_settings_provider(
      DisplaySettingsProvider* display_settings_provider) {
    display_settings_provider_.reset(display_settings_provider);
  }

  void enable_auto_sizing(bool enabled) {
    auto_sizing_enabled_ = enabled;
  }

  void SetMouseWatcherForTesting(PanelMouseWatcher* watcher) {
    SetMouseWatcher(watcher);
  }
#endif

 private:
  friend struct base::DefaultLazyInstanceTraits<PanelManager>;

  PanelManager();
  virtual ~PanelManager();

  // Overridden from DisplaySettingsProvider::DisplayAreaObserver:
  virtual void OnDisplayAreaChanged(const gfx::Rect& display_area) OVERRIDE;

  // Overridden from DisplaySettingsProvider::FullScreenObserver:
  virtual void OnFullScreenModeChanged(bool is_full_screen) OVERRIDE;

  // Tests may want to use a mock panel mouse watcher.
  void SetMouseWatcher(PanelMouseWatcher* watcher);

  // Tests may want to shorten time intervals to reduce running time.
  static bool shorten_time_intervals_;

  scoped_ptr<DetachedPanelStrip> detached_strip_;
  scoped_ptr<DockedPanelStrip> docked_strip_;

  scoped_ptr<PanelDragController> drag_controller_;
  scoped_ptr<PanelResizeController> resize_controller_;

  // Use a mouse watcher to know when to bring up titlebars to "peek" at
  // minimized panels. Mouse movement is only tracked when there is a minimized
  // panel.
  scoped_ptr<PanelMouseWatcher> panel_mouse_watcher_;

  scoped_ptr<DisplaySettingsProvider> display_settings_provider_;

  gfx::Rect display_area_;

  // Whether or not bounds will be updated when the preferred content size is
  // changed. The testing code could set this flag to false so that other tests
  // will not be affected.
  bool auto_sizing_enabled_;

  DISALLOW_COPY_AND_ASSIGN(PanelManager);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_MANAGER_H_
