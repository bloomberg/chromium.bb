// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_PASSWORDS_EXCEPTIONS_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_PASSWORDS_EXCEPTIONS_HANDLER_H_

#include "chrome/browser/dom_ui/options_ui.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/profile.h"

class PasswordsExceptionsHandler : public OptionsPageUIHandler {
 public:
  PasswordsExceptionsHandler();
  virtual ~PasswordsExceptionsHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  virtual void Initialize();

  virtual void RegisterMessages();

 private:
  // The password store associated with the currently active profile.
  PasswordStore* GetPasswordStore();

  // Fired when user clicks 'show saved passwords' button in personal page.
  void LoadSavedPasswords(const ListValue* args);

  // Remove an entry.
  // @param value the entry index to be removed.
  void RemoveEntry(const ListValue* args);

  // Get password value for the selected entry.
  // @param value the selected entry index.
  void ShowSelectedPassword(const ListValue* args);

  // Sets the password list contents to the given data. We take ownership of
  // the PasswordForms in the vector.
  void SetPasswordList();

  // A short class to mediate requests to the password store.
  class PasswordListPopulater : public PasswordStoreConsumer {
   public:
    explicit PasswordListPopulater(PasswordsExceptionsHandler* page)
        : page_(page),
          pending_login_query_(0) {
    }

    // Send a query to the password store to populate a PasswordsPageGtk.
    void Populate();

    // Send the password store's reply back to the PasswordsPageGtk.
    virtual void OnPasswordStoreRequestDone(
        int handle, const std::vector<webkit_glue::PasswordForm*>& result);

   private:
    PasswordsExceptionsHandler* page_;
    int pending_login_query_;
  };

  // Password store consumer for populating the password list.
  PasswordListPopulater populater_;

  // A weak reference to profile.
  Profile* profile_;
  std::vector<webkit_glue::PasswordForm*> password_list_;

  DISALLOW_COPY_AND_ASSIGN(PasswordsExceptionsHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_PASSWORDS_EXCEPTIONS_HANDLER_H_
