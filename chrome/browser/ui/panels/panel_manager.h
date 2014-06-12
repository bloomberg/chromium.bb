// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_MANAGER_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_MANAGER_H_

#include <list>
#include <vector>
#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/panels/display_settings_provider.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_collection.h"
#include "chrome/browser/ui/panels/panel_constants.h"
#include "ui/gfx/rect.h"

class DetachedPanelCollection;
class DockedPanelCollection;
class GURL;
class PanelDragController;
class PanelResizeController;
class PanelMouseWatcher;
class StackedPanelCollection;

// This class manages a set of panels.
class PanelManager : public DisplaySettingsProvider::DisplayObserver,
                     public DisplaySettingsProvider::FullScreenObserver {
 public:
  typedef std::list<StackedPanelCollection*> Stacks;

  enum CreateMode {
    CREATE_AS_DOCKED,  // Creates a docked panel. The default.
    CREATE_AS_DETACHED  // Creates a detached panel.
  };

  // Returns a single instance.
  static PanelManager* GetInstance();

  // Tells PanelManager to use |provider| for testing purpose. This has to be
  // called before GetInstance.
  static void SetDisplaySettingsProviderForTesting(
      DisplaySettingsProvider* provider);

  // Returns true if panels should be used for the extension.
  static bool ShouldUsePanels(const std::string& extension_id);

  // Returns true if panel stacking support is enabled.
  static bool IsPanelStackingEnabled();

  // Returns true if a panel can be system-minimized by the desktop
  // environment. Some desktop environment, like Unity, does not trigger the
  // "window-state-event" which prevents the minimize and unminimize from
  // working.
  static bool CanUseSystemMinimize();

  // Returns the default top-left position for a detached panel.
  gfx::Point GetDefaultDetachedPanelOrigin();

  // Creates a panel and returns it. The panel might be queued for display
  // later.
  // |app_name| is the default title for Panels when the page content does not
  // provide a title. For extensions, this is usually the application name
  // generated from the extension id.
  // |requested_bounds| is the desired bounds for the panel, but actual
  // bounds may differ after panel layout depending on create |mode|.
  // |mode| indicates whether panel should be created as docked or detached.
  Panel* CreatePanel(const std::string& app_name,
                     Profile* profile,
                     const GURL& url,
                     const gfx::Rect& requested_bounds,
                     CreateMode mode);

  // Close all panels (asynchronous). Panels will be removed after closing.
  void CloseAll();

  // Asynchronous confirmation of panel having been closed.
  void OnPanelClosed(Panel* panel);

  // Creates a StackedPanelCollection and returns it.
  StackedPanelCollection* CreateStack();

  // Deletes |stack|. The stack must be empty at the time of deletion.
  void RemoveStack(StackedPanelCollection* stack);

  // Returns the maximum size that panel can be auto-resized or resized by the
  // API.
  int GetMaxPanelWidth(const gfx::Rect& work_area) const;
  int GetMaxPanelHeight(const gfx::Rect& work_area) const;

  // Drags the given panel.
  // |mouse_location| is in screen coordinate system.
  void StartDragging(Panel* panel, const gfx::Point& mouse_location);
  void Drag(const gfx::Point& mouse_location);
  void EndDragging(bool cancelled);

  // Resizes the given panel.
  // |mouse_location| is in screen coordinate system.
  void StartResizingByMouse(Panel* panel, const gfx::Point& mouse_location,
                            int component);
  void ResizeByMouse(const gfx::Point& mouse_location);
  void EndResizingByMouse(bool cancelled);

  // Invoked when a panel's expansion state changes.
  void OnPanelExpansionStateChanged(Panel* panel);

  // Moves the |panel| to a different collection.
  void MovePanelToCollection(Panel* panel,
                             PanelCollection* target_collection,
                             PanelCollection::PositioningMask positioning_mask);

  // Returns true if we should bring up the titlebars, given the current mouse
  // point.
  bool ShouldBringUpTitlebars(int mouse_x, int mouse_y) const;

  // Brings up or down the titlebars for all minimized panels.
  void BringUpOrDownTitlebars(bool bring_up);

  std::vector<Panel*> GetDetachedAndStackedPanels() const;

  int num_panels() const;
  std::vector<Panel*> panels() const;

  const Stacks& stacks() const { return stacks_; }
  int num_stacks() const { return stacks_.size(); }

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

  DetachedPanelCollection* detached_collection() const {
    return detached_collection_.get();
  }

  DockedPanelCollection* docked_collection() const {
    return docked_collection_.get();
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

  void Initialize(DisplaySettingsProvider* provider);

  // Overridden from DisplaySettingsProvider::DisplayObserver:
  virtual void OnDisplayChanged() OVERRIDE;

  // Overridden from DisplaySettingsProvider::FullScreenObserver:
  virtual void OnFullScreenModeChanged(bool is_full_screen) OVERRIDE;

  // Returns the collection to which a new panel should add. The new panel
  // is expected to be created with |bounds| and |mode|. The size of |bounds|
  // could be used to determine which collection is more appropriate to have
  // the new panel. Upon return, |positioning_mask| contains the required mask
  // to be applied when the new panel is being added to the collection.
  PanelCollection* GetCollectionForNewPanel(
      Panel* new_panel,
      const gfx::Rect& bounds,
      CreateMode mode,
      PanelCollection::PositioningMask* positioning_mask);

  // Tests may want to use a mock panel mouse watcher.
  void SetMouseWatcher(PanelMouseWatcher* watcher);

  // Tests may want to shorten time intervals to reduce running time.
  static bool shorten_time_intervals_;

  scoped_ptr<DetachedPanelCollection> detached_collection_;
  scoped_ptr<DockedPanelCollection> docked_collection_;
  Stacks stacks_;

  scoped_ptr<PanelDragController> drag_controller_;
  scoped_ptr<PanelResizeController> resize_controller_;

  // Use a mouse watcher to know when to bring up titlebars to "peek" at
  // minimized panels. Mouse movement is only tracked when there is a minimized
  // panel.
  scoped_ptr<PanelMouseWatcher> panel_mouse_watcher_;

  scoped_ptr<DisplaySettingsProvider> display_settings_provider_;

  // Whether or not bounds will be updated when the preferred content size is
  // changed. The testing code could set this flag to false so that other tests
  // will not be affected.
  bool auto_sizing_enabled_;

  DISALLOW_COPY_AND_ASSIGN(PanelManager);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_MANAGER_H_
