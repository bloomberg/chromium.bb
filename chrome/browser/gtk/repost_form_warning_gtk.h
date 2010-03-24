// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_REPOST_FORM_WARNING_GTK_H_
#define CHROME_BROWSER_GTK_REPOST_FORM_WARNING_GTK_H_

#include <gtk/gtk.h>

#include "chrome/common/notification_registrar.h"
#include "chrome/browser/gtk/constrained_window_gtk.h"
#include "app/gtk_signal.h"

class NavigationController;
class TabContents;

// Displays a dialog that warns the user that they are about to resubmit a form.
// To display the dialog, allocate this object on the heap. It will open the
// dialog from its constructor and then delete itself when the user dismisses
// the dialog.
class RepostFormWarningGtk : public NotificationObserver,
    ConstrainedWindowGtkDelegate {
 public:
  RepostFormWarningGtk(GtkWindow* parent, TabContents* tab_contents);
  virtual ~RepostFormWarningGtk();

  virtual GtkWidget* GetWidgetRoot();

  virtual void DeleteDelegate();

 private:
  // NotificationObserver implementation.
  // Watch for a new load or a closed tab and dismiss the dialog if they occur.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Tell Gtk to destroy the dialog window.  This will only be done once, even
  // if Destroy is called multiple times.
  void Destroy();

  CHROMEGTK_CALLBACK_0(RepostFormWarningGtk, void, OnRefresh);
  CHROMEGTK_CALLBACK_0(RepostFormWarningGtk, void, OnCancel);
  CHROMEGTK_CALLBACK_1(RepostFormWarningGtk,
                       void,
                       OnHierarchyChanged,
                       GtkWidget*);

  NotificationRegistrar registrar_;

  // Navigation controller, used to continue the reload.
  NavigationController* navigation_controller_;

  GtkWidget* dialog_;
  GtkWidget* ok_;
  GtkWidget* cancel_;

  ConstrainedWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(RepostFormWarningGtk);
};

#endif  // CHROME_BROWSER_GTK_REPOST_FORM_WARNING_GTK_H_
