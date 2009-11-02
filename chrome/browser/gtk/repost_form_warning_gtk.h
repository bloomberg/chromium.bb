// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_REPOST_FORM_WARNING_GTK_H_
#define CHROME_BROWSER_GTK_REPOST_FORM_WARNING_GTK_H_

#include <gtk/gtk.h>

#include "chrome/common/notification_registrar.h"

class NavigationController;

// Displays a dialog that warns the user that they are about to resubmit a form.
// To display the dialog, allocate this object on the heap. It will open the
// dialog from its constructor and then delete itself when the user dismisses
// the dialog.
class RepostFormWarningGtk : public NotificationObserver {
 public:
  RepostFormWarningGtk(GtkWindow* parent,
                       NavigationController* navigation_controller);
  virtual ~RepostFormWarningGtk();

 private:
  // NotificationObserver implementation.
  // Watch for a new load or a closed tab and dismiss the dialog if they occur.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Tell Gtk to destroy the dialog window.  This will only be done once, even
  // if Destroy is called multiple times (eg, from both OnResponse and Observe.)
  void Destroy();

  static void OnResponse(GtkWidget* widget,
                         int response,
                         RepostFormWarningGtk* dialog);
  static void OnWindowDestroy(GtkWidget* widget, RepostFormWarningGtk* dialog);

  NotificationRegistrar registrar_;

  // Navigation controller, used to continue the reload.
  NavigationController* navigation_controller_;

  GtkWidget* dialog_;

  DISALLOW_COPY_AND_ASSIGN(RepostFormWarningGtk);
};

#endif  // CHROME_BROWSER_GTK_REPOST_FORM_WARNING_GTK_H_
