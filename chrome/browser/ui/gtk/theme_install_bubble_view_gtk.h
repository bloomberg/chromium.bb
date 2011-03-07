// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_THEME_INSTALL_BUBBLE_VIEW_GTK_H_
#define CHROME_BROWSER_UI_GTK_THEME_INSTALL_BUBBLE_VIEW_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"

class ThemeInstallBubbleViewGtk : public NotificationObserver {
 public:
  static void Show(GtkWindow* parent);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  explicit ThemeInstallBubbleViewGtk(GtkWidget* parent);

  virtual ~ThemeInstallBubbleViewGtk();

  void increment_num_loading() { num_loads_extant_++; }

  // Create the widget hierarchy.
  void InitWidgets();

  // Reposition |widget_| to be centered over |parent_|.
  void MoveWindow();

  // Our parent is going down; self destruct.
  CHROMEGTK_CALLBACK_0(ThemeInstallBubbleViewGtk, gboolean, OnUnmapEvent);

  // Draw the background. This is only signalled if we are using a compositing
  // window manager, otherwise we just use ActAsRoundedWindow().
  CHROMEGTK_CALLBACK_1(ThemeInstallBubbleViewGtk, gboolean,
                       OnExpose, GdkEventExpose*);

  GtkWidget* widget_;

  // The parent browser window, over which we position ourselves.
  GtkWidget* parent_;

  // The number of loads we represent. When it reaches 0 we delete ourselves.
  int num_loads_extant_;

  NotificationRegistrar registrar_;

  // Our one instance. We don't allow more than one to exist at a time.
  static ThemeInstallBubbleViewGtk* instance_;

  DISALLOW_COPY_AND_ASSIGN(ThemeInstallBubbleViewGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_THEME_INSTALL_BUBBLE_VIEW_GTK_H_
