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
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/ui/credential_provider_interface.h"
#include "components/prefs/pref_member.h"
#include "components/undo/undo_manager.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace autofill {
struct PasswordForm;
}

class PasswordUIView;

// Contains the common logic used by a PasswordUIView to
// interact with PasswordStore. It provides completion callbacks for
// PasswordStore operations and updates the view on PasswordStore changes.
class PasswordManagerPresenter
    : public password_manager::PasswordStore::Observer,
      public password_manager::PasswordStoreConsumer,
      public password_manager::CredentialProviderInterface {
 public:
  // |password_view| the UI view that owns this presenter, must not be NULL.
  explicit PasswordManagerPresenter(PasswordUIView* password_view);
  ~PasswordManagerPresenter() override;

  void Initialize();

  // PasswordStore::Observer implementation.
  void OnLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  // Repopulates the password and exception entries.
  void UpdatePasswordLists();

  // Gets the password entry at |index|.
  // TODO(https://crbug.com/892260): Take a SortKey as argument instead.
  const autofill::PasswordForm* GetPassword(size_t index);

  // password::manager::CredentialProviderInterface:
  std::vector<std::unique_ptr<autofill::PasswordForm>> GetAllPasswords()
      override;

  // Gets the password exception entry at |index|.
  // TODO(https://crbug.com/892260): Take a SortKey as argument instead.
  const autofill::PasswordForm* GetPasswordException(size_t index);

  // Removes the saved password entry at |index|.
  // |index| the entry index to be removed.
  // TODO(https://crbug.com/892260): Take a SortKey as argument instead.
  void RemoveSavedPassword(size_t index);

  // Removes the saved password exception entry at |index|.
  // |index| the entry index to be removed.
  // TODO(https://crbug.com/892260): Take a SortKey as argument instead.
  void RemovePasswordException(size_t index);

  // Undoes the last saved password or exception removal.
  void UndoRemoveSavedPasswordOrException();

  // Requests the plain text password for entry at |index| to be revealed.
  // |index| The index of the entry.
  // TODO(https://crbug.com/892260): Take a SortKey as argument instead.
  void RequestShowPassword(size_t index);

  // Wrapper around |PasswordStore::AddLogin| that adds the corresponding undo
  // action to |undo_manager_|.
  void AddLogin(const autofill::PasswordForm& form);

  // Wrapper around |PasswordStore::RemoveLogin| that adds the corresponding
  // undo action to |undo_manager_|.
  void RemoveLogin(const autofill::PasswordForm& form);

 private:
  // Convenience typedef for a map containing PasswordForms grouped into
  // equivalence classes. Each equivalence class corresponds to one entry shown
  // in the UI, and deleting an UI entry will delete all PasswordForms that are
  // a member of the corresponding equivalence class. The keys of the map are
  // sort keys, obtained by password_manager::CreateSortKey(). Each value of the
  // map contains forms with the same sort key.
  using PasswordFormMap =
      std::map<std::string,
               std::vector<std::unique_ptr<autofill::PasswordForm>>>;

  // Simple two state enum to indicate whether we should operate on saved
  // passwords or saved exceptions.
  enum class EntryKind { kPassword, kException };

  // Attempts to remove the entry corresponding to |index| from the map
  // corresponding to |entry_kind|. This will also add a corresponding undo
  // operation to |undo_manager_|. Returns whether removing the entry succeeded.
  bool TryRemovePasswordEntry(EntryKind entry_kind, size_t index);

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  // Sets the password and exception list of the UI view.
  void SetPasswordList();
  void SetPasswordExceptionList();

  // Returns the password store associated with the currently active profile.
  password_manager::PasswordStore* GetPasswordStore();

  PasswordFormMap password_map_;
  PasswordFormMap exception_map_;

  UndoManager undo_manager_;

  // Whether to show stored passwords or not.
  BooleanPrefMember show_passwords_;

  // UI view that owns this presenter.
  PasswordUIView* password_view_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerPresenter);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_MANAGER_PRESENTER_H_
