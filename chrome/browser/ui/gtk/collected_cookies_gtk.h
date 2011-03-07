// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the Gtk implementation of the collected Cookies dialog.

#ifndef CHROME_BROWSER_UI_GTK_COLLECTED_COOKIES_GTK_H_
#define CHROME_BROWSER_UI_GTK_COLLECTED_COOKIES_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/scoped_ptr.h"
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_tree.h"
#include "chrome/common/content_settings.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"

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
  virtual GtkWidget* GetFocusWidget();
  virtual void DeleteDelegate();

 private:
  virtual ~CollectedCookiesGtk();

  // Initialize all widgets of this dialog.
  void Init();

  // True if the selection contains at least one origin node.
  bool SelectionContainsOriginNode(GtkTreeSelection* selection,
                                   gtk_tree::TreeAdapter* adapter);

  // Enable the allow/block buttons if at least one origin node is selected.
  void EnableControls();

  // Add exceptions for all origin nodes within the selection.
  void AddExceptions(GtkTreeSelection* selection,
                     gtk_tree::TreeAdapter* adapter,
                     ContentSetting setting);

  // Notification Observer implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Callbacks.
  CHROMEGTK_CALLBACK_2(CollectedCookiesGtk, void, OnTreeViewRowExpanded,
                       GtkTreeIter*, GtkTreePath*);
  CHROMEGTK_CALLBACK_0(CollectedCookiesGtk, void, OnTreeViewSelectionChange);
  CHROMEGTK_CALLBACK_0(CollectedCookiesGtk, void, OnClose);
  CHROMEGTK_CALLBACK_0(CollectedCookiesGtk, void, OnBlockAllowedButtonClicked);
  CHROMEGTK_CALLBACK_0(CollectedCookiesGtk, void, OnAllowBlockedButtonClicked);
  CHROMEGTK_CALLBACK_0(CollectedCookiesGtk, void,
                       OnForSessionBlockedButtonClicked);

  NotificationRegistrar registrar_;

  ConstrainedWindow* window_;

  // Widgets of the dialog.
  GtkWidget* dialog_;

  GtkWidget* allowed_description_label_;
  GtkWidget* blocked_description_label_;

  GtkWidget* block_allowed_cookie_button_;

  GtkWidget* allow_blocked_cookie_button_;
  GtkWidget* for_session_blocked_cookie_button_;
  GtkWidget* close_button_;

  // The table listing the cookies.
  GtkWidget* allowed_tree_;
  GtkWidget* blocked_tree_;

  GtkTreeSelection* allowed_selection_;
  GtkTreeSelection* blocked_selection_;

  // The infobar widget.
  GtkWidget* infobar_;
  GtkWidget* infobar_label_;

  // The tab contents.
  TabContents* tab_contents_;

  // The Cookies Table model.
  scoped_ptr<CookiesTreeModel> allowed_cookies_tree_model_;
  scoped_ptr<CookiesTreeModel> blocked_cookies_tree_model_;
  scoped_ptr<gtk_tree::TreeAdapter> allowed_cookies_tree_adapter_;
  scoped_ptr<gtk_tree::TreeAdapter> blocked_cookies_tree_adapter_;

  DISALLOW_COPY_AND_ASSIGN(CollectedCookiesGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_COLLECTED_COOKIES_GTK_H_
