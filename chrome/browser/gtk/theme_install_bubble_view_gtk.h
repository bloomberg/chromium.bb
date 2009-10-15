// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_THEME_INSTALL_BUBBLE_VIEW_GTK_H_
#define CHROME_BROWSER_GTK_THEME_INSTALL_BUBBLE_VIEW_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

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

  // Move with the parent window.
  static gboolean HandleParentConfigureThunk(GtkWidget* widget,
                                             GdkEventConfigure* event,
                                             gpointer user_data) {
    return reinterpret_cast<ThemeInstallBubbleViewGtk*>(user_data)->
        HandleParentConfigure(event);
  }
  gboolean HandleParentConfigure(GdkEventConfigure* event);

  // Our parent is going down; self destruct.
  static gboolean HandleParentUnmapThunk(GtkWidget* widget,
                                         GdkEvent* event,
                                         gpointer user_data) {
    return reinterpret_cast<ThemeInstallBubbleViewGtk*>(user_data)->
        HandleParentUnmap();
  }
  gboolean HandleParentUnmap();

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

#endif  // CHROME_BROWSER_GTK_THEME_INSTALL_BUBBLE_VIEW_GTK_H_
