// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_RELOAD_BUTTON_GTK_H_
#define CHROME_BROWSER_UI_GTK_RELOAD_BUTTON_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/timer.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"

class Browser;
class GtkThemeService;
class LocationBarViewGtk;

class ReloadButtonGtk : public content::NotificationObserver,
                        MenuGtk::Delegate,
                        public ui::SimpleMenuModel::Delegate {
 public:
  enum Mode { MODE_RELOAD = 0, MODE_STOP };

  ReloadButtonGtk(LocationBarViewGtk* location_bar, Browser* browser);
  virtual ~ReloadButtonGtk();

  GtkWidget* widget() const { return widget_.get(); }

  // Ask for a specified button state.  If |force| is true this will be applied
  // immediately.
  void ChangeMode(Mode mode, bool force);

  // Provide content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Provide MenuGtk::Delegate implementation.
  virtual void StoppedShowing() OVERRIDE;

  // Provide SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool IsCommandIdVisible(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
     int command_id,
     ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

 private:
  friend class ReloadButtonGtkTest;

  CHROMEGTK_CALLBACK_0(ReloadButtonGtk, void, OnClicked);
  CHROMEGTK_CALLBACK_1(ReloadButtonGtk, gboolean, OnExpose, GdkEventExpose*);
  CHROMEGTK_CALLBACK_1(ReloadButtonGtk,
                       gboolean,
                       OnLeaveNotify,
                       GdkEventCrossing*);
  CHROMEGTK_CALLBACK_4(ReloadButtonGtk,
                       gboolean,
                       OnQueryTooltip,
                       gint,
                       gint,
                       gboolean,
                       GtkTooltip*);

  // Starts a timer to show the dropdown menu.
  CHROMEGTK_CALLBACK_1(ReloadButtonGtk,
                       gboolean,
                       OnButtonPress,
                       GdkEventButton*);

  // If there is a timer to show the dropdown menu, and the mouse has moved
  // sufficiently down the screen, cancel the timer and immediately show the
  // menu.
  CHROMEGTK_CALLBACK_1(ReloadButtonGtk,
                       gboolean,
                       OnMouseMove,
                       GdkEventMotion*);

  void UpdateThemeButtons();

  void OnDoubleClickTimer();
  void OnStopToReloadTimer();

  // Shows the dropdown menu.
  void ShowReloadMenu(int button, guint32 event_time);

  // Do actual reload. command == 0, indicates default dehaviour.
  void DoReload(int command);

  // Indicates if reload menu is currently enabled.
  bool ReloadMenuEnabled();
  void ClearCache();

  base::OneShotTimer<ReloadButtonGtk> double_click_timer_;
  base::OneShotTimer<ReloadButtonGtk> stop_to_reload_timer_;

  // These may be NULL when testing.
  LocationBarViewGtk* const location_bar_;
  Browser* const browser_;

  // The mode we should be in assuming no timers are running.
  Mode intended_mode_;

  // The currently-visible mode - this may differ from the intended mode.
  Mode visible_mode_;

  // Used to listen for theme change notifications.
  content::NotificationRegistrar registrar_;

  GtkThemeService* theme_service_;

  CustomDrawButtonBase reload_;
  CustomDrawButtonBase stop_;
  CustomDrawHoverController hover_controller_;

  ui::OwnedWidgetGtk widget_;

  // The delay times for the timers.  These are members so that tests can modify
  // them.
  base::TimeDelta double_click_timer_delay_;
  base::TimeDelta stop_to_reload_timer_delay_;

  // The y position of the last mouse down event.
  int y_position_of_last_press_;
  base::WeakPtrFactory<ReloadButtonGtk> weak_factory_;
  // The menu gets reset every time it is shown.
  scoped_ptr<MenuGtk> menu_;
  // The dropdown menu model.
  scoped_ptr<ui::SimpleMenuModel> menu_model_;
  // Indicates if menu is currently shown.
  bool menu_visible_;

  // TESTING ONLY
  // True if we should pretend the button is hovered.
  bool testing_mouse_hovered_;
  // Increments when we would tell the browser to "reload", so
  // test code can tell whether we did so (as there may be no |browser_|).
  int testing_reload_count_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ReloadButtonGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_RELOAD_BUTTON_GTK_H_
