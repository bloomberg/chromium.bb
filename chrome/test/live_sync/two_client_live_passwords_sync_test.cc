// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/test/live_sync/live_passwords_sync_test.h"

using webkit_glue::PasswordForm;

static const char* kValidPassphrase = "passphrase!";

IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest, Add) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest, Race) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form0);

  PasswordForm form1 = form0;
  form1.password_value = ASCIIToUTF16("password1");
  AddLogin(GetPasswordStore(1), form1);

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());
}

// TCM ID - 4577932.
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest, DisablePasswords) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  ASSERT_TRUE(GetClient(1)->DisableSyncForDatatype(syncable::PASSWORDS));
  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(ProfileContainsSamePasswordFormsAsVerifier(0));
  ASSERT_FALSE(ProfileContainsSamePasswordFormsAsVerifier(1));

  ASSERT_TRUE(GetClient(1)->EnableSyncForDatatype(syncable::PASSWORDS));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
  ASSERT_EQ(1, GetPasswordCount(1));
}

// TCM ID - 4649281.
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest, DisableSync) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  ASSERT_TRUE(GetClient(1)->DisableSyncForAllDatatypes());
  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));

  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion("Added a password."));
  ASSERT_TRUE(ProfileContainsSamePasswordFormsAsVerifier(0));
  ASSERT_FALSE(ProfileContainsSamePasswordFormsAsVerifier(1));

  ASSERT_TRUE(GetClient(1)->EnableSyncForAllDatatypes());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
  ASSERT_EQ(1, GetPasswordCount(1));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest, SetPassphrase) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  SetPassphrase(0, kValidPassphrase, true);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  SetPassphrase(1, kValidPassphrase, false);
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(1)->AwaitSyncCycleCompletion("Set passphrase."));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest,
                       SetPassphraseAndAddPassword) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  SetPassphrase(0, kValidPassphrase, true);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  SetPassphrase(1, kValidPassphrase, false);
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_EQ(1, GetPasswordCount(1));
}

// TODO(rsimha): Flaky due to http://crbug.com/80180.
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest,
                       FLAKY_SetPassphraseAndThenSetupSync) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  ASSERT_TRUE(GetClient(0)->SetupSync());
  SetPassphrase(0, kValidPassphrase, true);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion("Initial sync."));

  ASSERT_FALSE(GetClient(1)->SetupSync());
  SetPassphrase(1, kValidPassphrase, false);
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(1)->AwaitSyncCycleCompletion("Initial sync."));
}

// TODO(rsimha): Flaky due to http://crbug.com/80180.
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest,
                       FLAKY_SetPassphraseTwice) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  SetPassphrase(0, kValidPassphrase, true);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  SetPassphrase(1, kValidPassphrase, false);
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(1)->AwaitSyncCycleCompletion("Set passphrase."));

  SetPassphrase(1, kValidPassphrase, false);
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(1)->AwaitSyncCycleCompletion("Set passphrase again."));
}
