// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_OPTIONS_PASSWORDS_EXCEPTIONS_PAGE_GTK_H_
#define CHROME_BROWSER_UI_GTK_OPTIONS_PASSWORDS_EXCEPTIONS_PAGE_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include <vector>

#include "chrome/browser/password_manager/password_store.h"
#include "ui/base/gtk/gtk_signal.h"

class Profile;

// A page in the show saved passwords dialog that lists what sites we never
// show passwords for, with controls for the user to add/remove sites from that
// list.
class PasswordsExceptionsPageGtk {
 public:
  explicit PasswordsExceptionsPageGtk(Profile* profile);
  virtual ~PasswordsExceptionsPageGtk();

  GtkWidget* get_page_widget() const { return page_; }

 private:
  // Initialize the exception tree widget, setting the member variables.
  void InitExceptionTree();

  // The password store associated with the currently active profile.
  PasswordStore* GetPasswordStore();

  // Sets the exception list contents to the given data. We take ownership of
  // the PasswordForms in the vector.
  void SetExceptionList(const std::vector<webkit_glue::PasswordForm*>& result);

  CHROMEGTK_CALLBACK_0(PasswordsExceptionsPageGtk, void, OnRemoveButtonClicked);
  CHROMEGTK_CALLBACK_0(PasswordsExceptionsPageGtk, void,
                       OnRemoveAllButtonClicked);

  CHROMEG_CALLBACK_0(PasswordsExceptionsPageGtk, void,
                     OnExceptionSelectionChanged, GtkTreeSelection*);

  // Sorting function.
  static gint CompareSite(GtkTreeModel* model,
                          GtkTreeIter* a, GtkTreeIter* b,
                          gpointer window);

  // A short class to mediate requests to the password store.
  class ExceptionListPopulater : public PasswordStoreConsumer {
   public:
    explicit ExceptionListPopulater(PasswordsExceptionsPageGtk* page)
        : page_(page),
          pending_login_query_(0) {
    }

    // Send a query to the password store to populate an
    // PasswordsExceptionsPageGtk.
    void populate();

    // PasswordStoreConsumer implementation.
    // Send the password store's reply back to the PasswordsExceptionsPageGtk.
    virtual void OnPasswordStoreRequestDone(
        int handle, const std::vector<webkit_glue::PasswordForm*>& result);

   private:
    PasswordsExceptionsPageGtk* page_;
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
  std::vector<webkit_glue::PasswordForm*> exception_list_;

  DISALLOW_COPY_AND_ASSIGN(PasswordsExceptionsPageGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_OPTIONS_PASSWORDS_EXCEPTIONS_PAGE_GTK_H_
