// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_PASSWORDS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_PASSWORDS_HELPER_H_

#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/autofill/core/common/password_form.h"

namespace password_manager {
class PasswordStore;
}

namespace passwords_helper {

// Adds the login held in |form| to the password store |store|. Even though
// logins are normally added asynchronously, this method will block until the
// login is added.
void AddLogin(password_manager::PasswordStore* store,
              const autofill::PasswordForm& form);

// Update the data held in password store |store| with a modified |form|.
// This method blocks until the operation is complete.
void UpdateLogin(password_manager::PasswordStore* store,
                 const autofill::PasswordForm& form);

// Searches |store| for all logins matching a fake signon realm used only by
// LivePasswordsSyncTest and adds the results to |matches|. Note that the
// caller is responsible for deleting the forms added to |matches|.
void GetLogins(password_manager::PasswordStore* store,
               std::vector<autofill::PasswordForm>& matches);

// Removes the login held in |form| from the password store |store|.  This
// method blocks until the operation is complete.
void RemoveLogin(password_manager::PasswordStore* store,
                 const autofill::PasswordForm& form);

// Removes all password forms from the password store |store|.
void RemoveLogins(password_manager::PasswordStore* store);

// Sets the cryptographer's encryption passphrase for the profile at index
// |index| to |passphrase|, and passphrase type |type|.
void SetEncryptionPassphrase(int index,
                             const std::string& passphrase,
                             ProfileSyncService::PassphraseType type);

// Sets the cryptographer's decryption passphrase for the profile at index
// |index| to |passphrase|. Returns false if the operation failed, and true
// otherwise.
bool SetDecryptionPassphrase(int index, const std::string& passphrase);

// Gets the password store of the profile with index |index|.
password_manager::PasswordStore* GetPasswordStore(int index);

// Gets the password store of the verifier profile.
password_manager::PasswordStore* GetVerifierPasswordStore();

// Returns true iff the profile with index |index| contains the same password
// forms as the verifier profile.
bool ProfileContainsSamePasswordFormsAsVerifier(int index);

// Returns true iff the profile with index |index_a| contains the same
// password forms as the profile with index |index_b|.
bool ProfilesContainSamePasswordForms(int index_a, int index_b);

// Returns true iff all profiles contain the same password forms as the
// verifier profile.
bool AllProfilesContainSamePasswordFormsAsVerifier();

// Returns true iff all profiles contain the same password forms.
bool AllProfilesContainSamePasswordForms();

// Returns true if all profiles contain the same password forms and
// it doesn't time out.
bool AwaitAllProfilesContainSamePasswordForms();

// Returns true if specified profile contains the same password forms as the
// verifier and it doesn't time out.
bool AwaitProfileContainsSamePasswordFormsAsVerifier(int index);

// Returns the number of forms in the password store of the profile with index
// |index|.
int GetPasswordCount(int index);

// Returns the number of forms in the password store of the verifier profile.
int GetVerifierPasswordCount();

// Creates a test password form with a well known fake signon realm used only
// by LivePasswordsSyncTest based on |index|.
autofill::PasswordForm CreateTestPasswordForm(int index);

}  // namespace passwords_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_PASSWORDS_HELPER_H_
