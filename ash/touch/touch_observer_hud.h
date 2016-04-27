// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_OBSERVER_HUD_H_
#define ASH_TOUCH_TOUCH_OBSERVER_HUD_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "base/macros.h"
#include "ui/display/display_observer.h"
#include "ui/events/event_handler.h"
#include "ui/views/widget/widget_observer.h"

#if defined(OS_CHROMEOS)
#include "ui/display/chromeos/display_configurator.h"
#endif  // defined(OS_CHROMEOS)

namespace views {
class Widget;
}

namespace ash {

// An event filter which handles system level gesture events. Objects of this
// class manage their own lifetime.
class ASH_EXPORT TouchObserverHUD : public ui::EventHandler,
                                    public views::WidgetObserver,
                                    public display::DisplayObserver,
#if defined(OS_CHROMEOS)
                                    public ui::DisplayConfigurator::Observer,
#endif  // defined(OS_CHROMEOS)
                                    public WindowTreeHostManager::Observer {
 public:
  // Called to clear touch points and traces from the screen. Default
  // implementation does nothing. Sub-classes should implement appropriately.
  virtual void Clear();

  // Removes the HUD from the screen.
  void Remove();

  int64_t display_id() const { return display_id_; }

 protected:
  explicit TouchObserverHUD(aura::Window* initial_root);

  ~TouchObserverHUD() override;

  virtual void SetHudForRootWindowController(
      RootWindowController* controller) = 0;
  virtual void UnsetHudForRootWindowController(
      RootWindowController* controller) = 0;

  views::Widget* widget() { return widget_; }

  // Overriden from ui::EventHandler.
  void OnTouchEvent(ui::TouchEvent* event) override;

  // Overridden from views::WidgetObserver.
  void OnWidgetDestroying(views::Widget* widget) override;

  // Overridden from display::DisplayObserver.
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

#if defined(OS_CHROMEOS)
  // Overriden from ui::DisplayConfigurator::Observer.
  void OnDisplayModeChanged(
      const ui::DisplayConfigurator::DisplayStateList& outputs) override;
#endif  // defined(OS_CHROMEOS)

  // Overriden form WindowTreeHostManager::Observer.
  void OnDisplaysInitialized() override;
  void OnDisplayConfigurationChanging() override;
  void OnDisplayConfigurationChanged() override;

 private:
  friend class TouchHudTestBase;

  const int64_t display_id_;
  aura::Window* root_window_;

  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(TouchObserverHUD);
};

}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_OBSERVER_HUD_H_
