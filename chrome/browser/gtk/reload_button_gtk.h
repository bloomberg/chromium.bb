// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_RELOAD_BUTTON_GTK_H_
#define CHROME_BROWSER_GTK_RELOAD_BUTTON_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "app/gtk_signal.h"
#include "base/basictypes.h"
#include "base/timer.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/owned_widget_gtk.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class Browser;
class GtkThemeProvider;
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
                       const NotificationDetails& details);

 private:
  friend class ReloadButtonGtkPeer;

  CHROMEGTK_CALLBACK_0(ReloadButtonGtk, void, OnClicked);
  CHROMEGTK_CALLBACK_1(ReloadButtonGtk, gboolean, OnExpose, GdkEventExpose*);
  CHROMEGTK_CALLBACK_1(ReloadButtonGtk, gboolean, OnLeaveNotify,
                       GdkEventCrossing*);
  CHROMEGTK_CALLBACK_4(ReloadButtonGtk, gboolean, OnQueryTooltip, gint, gint,
                       gboolean, GtkTooltip*);

  void SetToggled();

  bool timer_running() const {
    return timer_.IsRunning() || pretend_timer_is_running_for_unittest_;
  }

  void OnButtonTimer();

  void UpdateThemeButtons();

  // Used to listen for theme change notifications.
  NotificationRegistrar registrar_;

  LocationBarViewGtk* const location_bar_;

  // Keep a pointer to the Browser object to execute commands on it.
  Browser* const browser_;

  // Delay time to wait before allowing a mode change.  This is to prevent a
  // mode switch while the user is double clicking.
  int button_delay_;
  base::OneShotTimer<ReloadButtonGtk> timer_;
  bool pretend_timer_is_running_for_unittest_;

  // The mode we should be in.
  Mode intended_mode_;

  // The currently-visible mode - this may different from the intended mode.
  Mode visible_mode_;

  GtkThemeProvider* theme_provider_;

  CustomDrawButtonBase reload_;
  CustomDrawButtonBase stop_;
  CustomDrawHoverController hover_controller_;

  OwnedWidgetGtk widget_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ReloadButtonGtk);
};

#endif  // CHROME_BROWSER_GTK_RELOAD_BUTTON_GTK_H_
