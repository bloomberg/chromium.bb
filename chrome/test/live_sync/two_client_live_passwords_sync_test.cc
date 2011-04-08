// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/test/live_sync/live_passwords_sync_test.h"

using webkit_glue::PasswordForm;

static const char* kValidPassphrase = "passphrase!";

// TODO(rsimha): See http://crbug.com/78840.
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest, FLAKY_Add) {

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  AddLogin(GetPasswordStore(0), form);

  std::vector<PasswordForm> verifier_forms;
  GetLogins(GetVerifierPasswordStore(), verifier_forms);
  ASSERT_EQ(1U, verifier_forms.size());

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  std::vector<PasswordForm> forms0;
  GetLogins(GetPasswordStore(0), forms0);
  ASSERT_TRUE(ContainsSamePasswordForms(verifier_forms, forms0));

  std::vector<PasswordForm> forms1;
  GetLogins(GetPasswordStore(1), forms1);
  ASSERT_TRUE(ContainsSamePasswordForms(verifier_forms, forms1));
}

// TODO(rsimha): See http://crbug.com/78840.
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest, FLAKY_Race) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form0);

  PasswordForm form1 = form0;
  form1.password_value = ASCIIToUTF16("password1");
  AddLogin(GetPasswordStore(1), form1);

  ASSERT_TRUE(AwaitQuiescence());

  std::vector<PasswordForm> forms0;
  GetLogins(GetPasswordStore(0), forms0);
  ASSERT_EQ(1U, forms0.size());

  std::vector<PasswordForm> forms1;
  GetLogins(GetPasswordStore(1), forms1);
  ASSERT_EQ(1U, forms1.size());

  ASSERT_TRUE(ContainsSamePasswordForms(forms0, forms1));
}

// TODO(rsimha): See http://crbug.com/78840.
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest, FLAKY_SetPassphrase) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  SetPassphrase(0, kValidPassphrase, true);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  SetPassphrase(1, kValidPassphrase, false);
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());
}

// TODO(rsimha): See http://crbug.com/78840.
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest,
                       FLAKY_SetPassphraseAndAddPassword) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  SetPassphrase(0, kValidPassphrase, true);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  SetPassphrase(1, kValidPassphrase, false);
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form);

  std::vector<PasswordForm> forms0;
  GetLogins(GetPasswordStore(0), forms0);
  ASSERT_EQ(1U, forms0.size());

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  std::vector<PasswordForm> forms1;
  GetLogins(GetPasswordStore(1), forms1);
  ASSERT_EQ(1U, forms1.size());
}

// TODO(rsimha): See http://crbug.com/78840.
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest,
                       FLAKY_SetPassphraseAndThenSetupSync) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  ASSERT_TRUE(GetClient(0)->SetupSync());
  SetPassphrase(0, kValidPassphrase, true);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion("Initial sync."));

  SetPassphrase(1, kValidPassphrase, false);
  ASSERT_TRUE(GetClient(1)->SetupSync());
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());
}

// TODO(rsimha): See http://crbug.com/78840.
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest,
                       FLAKY_SetPassphraseTwice) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  SetPassphrase(0, kValidPassphrase, true);
  ASSERT_TRUE(GetClient(0)->AwaitPassphraseAccepted());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  SetPassphrase(1, kValidPassphrase, false);
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());

  SetPassphrase(1, kValidPassphrase, false);
  ASSERT_TRUE(GetClient(1)->AwaitPassphraseAccepted());
}
