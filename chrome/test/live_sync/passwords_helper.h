// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_PASSWORDS_HELPER_H_
#define CHROME_TEST_LIVE_SYNC_PASSWORDS_HELPER_H_
#pragma once

#include <vector>

#include "base/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/live_sync/live_sync_test.h"
#include "webkit/glue/password_form.h"

class PasswordStore;

namespace passwords_helper {

// Adds the login held in |form| to the password store |store|. Even though
// logins are normally added asynchronously, this method will block until the
// login is added.
void AddLogin(PasswordStore* store, const webkit_glue::PasswordForm& form);

// Update the data held in password store |store| with a modified |form|.
// This method blocks until the operation is complete.
void UpdateLogin(PasswordStore* store, const webkit_glue::PasswordForm& form);

// Searches |store| for all logins matching a fake signon realm used only by
// LivePasswordsSyncTest and adds the results to |matches|. Note that the
// caller is responsible for deleting the forms added to |matches|.
void GetLogins(PasswordStore* store,
               std::vector<webkit_glue::PasswordForm>& matches);

// Removes the login held in |form| from the password store |store|.  This
// method blocks until the operation is complete.
void RemoveLogin(PasswordStore* store, const webkit_glue::PasswordForm& form);

// Removes all password forms from the password store |store|.
void RemoveLogins(PasswordStore* store);

// Sets the cryptographer's passphrase for the profile at index |index| to
// |passphrase|. |is_creation| is true if a new passphrase is being set up
// and false otherwise.
void SetPassphrase(int index, const std::string& passphrase, bool is_creation);

// Gets the password store of the profile with index |index|.
PasswordStore* GetPasswordStore(int index);

// Gets the password store of the verifier profile.
PasswordStore* GetVerifierPasswordStore();

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

// Returns the number of forms in the password store of the profile with index
// |index|.
int GetPasswordCount(int index);

// Returns the number of forms in the password store of the verifier profile.
int GetVerifierPasswordCount();

// Creates a test password form with a well known fake signon realm used only
// by LivePasswordsSyncTest based on |index|.
webkit_glue::PasswordForm CreateTestPasswordForm(int index);

}  // namespace passwords_helper

#endif  // CHROME_TEST_LIVE_SYNC_PASSWORDS_HELPER_H_
