// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the Gtk implementation of the collected Cookies dialog.

#ifndef CHROME_BROWSER_UI_GTK_COLLECTED_COOKIES_GTK_H_
#define CHROME_BROWSER_UI_GTK_COLLECTED_COOKIES_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_tree.h"
#include "chrome/common/content_settings.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"

class CookiesTreeModel;
class TabContentsWrapper;

// CollectedCookiesGtk is a dialog that displays the allowed and blocked
// cookies of the current tab contents.  To display the dialog, invoke
// ShowCollectedCookiesDialog() on the delegate of the tab contents wrapper's
// content settings tab helper.

class CollectedCookiesGtk : public ConstrainedWindowGtkDelegate,
                            public gtk_tree::TreeAdapter::Delegate,
                            public content::NotificationObserver {
 public:
  CollectedCookiesGtk(GtkWindow* parent, TabContentsWrapper* wrapper);

  // ConstrainedWindowGtkDelegate methods.
  virtual GtkWidget* GetWidgetRoot() OVERRIDE;
  virtual GtkWidget* GetFocusWidget() OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;

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
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Create the information panes for the allowed and blocked cookies.
  GtkWidget* CreateAllowedPane();
  GtkWidget* CreateBlockedPane();

  // Show information about selected cookie in the cookie info view.
  void ShowCookieInfo(gint current_page);
  void ShowSelectionInfo(GtkTreeSelection* selection,
                         gtk_tree::TreeAdapter* adapter);


  // Callbacks.
  CHROMEGTK_CALLBACK_2(CollectedCookiesGtk, void, OnTreeViewRowExpanded,
                       GtkTreeIter*, GtkTreePath*);
  CHROMEGTK_CALLBACK_0(CollectedCookiesGtk, void, OnTreeViewSelectionChange);
  CHROMEGTK_CALLBACK_0(CollectedCookiesGtk, void, OnClose);
  CHROMEGTK_CALLBACK_0(CollectedCookiesGtk, void, OnBlockAllowedButtonClicked);
  CHROMEGTK_CALLBACK_0(CollectedCookiesGtk, void, OnAllowBlockedButtonClicked);
  CHROMEGTK_CALLBACK_0(CollectedCookiesGtk, void,
                       OnForSessionBlockedButtonClicked);
  CHROMEGTK_CALLBACK_2(CollectedCookiesGtk, void, OnSwitchPage,
                       gpointer, guint);

  content::NotificationRegistrar registrar_;

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
  GtkWidget* notebook_;
  GtkWidget* allowed_tree_;
  GtkWidget* blocked_tree_;

  GtkTreeSelection* allowed_selection_;
  GtkTreeSelection* blocked_selection_;

  // The infobar widget.
  GtkWidget* infobar_;
  GtkWidget* infobar_label_;

  // Displays information about selected cookie.
  GtkWidget* cookie_info_view_;

  // The tab contents wrapper.
  TabContentsWrapper* wrapper_;

  bool status_changed_;

  // The Cookies Table model.
  scoped_ptr<CookiesTreeModel> allowed_cookies_tree_model_;
  scoped_ptr<CookiesTreeModel> blocked_cookies_tree_model_;
  scoped_ptr<gtk_tree::TreeAdapter> allowed_cookies_tree_adapter_;
  scoped_ptr<gtk_tree::TreeAdapter> blocked_cookies_tree_adapter_;

  DISALLOW_COPY_AND_ASSIGN(CollectedCookiesGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_COLLECTED_COOKIES_GTK_H_
