// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the Gtk implementation of the collected Cookies dialog.

#ifndef CHROME_BROWSER_GTK_COLLECTED_COOKIES_GTK_H_
#define CHROME_BROWSER_GTK_COLLECTED_COOKIES_GTK_H_

#include <gtk/gtk.h>

#include "app/gtk_signal.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/gtk/constrained_window_gtk.h"
#include "chrome/browser/gtk/gtk_tree.h"
#include "chrome/common/notification_registrar.h"

class CookiesTreeModel;

// CollectedCookiesGtk is a dialog that displays the allowed and blocked
// cookies of the current tab contents.  To display the dialog, invoke
// ShowCollectedCookiesDialog() on the delegate of the tab contents.

class CollectedCookiesGtk : public ConstrainedDialogDelegate,
                                   gtk_tree::TreeAdapter::Delegate,
                                   NotificationObserver {
 public:
  CollectedCookiesGtk(GtkWindow* parent, TabContents* tab_contents);

  // ConstrainedDialogDelegate methods.
  virtual GtkWidget* GetWidgetRoot();
  virtual void DeleteDelegate();

 private:
  virtual ~CollectedCookiesGtk();

  void Init();

  // Notification Observer implementation.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

  // Callbacks.
  CHROMEGTK_CALLBACK_2(CollectedCookiesGtk, void, OnTreeViewRowExpanded,
                       GtkTreeIter*, GtkTreePath*);
  CHROMEGTK_CALLBACK_0(CollectedCookiesGtk, void, OnClose);

  NotificationRegistrar registrar_;

  ConstrainedWindow* window_;

  // Widgets of the dialog.
  GtkWidget* dialog_;

  GtkWidget* allowed_description_label_;
  GtkWidget* blocked_description_label_;

  // The table listing the cookies.
  GtkWidget* allowed_tree_;
  GtkWidget* blocked_tree_;

  GtkWidget* allowed_cookie_display_;
  GtkWidget* blocked_cookie_display_;

  // The tab contents.
  TabContents* tab_contents_;

  // The Cookies Table model.
  scoped_ptr<CookiesTreeModel> allowed_cookies_tree_model_;
  scoped_ptr<CookiesTreeModel> blocked_cookies_tree_model_;
  scoped_ptr<gtk_tree::TreeAdapter> allowed_cookies_tree_adapter_;
  scoped_ptr<gtk_tree::TreeAdapter> blocked_cookies_tree_adapter_;

  DISALLOW_COPY_AND_ASSIGN(CollectedCookiesGtk);
};

#endif  // CHROME_BROWSER_GTK_COLLECTED_COOKIES_GTK_H_
