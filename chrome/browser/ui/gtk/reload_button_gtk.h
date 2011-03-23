// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_RELOAD_BUTTON_GTK_H_
#define CHROME_BROWSER_UI_GTK_RELOAD_BUTTON_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/timer.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/owned_widget_gtk.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"

class Browser;
class GtkThemeService;
class LocationBarViewGtk;
class Task;

class ReloadButtonGtk : public NotificationObserver {
 public:
  enum Mode { MODE_RELOAD = 0, MODE_STOP };

  ReloadButtonGtk(LocationBarViewGtk* location_bar, Browser* browser);
  ~ReloadButtonGtk();

  GtkWidget* widget() const { return widget_.get(); }

  // Ask for a specified button state.  If |force| is true this will be applied
  // immediately.
  void ChangeMode(Mode mode, bool force);

  // Provide NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& /* details */);

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

  void UpdateThemeButtons();

  void OnDoubleClickTimer();
  void OnStopToReloadTimer();

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
  NotificationRegistrar registrar_;

  GtkThemeService* theme_service_;

  CustomDrawButtonBase reload_;
  CustomDrawButtonBase stop_;
  CustomDrawHoverController hover_controller_;

  OwnedWidgetGtk widget_;

  // The delay times for the timers.  These are members so that tests can modify
  // them.
  base::TimeDelta double_click_timer_delay_;
  base::TimeDelta stop_to_reload_timer_delay_;

  // TESTING ONLY
  // True if we should pretend the button is hovered.
  bool testing_mouse_hovered_;
  // Increments when we would tell the browser to "reload", so
  // test code can tell whether we did so (as there may be no |browser_|).
  int testing_reload_count_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ReloadButtonGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_RELOAD_BUTTON_GTK_H_
