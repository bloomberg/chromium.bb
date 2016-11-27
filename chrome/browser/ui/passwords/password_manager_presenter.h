// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_MANAGER_PRESENTER_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_MANAGER_PRESENTER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/prefs/pref_member.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace autofill {
struct PasswordForm;
}

// Multimap from sort key to password forms.
using DuplicatesMap =
    std::multimap<std::string, std::unique_ptr<autofill::PasswordForm>>;

enum class PasswordEntryType { SAVED, BLACKLISTED };

class PasswordUIView;

// Contains the common logic used by a PasswordUIView to
// interact with PasswordStore. It provides completion callbacks for
// PasswordStore operations and updates the view on PasswordStore changes.
class PasswordManagerPresenter
    : public password_manager::PasswordStore::Observer {
 public:
  // |password_view| the UI view that owns this presenter, must not be NULL.
  explicit PasswordManagerPresenter(PasswordUIView* password_view);
  ~PasswordManagerPresenter() override;

  // PasswordStore::Observer implementation.
  void OnLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  // Repopulates the password and exception entries.
  void UpdatePasswordLists();

  void Initialize();

  // Gets the password entry at |index|.
  const autofill::PasswordForm* GetPassword(size_t index);

  // Gets all password entries.
  std::vector<std::unique_ptr<autofill::PasswordForm>> GetAllPasswords();

  // Gets the password exception entry at |index|.
  const autofill::PasswordForm* GetPasswordException(size_t index);

  // Removes the saved password entry at |index|.
  // |index| the entry index to be removed.
  void RemoveSavedPassword(size_t index);

  // Removes the saved password exception entry at |index|.
  // |index| the entry index to be removed.
  void RemovePasswordException(size_t index);

  // Requests the plain text password for entry at |index| to be revealed.
  // |index| The index of the entry.
  void RequestShowPassword(size_t index);

  // Returns true if the user is authenticated.
  virtual bool IsUserAuthenticated();

 private:
  friend class PasswordManagerPresenterTest;

  // Sets the password and exception list of the UI view.
  void SetPasswordList();
  void SetPasswordExceptionList();

  // Sort entries of |list| based on sort key. The key is the concatenation of
  // origin, entry type (non-Android credential, Android w/ affiliated web realm
  // or Android w/o affiliated web realm). If |entry_type == SAVED|,
  // username, password and federation are also included in sort key. If there
  // are several forms with the same key, all such forms but the first one are
  // stored in |duplicates| instead of |list|.
  void SortEntriesAndHideDuplicates(
      std::vector<std::unique_ptr<autofill::PasswordForm>>* list,
      DuplicatesMap* duplicates,
      PasswordEntryType entry_type);

  // Returns the password store associated with the currently active profile.
  password_manager::PasswordStore* GetPasswordStore();

  // A short class to mediate requests to the password store.
  class ListPopulater : public password_manager::PasswordStoreConsumer {
   public:
    explicit ListPopulater(PasswordManagerPresenter* page);
    ~ListPopulater() override;

    // Send a query to the password store to populate a list.
    virtual void Populate() = 0;

   protected:
    PasswordManagerPresenter* page_;
  };

  // A short class to mediate requests to the password store for passwordlist.
  class PasswordListPopulater : public ListPopulater {
   public:
    explicit PasswordListPopulater(PasswordManagerPresenter* page);

    // Send a query to the password store to populate a password list.
    void Populate() override;

    // Send the password store's reply back to the handler.
    void OnGetPasswordStoreResults(
        std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;
  };

  // A short class to mediate requests to the password store for exceptions.
  class PasswordExceptionListPopulater : public ListPopulater {
   public:
    explicit PasswordExceptionListPopulater(PasswordManagerPresenter* page);

    // Send a query to the password store to populate a passwordException list.
    void Populate() override;

    // Send the password store's reply back to the handler.
    void OnGetPasswordStoreResults(
        std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;
  };

  // Password store consumer for populating the password list and exceptions.
  PasswordListPopulater populater_;
  PasswordExceptionListPopulater exception_populater_;

  std::vector<std::unique_ptr<autofill::PasswordForm>> password_list_;
  std::vector<std::unique_ptr<autofill::PasswordForm>> password_exception_list_;
  DuplicatesMap password_duplicates_;
  DuplicatesMap password_exception_duplicates_;

  // Whether to show stored passwords or not.
  BooleanPrefMember show_passwords_;

  // The last time the user was successfully authenticated.
  // Used to determine whether or not to reveal plaintext passwords.
  base::TimeTicks last_authentication_time_;

  // UI view that owns this presenter.
  PasswordUIView* password_view_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerPresenter);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_MANAGER_PRESENTER_H_
