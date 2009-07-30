// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_OPTIONS_COOKIES_VIEW_H_
#define CHROME_BROWSER_GTK_OPTIONS_COOKIES_VIEW_H_

#include <gtk/gtk.h>

#include "app/table_model_observer.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/task.h"

class CookiesTableModel;
class Profile;

class CookiesView : public TableModelObserver {
 public:
  virtual ~CookiesView();

  // Create (if necessary) and show the keyword window window.
  static void Show(Profile* profile);

 private:
  explicit CookiesView(Profile* profile);

  // Initialize the dialog contents and layout.
  void Init();

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

  // Set the column values for |row| of |cookies_table_model_| in the
  // |list_store_| at |iter|.
  void SetColumnValues(int row, GtkTreeIter* iter);

  // Add the values from |row| of |cookies_table_model_|.
  void AddNodeToList(int row);

  // TableModelObserver implementation.
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);

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

  // Widgets of the dialog.
  GtkWidget* dialog_;
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
  GtkStyle* label_style_;
  GtkStyle* dialog_style_;
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

  // The Cookies Table model
  scoped_ptr<CookiesTableModel> cookies_table_model_;

  // A factory to construct Runnable Methods so that we can be called back to
  // re-evaluate the model after the search query string changes.
  ScopedRunnableMethodFactory<CookiesView> filter_update_factory_;

  DISALLOW_COPY_AND_ASSIGN(CookiesView);
};


#endif  // CHROME_BROWSER_GTK_OPTIONS_COOKIES_VIEW_H_
