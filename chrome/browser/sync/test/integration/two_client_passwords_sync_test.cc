// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"

using passwords_helper::AddLogin;
using passwords_helper::AllProfilesContainSamePasswordForms;
using passwords_helper::AllProfilesContainSamePasswordFormsAsVerifier;
using passwords_helper::CreateTestPasswordForm;
using passwords_helper::GetPasswordCount;
using passwords_helper::GetPasswordStore;
using passwords_helper::GetVerifierPasswordCount;
using passwords_helper::GetVerifierPasswordStore;
using passwords_helper::ProfileContainsSamePasswordFormsAsVerifier;
using passwords_helper::RemoveLogin;
using passwords_helper::RemoveLogins;
using passwords_helper::SetDecryptionPassphrase;
using passwords_helper::SetEncryptionPassphrase;
using passwords_helper::UpdateLogin;

using content::PasswordForm;

static const char* kValidPassphrase = "passphrase!";
static const char* kAnotherValidPassphrase = "another passphrase!";

class TwoClientPasswordsSyncTest : public SyncTest {
 public:
  TwoClientPasswordsSyncTest() : SyncTest(TWO_CLIENT) {}
  virtual ~TwoClientPasswordsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientPasswordsSyncTest);
};

// TCM ID - 3732277
IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, Add) {
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

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, Race) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form0);

  PasswordForm form1 = form0;
  form1.password_value = ASCIIToUTF16("new_password");
  AddLogin(GetPasswordStore(1), form1);

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());
}

// TCM ID - 4577932.
IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, DisablePasswords) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  ASSERT_TRUE(GetClient(1)->DisableSyncForDatatype(syncer::PASSWORDS));
  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));

  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion("Added a password."));
  ASSERT_TRUE(ProfileContainsSamePasswordFormsAsVerifier(0));
  ASSERT_FALSE(ProfileContainsSamePasswordFormsAsVerifier(1));

  ASSERT_TRUE(GetClient(1)->EnableSyncForDatatype(syncer::PASSWORDS));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
  ASSERT_EQ(1, GetPasswordCount(1));
}

// TCM ID - 4649281.
IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, DisableSync) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  ASSERT_TRUE(GetClient(1)->DisableSyncForAllDatatypes());
  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));

  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion("Added a password."));
  ASSERT_TRUE(ProfileContainsSamePasswordFormsAsVerifier(0));
  ASSERT_FALSE(ProfileContainsSamePasswordFormsAsVerifier(1));

  ASSERT_TRUE(GetClient(1)->EnableSyncForAllDatatypes());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
  ASSERT_EQ(1, GetPasswordCount(1));
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, SetPassphrase) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  SetEncryptionPassphrase(0, kValidPassphrase, ProfileSyncService::EXPLICIT);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_TRUE(GetClient(1)->AwaitPassphraseRequired());
  ASSERT_TRUE(SetDecryptionPassphrase(1, kValidPassphrase));
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(1)->AwaitFullSyncCompletion("Set passphrase."));
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest,
                       SetPassphraseAndAddPassword) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  SetEncryptionPassphrase(0, kValidPassphrase, ProfileSyncService::EXPLICIT);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_TRUE(GetClient(1)->AwaitPassphraseRequired());
  ASSERT_TRUE(SetDecryptionPassphrase(1, kValidPassphrase));
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_EQ(1, GetPasswordCount(1));
}

// TCM ID - 4603879
IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, Update) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  AddLogin(GetPasswordStore(0), form);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  form.password_value = ASCIIToUTF16("new_password");
  UpdateLogin(GetVerifierPasswordStore(), form);
  UpdateLogin(GetPasswordStore(1), form);
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(1, GetVerifierPasswordCount());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
}

// TCM ID - 3719309
IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, Delete) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form0);
  AddLogin(GetPasswordStore(0), form0);
  PasswordForm form1 = CreateTestPasswordForm(1);
  AddLogin(GetVerifierPasswordStore(), form1);
  AddLogin(GetPasswordStore(0), form1);
  ASSERT_TRUE(AwaitQuiescence());

  RemoveLogin(GetPasswordStore(1), form0);
  RemoveLogin(GetVerifierPasswordStore(), form0);
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(1, GetVerifierPasswordCount());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
}

// TCM ID - 7573511
// Flaky on Mac and Windows: http://crbug.com/111399
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_DeleteAll DISABLED_DeleteAll
#else
#define MAYBE_DeleteAll DeleteAll
#endif
IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, MAYBE_DeleteAll) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form0);
  AddLogin(GetPasswordStore(0), form0);
  PasswordForm form1 = CreateTestPasswordForm(1);
  AddLogin(GetVerifierPasswordStore(), form1);
  AddLogin(GetPasswordStore(0), form1);
  ASSERT_TRUE(AwaitQuiescence());

  RemoveLogins(GetPasswordStore(1));
  RemoveLogins(GetVerifierPasswordStore());
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(0, GetVerifierPasswordCount());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
}

// TCM ID - 3694311
IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest, Merge) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form0);
  PasswordForm form1 = CreateTestPasswordForm(1);
  AddLogin(GetPasswordStore(1), form1);
  PasswordForm form2 = CreateTestPasswordForm(2);
  AddLogin(GetPasswordStore(1), form2);
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(3, GetPasswordCount(0));
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest,
                       SetPassphraseAndThenSetupSync) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  ASSERT_TRUE(GetClient(0)->SetupSync());
  SetEncryptionPassphrase(0, kValidPassphrase, ProfileSyncService::EXPLICIT);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion("Initial sync."));

  ASSERT_FALSE(GetClient(1)->SetupSync());
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseRequired());
  ASSERT_TRUE(SetDecryptionPassphrase(1, kValidPassphrase));
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(1)->AwaitFullSyncCompletion("Initial sync."));

  // Following ensures types are enabled and active (see bug 87572).
  syncer::ModelSafeRoutingInfo routes;
  GetClient(0)->service()->GetModelSafeRoutingInfo(&routes);
  ASSERT_EQ(syncer::GROUP_PASSWORD, routes[syncer::PASSWORDS]);
  routes.clear();
  GetClient(1)->service()->GetModelSafeRoutingInfo(&routes);
  ASSERT_EQ(syncer::GROUP_PASSWORD, routes[syncer::PASSWORDS]);
}

IN_PROC_BROWSER_TEST_F(TwoClientPasswordsSyncTest,
                       SetDifferentPassphraseAndThenSetupSync) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  ASSERT_TRUE(GetClient(0)->SetupSync());
  SetEncryptionPassphrase(0, kValidPassphrase, ProfileSyncService::EXPLICIT);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion("Initial sync."));

  // Setup 1 with a different passphrase, so that it fails to sync.
  ASSERT_FALSE(GetClient(1)->SetupSync());
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseRequired());
  ASSERT_FALSE(SetDecryptionPassphrase(1, kAnotherValidPassphrase));
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseRequired());

  // Add a password on 0 while clients have different passphrases.
  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form0);
  AddLogin(GetPasswordStore(0), form0);

  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion("Added a password."));

  // Password hasn't been synced to 1 yet.
  ASSERT_FALSE(AllProfilesContainSamePasswordFormsAsVerifier());

  // Update 1 with the correct passphrase, the password should now sync over.
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseRequired());
  ASSERT_TRUE(SetDecryptionPassphrase(1, kValidPassphrase));
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
}
