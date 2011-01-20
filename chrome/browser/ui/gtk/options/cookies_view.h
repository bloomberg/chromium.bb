// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the Gtk implementation of the Cookie Manager dialog.

#ifndef CHROME_BROWSER_UI_GTK_OPTIONS_COOKIES_VIEW_H_
#define CHROME_BROWSER_UI_GTK_OPTIONS_COOKIES_VIEW_H_
#pragma once

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "chrome/browser/ui/gtk/gtk_chrome_cookie_view.h"
#include "chrome/browser/ui/gtk/gtk_tree.h"
#include "net/base/cookie_monster.h"
#include "ui/base/gtk/gtk_signal.h"

class CookieDisplayGtk;
class CookiesTreeModel;
class CookiesViewTest;
class Profile;

// CookiesView is thread-hostile, and should only be called on the UI thread.
// Usage:
//   CookiesView::Show(profile);
// Once the CookiesView is shown, it is responsible for deleting itself when the
// user closes the dialog.

class CookiesView : public gtk_tree::TreeAdapter::Delegate {
 public:
  virtual ~CookiesView();

  // Create (if necessary) and show the cookie manager window.
  static void Show(
      GtkWindow* parent,
      Profile* profile,
      BrowsingDataDatabaseHelper* browsing_data_database_helper,
      BrowsingDataLocalStorageHelper* browsing_data_local_storage_helper,
      BrowsingDataAppCacheHelper* browsing_data_appcache_helper,
      BrowsingDataIndexedDBHelper* browsing_data_indexed_db_helper);

  // gtk_tree::TreeAdapter::Delegate implementation.
  virtual void OnAnyModelUpdateStart();
  virtual void OnAnyModelUpdate();

 private:
  CookiesView(
      GtkWindow* parent,
      Profile* profile,
      BrowsingDataDatabaseHelper* browsing_data_database_helper,
      BrowsingDataLocalStorageHelper* browsing_data_local_storage_helper,
      BrowsingDataAppCacheHelper* browsing_data_appcache_helper,
      BrowsingDataIndexedDBHelper* browsing_data_indexed_db_helper);

  // A method only used in unit tests that sets a bit inside this class that
  // lets it be stack allocated.
  void TestDestroySynchronously();

  // Initialize the dialog contents and layout.
  void Init(GtkWindow* parent);

  // Set the initial selection and tree expanded state.
  void SetInitialTreeState();

  // Set sensitivity of buttons based on selection and filter state.
  void EnableControls();

  // Remove any cookies that are currently selected.
  void RemoveSelectedItems();

  CHROMEGTK_CALLBACK_1(CookiesView, void, OnResponse, int);
  CHROMEGTK_CALLBACK_0(CookiesView, void, OnWindowDestroy);
  // Callback for the table.
  CHROMEGTK_CALLBACK_0(CookiesView, void, OnTreeViewSelectionChange);
  CHROMEGTK_CALLBACK_1(CookiesView, gboolean, OnTreeViewKeyPress,
                       GdkEventKey*);
  CHROMEGTK_CALLBACK_2(CookiesView, void, OnTreeViewRowExpanded,
                       GtkTreeIter*, GtkTreePath*);
  // Callbacks for user actions filtering the list.
  CHROMEGTK_CALLBACK_0(CookiesView, void, OnFilterEntryActivated);
  CHROMEGTK_CALLBACK_0(CookiesView, void, OnFilterEntryChanged);
  CHROMEGTK_CALLBACK_0(CookiesView, void, OnFilterClearButtonClicked);

  // Filter the list against the text in |filter_entry_|.
  void UpdateFilterResults();


  // The parent widget.
  GtkWidget* dialog_;

  // Widgets of the dialog.
  GtkWidget* description_label_;
  GtkWidget* filter_entry_;
  GtkWidget* filter_clear_button_;
  GtkWidget* remove_button_;
  GtkWidget* remove_all_button_;

  // The table listing the cookies.
  GtkWidget* tree_;
  GtkTreeSelection* selection_;

  GtkWidget* cookie_display_;

  // The profile and related helpers.
  Profile* profile_;
  scoped_refptr<BrowsingDataDatabaseHelper> browsing_data_database_helper_;
  scoped_refptr<BrowsingDataLocalStorageHelper>
      browsing_data_local_storage_helper_;
  scoped_refptr<BrowsingDataAppCacheHelper> browsing_data_appcache_helper_;
  scoped_refptr<BrowsingDataIndexedDBHelper> browsing_data_indexed_db_helper_;

  // A factory to construct Runnable Methods so that we can be called back to
  // re-evaluate the model after the search query string changes.
  ScopedRunnableMethodFactory<CookiesView> filter_update_factory_;

  // The Cookies Table model.
  scoped_ptr<CookiesTreeModel> cookies_tree_model_;
  scoped_ptr<gtk_tree::TreeAdapter> cookies_tree_adapter_;

  // A reference to the "destroy" signal handler for this object. We disconnect
  // from this signal if we need to be destroyed synchronously.
  gulong destroy_handler_;

  // Whether we own |dialog_| or the other way around.
  bool destroy_dialog_in_destructor_;

  friend class CookiesViewTest;
  FRIEND_TEST_ALL_PREFIXES(CookiesViewTest, Empty);
  FRIEND_TEST_ALL_PREFIXES(CookiesViewTest, Noop);
  FRIEND_TEST_ALL_PREFIXES(CookiesViewTest, RemoveAll);
  FRIEND_TEST_ALL_PREFIXES(CookiesViewTest, RemoveAllWithDefaultSelected);
  FRIEND_TEST_ALL_PREFIXES(CookiesViewTest, Remove);
  FRIEND_TEST_ALL_PREFIXES(CookiesViewTest, RemoveCookiesByType);
  FRIEND_TEST_ALL_PREFIXES(CookiesViewTest, RemoveByDomain);
  FRIEND_TEST_ALL_PREFIXES(CookiesViewTest, RemoveDefaultSelection);
  FRIEND_TEST_ALL_PREFIXES(CookiesViewTest, Filter);
  FRIEND_TEST_ALL_PREFIXES(CookiesViewTest, FilterRemoveAll);
  FRIEND_TEST_ALL_PREFIXES(CookiesViewTest, FilterRemove);

  DISALLOW_COPY_AND_ASSIGN(CookiesView);
};

#endif  // CHROME_BROWSER_UI_GTK_OPTIONS_COOKIES_VIEW_H_
