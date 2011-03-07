// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_OPTIONS_PASSWORDS_PAGE_GTK_H_
#define CHROME_BROWSER_UI_GTK_OPTIONS_PASSWORDS_PAGE_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/common/notification_observer.h"
#include "ui/base/gtk/gtk_signal.h"

class Profile;

class PasswordsPageGtk : public NotificationObserver {
 public:
  explicit PasswordsPageGtk(Profile* profile);
  virtual ~PasswordsPageGtk();

  GtkWidget* get_page_widget() const { return page_; }

 private:
  // Initialize the password tree widget, setting the member variables.
  void InitPasswordTree();

  // The password store associated with the currently active profile.
  PasswordStore* GetPasswordStore();

  // Sets the password list contents to the given data. We take ownership of
  // the PasswordForms in the vector.
  void SetPasswordList(const std::vector<webkit_glue::PasswordForm*>& result);

  // Helper that hides the password.
  void HidePassword();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Handles changes to the observed preferences and updates the UI.
  void OnPrefChanged(const std::string& pref_name);

  CHROMEGTK_CALLBACK_0(PasswordsPageGtk, void, OnRemoveButtonClicked);
  CHROMEGTK_CALLBACK_0(PasswordsPageGtk, void, OnRemoveAllButtonClicked);
  CHROMEGTK_CALLBACK_1(PasswordsPageGtk, void, OnRemoveAllConfirmResponse, int);
  CHROMEGTK_CALLBACK_0(PasswordsPageGtk, void, OnShowPasswordButtonClicked);
  CHROMEGTK_CALLBACK_0(PasswordsPageGtk, void, OnShowPasswordButtonRealized);

  CHROMEG_CALLBACK_0(PasswordsPageGtk, void, OnPasswordSelectionChanged,
                     GtkTreeSelection*);

  // Sorting functions.
  static gint CompareSite(GtkTreeModel* model,
                          GtkTreeIter* a, GtkTreeIter* b,
                          gpointer window);
  static gint CompareUsername(GtkTreeModel* model,
                              GtkTreeIter* a, GtkTreeIter* b,
                              gpointer window);

  // A short class to mediate requests to the password store.
  class PasswordListPopulater : public PasswordStoreConsumer {
   public:
    explicit PasswordListPopulater(PasswordsPageGtk* page)
        : page_(page),
          pending_login_query_(0) {
    }

    // Send a query to the password store to populate a PasswordsPageGtk.
    void populate();

    // Send the password store's reply back to the PasswordsPageGtk.
    virtual void OnPasswordStoreRequestDone(
        int handle, const std::vector<webkit_glue::PasswordForm*>& result);

   private:
    PasswordsPageGtk* page_;
    int pending_login_query_;
  };

  // Password store consumer for populating the password list.
  PasswordListPopulater populater;

  // Widgets for the buttons.
  GtkWidget* remove_button_;
  GtkWidget* remove_all_button_;
  GtkWidget* show_password_button_;

  // Widget for the shown password
  GtkWidget* password_;
  bool password_showing_;

  // Widgets for the password table.
  GtkWidget* password_tree_;
  GtkListStore* password_list_store_;
  GtkTreeModel* password_list_sort_;
  GtkTreeSelection* password_selection_;

  // The parent GtkHBox widget and GtkWindow window.
  GtkWidget* page_;

  Profile* profile_;
  BooleanPrefMember allow_show_passwords_;
  std::vector<webkit_glue::PasswordForm*> password_list_;

  DISALLOW_COPY_AND_ASSIGN(PasswordsPageGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_OPTIONS_PASSWORDS_PAGE_GTK_H_
