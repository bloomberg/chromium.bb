// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the Gtk implementation of the Cookie Manager dialog.

#ifndef CHROME_BROWSER_GTK_OPTIONS_COOKIES_VIEW_H_
#define CHROME_BROWSER_GTK_OPTIONS_COOKIES_VIEW_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "chrome/browser/gtk/gtk_tree.h"
#include "net/base/cookie_monster.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

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
      BrowsingDataAppCacheHelper* browsing_data_appcache_helper);

  // gtk_tree::TreeAdapter::Delegate implementation.
  virtual void OnAnyModelUpdateStart();
  virtual void OnAnyModelUpdate();

 private:
  CookiesView(
      GtkWindow* parent,
      Profile* profile,
      BrowsingDataDatabaseHelper* browsing_data_database_helper,
      BrowsingDataLocalStorageHelper* browsing_data_local_storage_helper,
      BrowsingDataAppCacheHelper* browsing_data_appcache_helper);

  // Initialize the dialog contents and layout.
  void Init(GtkWindow* parent);

  // Set the initial selection and tree expanded state.
  void SetInitialTreeState();

  // Set sensitivity of buttons based on selection and filter state.
  void EnableControls();

  // Remove any cookies that are currently selected.
  void RemoveSelectedItems();

  // Callback for dialog buttons.
  static void OnResponse(GtkDialog* dialog, int response_id,
                         CookiesView* window);

  // Callback for window destruction.
  static void OnWindowDestroy(GtkWidget* widget, CookiesView* window);

  // Callback for when user selects something in the table.
  static void OnSelectionChanged(GtkTreeSelection *selection,
                                 CookiesView* window);

  // Callback for when user presses a key with the table focused.
  static gboolean OnTreeViewKeyPress(GtkWidget* tree_view, GdkEventKey* key,
                                     CookiesView* window);

  // Callback when user expands a row in the table.
  static void OnTreeViewRowExpanded(GtkTreeView* tree_view, GtkTreeIter* iter,
                                    GtkTreePath* path, gpointer user_data);

  // Filter the list against the text in |filter_entry_|.
  void UpdateFilterResults();

  // Callbacks for user actions filtering the list.
  static void OnFilterEntryActivated(GtkEntry* entry, CookiesView* window);
  static void OnFilterEntryChanged(GtkEditable* editable, CookiesView* window);
  static void OnFilterClearButtonClicked(GtkButton* button,
                                         CookiesView* window);

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

  scoped_ptr<CookieDisplayGtk> cookie_display_gtk_;

  // The profile and related helpers.
  Profile* profile_;
  scoped_refptr<BrowsingDataDatabaseHelper> browsing_data_database_helper_;
  scoped_refptr<BrowsingDataLocalStorageHelper>
      browsing_data_local_storage_helper_;
  scoped_refptr<BrowsingDataAppCacheHelper> browsing_data_appcache_helper_;

  // A factory to construct Runnable Methods so that we can be called back to
  // re-evaluate the model after the search query string changes.
  ScopedRunnableMethodFactory<CookiesView> filter_update_factory_;

  // The Cookies Table model.
  scoped_ptr<CookiesTreeModel> cookies_tree_model_;
  scoped_ptr<gtk_tree::TreeAdapter> cookies_tree_adapter_;

  friend class CookiesViewTest;
  FRIEND_TEST(CookiesViewTest, Empty);
  FRIEND_TEST(CookiesViewTest, Noop);
  FRIEND_TEST(CookiesViewTest, RemoveAll);
  FRIEND_TEST(CookiesViewTest, RemoveAllWithDefaultSelected);
  FRIEND_TEST(CookiesViewTest, Remove);
  FRIEND_TEST(CookiesViewTest, RemoveCookiesByType);
  FRIEND_TEST(CookiesViewTest, RemoveByDomain);
  FRIEND_TEST(CookiesViewTest, RemoveDefaultSelection);
  FRIEND_TEST(CookiesViewTest, Filter);
  FRIEND_TEST(CookiesViewTest, FilterRemoveAll);
  FRIEND_TEST(CookiesViewTest, FilterRemove);

  DISALLOW_COPY_AND_ASSIGN(CookiesView);
};

#endif  // CHROME_BROWSER_GTK_OPTIONS_COOKIES_VIEW_H_
