// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_OBSERVER_HUD_H_
#define ASH_TOUCH_TOUCH_OBSERVER_HUD_H_

#include "ash/ash_export.h"
#include "ash/display/display_controller.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/display_observer.h"
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
                                    public gfx::DisplayObserver,
#if defined(OS_CHROMEOS)
                                    public ui::DisplayConfigurator::Observer,
#endif  // defined(OS_CHROMEOS)
                                    public DisplayController::Observer {
 public:
  // Called to clear touch points and traces from the screen. Default
  // implementation does nothing. Sub-classes should implement appropriately.
  virtual void Clear();

  // Removes the HUD from the screen.
  void Remove();

  int64 display_id() const { return display_id_; }

 protected:
  explicit TouchObserverHUD(aura::Window* initial_root);

  virtual ~TouchObserverHUD();

  virtual void SetHudForRootWindowController(
      RootWindowController* controller) = 0;
  virtual void UnsetHudForRootWindowController(
      RootWindowController* controller) = 0;

  views::Widget* widget() { return widget_; }

  // Overriden from ui::EventHandler.
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;

  // Overridden from views::WidgetObserver.
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  // Overridden from gfx::DisplayObserver.
  virtual void OnDisplayAdded(const gfx::Display& new_display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& old_display) OVERRIDE;
  virtual void OnDisplayMetricsChanged(const gfx::Display& display,
                                       uint32_t metrics) OVERRIDE;

#if defined(OS_CHROMEOS)
  // Overriden from ui::DisplayConfigurator::Observer.
  virtual void OnDisplayModeChanged(
      const ui::DisplayConfigurator::DisplayStateList& outputs) OVERRIDE;
#endif  // defined(OS_CHROMEOS)

  // Overriden form DisplayController::Observer.
  virtual void OnDisplaysInitialized() OVERRIDE;
  virtual void OnDisplayConfigurationChanging() OVERRIDE;
  virtual void OnDisplayConfigurationChanged() OVERRIDE;

 private:
  friend class TouchHudTestBase;

  const int64 display_id_;
  aura::Window* root_window_;

  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(TouchObserverHUD);
};

}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_OBSERVER_HUD_H_
