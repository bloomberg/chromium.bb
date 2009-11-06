// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the Gtk implementation of the Cookie Manager dialog.

#ifndef CHROME_BROWSER_GTK_OPTIONS_COOKIES_VIEW_H_
#define CHROME_BROWSER_GTK_OPTIONS_COOKIES_VIEW_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/common/gtk_tree.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class CookiesTableModel;
class CookiesViewTest;
class Profile;

// CookiesView is thread-hostile, and should only be called on the UI thread.
// Usage:
//   CookiesView::Show(profile);
// Once the CookiesView is shown, it is responsible for deleting itself when the
// user closes the dialog.

class CookiesView : public gtk_tree::TableAdapter::Delegate {
 public:
  virtual ~CookiesView();

  // Create (if necessary) and show the cookie manager window.
  static void Show(Profile* profile);

  // gtk_tree::TableAdapter::Delegate implementation.
  virtual void OnAnyModelUpdateStart();
  virtual void OnAnyModelUpdate();
  virtual void SetColumnValues(int row, GtkTreeIter* iter);

 private:
  // Column ids for |list_store_|.
  enum {
    COL_SITE,
    COL_COOKIE_NAME,
    COL_COUNT,
  };

  explicit CookiesView(Profile* profile);

  // Initialize the dialog contents and layout.
  void Init();

  // Initialize the widget styles and display the dialog.
  void InitStylesAndShow();

  // Helper for initializing cookie details table.
  void InitCookieDetailRow(int row, int label_id, GtkWidget** display_label);

  // Set sensitivity of buttons based on selection and filter state.
  void EnableControls();

  // Set sensitivity of cookie details.
  void SetCookieDetailsSensitivity(gboolean enabled);

  // Show the details of the currently selected cookie.
  void PopulateCookieDetails();

  // Reset the cookie details display.
  void ClearCookieDetails();

  // Remove any cookies that are currently selected.
  void RemoveSelectedCookies();

  // Compare the value of the given column at the given rows.
  gint CompareRows(GtkTreeModel* model, GtkTreeIter* a, GtkTreeIter* b,
                   int column_id);

  // List sorting callbacks.
  static gint CompareSite(GtkTreeModel* model, GtkTreeIter* a, GtkTreeIter* b,
                          gpointer window);
  static gint CompareCookieName(GtkTreeModel* model, GtkTreeIter* a,
                                GtkTreeIter* b, gpointer window);

  // Callback for dialog buttons.
  static void OnResponse(GtkDialog* dialog, int response_id,
                         CookiesView* window);

  // Callback for window destruction.
  static void OnWindowDestroy(GtkWidget* widget, CookiesView* window);

  // Callback for when user selects something in the table.
  static void OnSelectionChanged(GtkTreeSelection *selection,
                                 CookiesView* window);

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
  GtkListStore* list_store_;
  GtkTreeModel* list_sort_;
  GtkTreeSelection* selection_;

  // The cookie details widgets.
  GtkWidget* cookie_details_table_;
  GtkWidget* cookie_name_entry_;
  GtkWidget* cookie_content_entry_;
  GtkWidget* cookie_domain_entry_;
  GtkWidget* cookie_path_entry_;
  GtkWidget* cookie_send_for_entry_;
  GtkWidget* cookie_created_entry_;
  GtkWidget* cookie_expires_entry_;

  // The profile.
  Profile* profile_;

  // The Cookies Table model.
  scoped_ptr<CookiesTableModel> cookies_table_model_;
  scoped_ptr<gtk_tree::TableAdapter> cookies_table_adapter_;

  // A factory to construct Runnable Methods so that we can be called back to
  // re-evaluate the model after the search query string changes.
  ScopedRunnableMethodFactory<CookiesView> filter_update_factory_;

  friend class CookiesViewTest;
  FRIEND_TEST(CookiesViewTest, Empty);
  FRIEND_TEST(CookiesViewTest, RemoveAll);
  FRIEND_TEST(CookiesViewTest, RemoveAllWithAllSelected);
  FRIEND_TEST(CookiesViewTest, Remove);
  FRIEND_TEST(CookiesViewTest, RemoveMultiple);
  FRIEND_TEST(CookiesViewTest, RemoveDefaultSelection);
  FRIEND_TEST(CookiesViewTest, Filter);
  FRIEND_TEST(CookiesViewTest, FilterRemoveAll);
  FRIEND_TEST(CookiesViewTest, FilterRemove);
  FRIEND_TEST(CookiesViewTest, Sort);
  FRIEND_TEST(CookiesViewTest, SortRemove);
  FRIEND_TEST(CookiesViewTest, SortFilterRemove);
  FRIEND_TEST(CookiesViewTest, SortRemoveMultiple);
  FRIEND_TEST(CookiesViewTest, SortRemoveDefaultSelection);

  DISALLOW_COPY_AND_ASSIGN(CookiesView);
};

#endif  // CHROME_BROWSER_GTK_OPTIONS_COOKIES_VIEW_H_
