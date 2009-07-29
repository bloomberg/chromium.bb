// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_OPTIONS_EXCEPTIONS_PAGE_GTK_H_
#define CHROME_BROWSER_GTK_OPTIONS_EXCEPTIONS_PAGE_GTK_H_

#include <gtk/gtk.h>

#include <vector>

#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/profile.h"

class ExceptionsPageGtk {
 public:
  explicit ExceptionsPageGtk(Profile* profile);
  ~ExceptionsPageGtk();

  GtkWidget* get_page_widget() const {
    return page_;
  }

 private:
  // Initialize the exception tree widget, setting the member variables.
  void InitExceptionTree();

  // The password store associated with the currently active profile.
  PasswordStore* GetPasswordStore();

  // Sets the exception list contents to the given data.
  void SetExceptionList(const std::vector<webkit_glue::PasswordForm*>& result);

  // Callback for the remove button.
  static void OnRemoveButtonClicked(GtkButton* widget, ExceptionsPageGtk* page);

  // Callback for the remove all button.
  static void OnRemoveAllButtonClicked(GtkButton* widget,
                                       ExceptionsPageGtk* page);

  // Callback for selection changed events.
  static void OnExceptionSelectionChanged(GtkTreeSelection* selection,
                                          ExceptionsPageGtk* page);

  // Sorting function.
  static gint CompareSite(GtkTreeModel* model,
                          GtkTreeIter* a, GtkTreeIter* b,
                          gpointer window);

  // A short class to mediate requests to the password store.
  class ExceptionListPopulater : public PasswordStoreConsumer {
   public:
    explicit ExceptionListPopulater(ExceptionsPageGtk* page) : page_(page) {
    }

    // Send a query to the password store to populate an ExceptionsPageGtk.
    void populate();

    // Send the password store's reply back to the ExceptionsPageGtk.
    virtual void OnPasswordStoreRequestDone(
        int handle, const std::vector<webkit_glue::PasswordForm*>& result);

   private:
    ExceptionsPageGtk* page_;
    int pending_login_query_;
  };

  // Password store consumer for populating the exception list.
  ExceptionListPopulater populater;

  // Widgets for the buttons.
  GtkWidget* remove_button_;
  GtkWidget* remove_all_button_;

  // Widgets for the exception table.
  GtkWidget* exception_tree_;
  GtkListStore* exception_list_store_;
  GtkTreeModel* exception_list_sort_;
  GtkTreeSelection* exception_selection_;

  // The parent GtkHBox widget.
  GtkWidget* page_;

  Profile* profile_;
  std::vector<webkit_glue::PasswordForm> exception_list_;

  DISALLOW_COPY_AND_ASSIGN(ExceptionsPageGtk);
};

#endif  // CHROME_BROWSER_GTK_OPTIONS_EXCEPTIONS_PAGE_GTK_H_
