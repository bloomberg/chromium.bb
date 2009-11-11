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
#include "net/base/cookie_monster.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

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
  static void Show(Profile* profile);

  // gtk_tree::TreeAdapter::Delegate implementation.
  virtual void OnAnyModelUpdateStart();
  virtual void OnAnyModelUpdate();

 private:
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
  void PopulateCookieDetails(const std::string& domain,
                             const net::CookieMonster::CanonicalCookie& cookie);

  // Reset the cookie details display.
  void ClearCookieDetails();

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

  // The parent widget.
  GtkWidget* dialog_;

  // Widgets of the dialog.
  GtkWidget* description_label_;
  GtkWidget* remove_button_;
  GtkWidget* remove_all_button_;

  // The table listing the cookies.
  GtkWidget* tree_;
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
  scoped_ptr<CookiesTreeModel> cookies_tree_model_;
  scoped_ptr<gtk_tree::TreeAdapter> cookies_tree_adapter_;

  friend class CookiesViewTest;
  FRIEND_TEST(CookiesViewTest, Empty);
  FRIEND_TEST(CookiesViewTest, Noop);
  FRIEND_TEST(CookiesViewTest, RemoveAll);
  FRIEND_TEST(CookiesViewTest, RemoveAllWithDefaultSelected);
  FRIEND_TEST(CookiesViewTest, Remove);
  FRIEND_TEST(CookiesViewTest, RemoveCookiesByDomain);
  FRIEND_TEST(CookiesViewTest, RemoveByDomain);
  FRIEND_TEST(CookiesViewTest, RemoveDefaultSelection);

  DISALLOW_COPY_AND_ASSIGN(CookiesView);
};

#endif  // CHROME_BROWSER_GTK_OPTIONS_COOKIES_VIEW_H_
