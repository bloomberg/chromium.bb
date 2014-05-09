// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/password_manager/core/browser/password_form_data.h"

using passwords_helper::AddLogin;
using passwords_helper::AllProfilesContainSamePasswordFormsAsVerifier;
using passwords_helper::AwaitAllProfilesContainSamePasswordForms;
using passwords_helper::AwaitProfileContainsSamePasswordFormsAsVerifier;
using passwords_helper::CreateTestPasswordForm;
using passwords_helper::GetPasswordCount;
using passwords_helper::GetPasswordStore;
using passwords_helper::GetVerifierPasswordCount;
using passwords_helper::GetVerifierPasswordStore;
using passwords_helper::ProfileContainsSamePasswordFormsAsVerifier;
using passwords_helper::SetDecryptionPassphrase;
using passwords_helper::SetEncryptionPassphrase;
using sync_integration_test_util::AwaitPassphraseAccepted;
using sync_integration_test_util::AwaitPassphraseRequired;

using autofill::PasswordForm;

static const char* kValidPassphrase = "passphrase!";
static const char* kAnotherValidPassphrase = "Mot de passe!";

class MultipleClientPasswordsSyncTest : public SyncTest {
 public:
  MultipleClientPasswordsSyncTest() : SyncTest(MULTIPLE_CLIENT) {}
  virtual ~MultipleClientPasswordsSyncTest() {}

  virtual bool TestUsesSelfNotifications() OVERRIDE {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleClientPasswordsSyncTest);
};

IN_PROC_BROWSER_TEST_F(MultipleClientPasswordsSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  for (int i = 0; i < num_clients(); ++i) {
    PasswordForm form = CreateTestPasswordForm(i);
    AddLogin(GetPasswordStore(i), form);
  }

  ASSERT_TRUE(AwaitAllProfilesContainSamePasswordForms());
  ASSERT_EQ(num_clients(), GetPasswordCount(0));
}

IN_PROC_BROWSER_TEST_F(MultipleClientPasswordsSyncTest,
                       SetPassphraseAndThenSetupSync) {
  ASSERT_TRUE(SetupClients());

  ASSERT_TRUE(GetClient(0)->SetupSync());
  SetEncryptionPassphrase(0, kValidPassphrase, ProfileSyncService::EXPLICIT);
  ASSERT_TRUE(AwaitPassphraseAccepted(GetSyncService(0)));

  // When client 1 hits a passphrase required state, we can infer that
  // client 0's passphrase has been committed. to the server.
  GetClient(1)->SetupSync();
  ASSERT_TRUE(AwaitPassphraseRequired(GetSyncService(1)));

  // Setup client 2 *after* the passphrase has been committed.
  ASSERT_FALSE(GetClient(2)->SetupSync());
  ASSERT_TRUE(AwaitPassphraseRequired(GetSyncService(2)));

  // Get clients 1 and 2 out of the passphrase required state.
  ASSERT_TRUE(SetDecryptionPassphrase(1, kValidPassphrase));
  ASSERT_TRUE(AwaitPassphraseAccepted(GetSyncService(1)));
  ASSERT_TRUE(SetDecryptionPassphrase(2, kValidPassphrase));
  ASSERT_TRUE(AwaitPassphraseAccepted(GetSyncService(2)));

  // For some reason, the tests won't pass unless these flags are set.
  GetSyncService(1)->SetSyncSetupCompleted();
  GetSyncService(1)->SetSetupInProgress(false);
  GetSyncService(2)->SetSyncSetupCompleted();
  GetSyncService(2)->SetSetupInProgress(false);

  // Move around some passwords to make sure it's all working.
  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form0);

  ASSERT_TRUE(AwaitAllProfilesContainSamePasswordForms());
}

IN_PROC_BROWSER_TEST_F(MultipleClientPasswordsSyncTest,
                       SetDifferentPassphraseAndThenSetupSync) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  ASSERT_TRUE(GetClient(0)->SetupSync());
  SetEncryptionPassphrase(0, kValidPassphrase, ProfileSyncService::EXPLICIT);
  ASSERT_TRUE(AwaitPassphraseAccepted(GetSyncService((0))));

  // When client 1 hits a passphrase required state, we can infer that
  // client 0's passphrase has been committed. to the server.
  GetClient(1)->SetupSync();
  ASSERT_TRUE(AwaitPassphraseRequired(GetSyncService(1)));

  // Give client 1 the correct passphrase.
  SetDecryptionPassphrase(1, kValidPassphrase);
  ASSERT_TRUE(AwaitPassphraseAccepted(GetSyncService((1))));

  // For some reason, the tests won't pass unless these flags are set.
  GetSyncService(1)->SetSetupInProgress(false);
  GetSyncService(1)->SetSyncSetupCompleted();

  // Give client 2 a different passphrase so it fails to sync.
  ASSERT_FALSE(GetClient(2)->SetupSync());
  ASSERT_TRUE(AwaitPassphraseRequired(GetSyncService((2))));
  SetDecryptionPassphrase(2, kAnotherValidPassphrase);
  ASSERT_TRUE(AwaitPassphraseRequired(GetSyncService((2))));

  // Add a password on 0 while client 2 has different passphrases.
  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form0);
  AddLogin(GetPasswordStore(0), form0);

  // It should sync to client 1.
  ASSERT_TRUE(AwaitProfileContainsSamePasswordFormsAsVerifier(1));

  // But it won't get synced to 2.
  ASSERT_FALSE(ProfileContainsSamePasswordFormsAsVerifier(2));

  // Update 2 with the correct passphrase, the password should now sync over.
  ASSERT_TRUE(AwaitPassphraseRequired(GetSyncService(2)));
  ASSERT_TRUE(SetDecryptionPassphrase(2, kValidPassphrase));
  ASSERT_TRUE(AwaitPassphraseAccepted(GetSyncService(2)));

  // For some reason, the tests won't pass unless these flags are set.
  GetSyncService(2)->SetSetupInProgress(false);
  GetSyncService(2)->SetSyncSetupCompleted();

  ASSERT_TRUE(AwaitProfileContainsSamePasswordFormsAsVerifier(2));
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
}
