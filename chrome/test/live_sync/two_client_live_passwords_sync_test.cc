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

// TODO(sync): Remove FAILS_ annotation after http://crbug.com/59867 is fixed.
#if defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest, FAILS_Add) {
#else
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest, Add) {
#endif
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form;
  form.origin = GURL("http://www.google.com/");
  form.username_value = ASCIIToUTF16("username");
  form.password_value = ASCIIToUTF16("password");

  AddLogin(GetVerifierPasswordStore(), form);
  AddLogin(GetPasswordStore(0), form);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  std::vector<PasswordForm> expected;
  GetLogins(GetVerifierPasswordStore(), form, expected);
  ASSERT_EQ(1U, expected.size());

  std::vector<PasswordForm> actual_zero;
  GetLogins(GetPasswordStore(0), form, actual_zero);
  ASSERT_TRUE(ContainsSamePasswordForms(expected, actual_zero));

  std::vector<PasswordForm> actual_one;
  GetLogins(GetPasswordStore(1), form, actual_one);
  ASSERT_TRUE(ContainsSamePasswordForms(expected, actual_one));
}

// TODO(sync): Remove FAILS_ annotation after http://crbug.com/59867 is fixed.
#if defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest, FAILS_Race) {
#else
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest, Race) {
#endif
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form;
  form.origin = GURL("http://www.google.com/");

  PasswordForm form_zero;
  form_zero.origin = GURL("http://www.google.com/");
  form_zero.username_value = ASCIIToUTF16("username");
  form_zero.password_value = ASCIIToUTF16("zero");
  AddLogin(GetPasswordStore(0), form_zero);

  PasswordForm form_one;
  form_one.origin = GURL("http://www.google.com/");
  form_one.username_value = ASCIIToUTF16("username");
  form_one.password_value = ASCIIToUTF16("one");
  AddLogin(GetPasswordStore(1), form_one);

  ASSERT_TRUE(AwaitQuiescence());

  std::vector<PasswordForm> actual_zero;
  GetLogins(GetPasswordStore(0), form, actual_zero);
  ASSERT_EQ(1U, actual_zero.size());

  std::vector<PasswordForm> actual_one;
  GetLogins(GetPasswordStore(1), form, actual_one);
  ASSERT_EQ(1U, actual_one.size());

  ASSERT_TRUE(ContainsSamePasswordForms(actual_zero, actual_one));
}

// TODO(sync): Remove FAILS_ annotation after http://crbug.com/59867 is fixed.
#if defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest, FAILS_SetPassphrase) {
#else
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest, SetPassphrase) {
#endif
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  GetClient(0)->service()->SetPassphrase(kValidPassphrase, true, true);
  GetClient(0)->AwaitPassphraseAccepted();
  GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1));
  ASSERT_TRUE(GetClient(1)->service()->observed_passphrase_required());
  GetClient(1)->service()->SetPassphrase(kValidPassphrase, true, true);
  GetClient(1)->AwaitPassphraseAccepted();
  ASSERT_FALSE(GetClient(1)->service()->observed_passphrase_required());
}

// TODO(sync): Remove FAILS_ annotation after http://crbug.com/59867 is fixed.
#if defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest,
                       FAILS_SetPassphraseAndAddPassword) {
#else
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest,
                       SetPassphraseAndAddPassword) {
#endif
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  GetClient(0)->service()->SetPassphrase(kValidPassphrase, true, true);
  GetClient(0)->AwaitPassphraseAccepted();

  PasswordForm form;
  form.origin = GURL("http://www.google.com/");
  form.username_value = ASCIIToUTF16("username");
  form.password_value = ASCIIToUTF16("password");

  AddLogin(GetPasswordStore(0), form);

  GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1));
  ASSERT_TRUE(GetClient(1)->service()->observed_passphrase_required());
  ASSERT_EQ(1, GetClient(1)->GetLastSessionSnapshot()->
      num_conflicting_updates);

  GetClient(1)->service()->SetPassphrase(kValidPassphrase, true, true);
  GetClient(1)->AwaitPassphraseAccepted();
  ASSERT_FALSE(GetClient(1)->service()->observed_passphrase_required());
  GetClient(1)->AwaitSyncCycleCompletion("Accept passphrase and decrypt.");
  ASSERT_EQ(0, GetClient(1)->GetLastSessionSnapshot()->
      num_conflicting_updates);
}

// TODO(sync): Remove DISABLED_ annotation after http://crbug.com/59867 and
// http://crbug.com/67862 are fixed.
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest,
                       DISABLED_SetPassphraseAndThenSetupSync) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  ASSERT_TRUE(GetClient(0)->SetupSync());
  GetClient(0)->service()->SetPassphrase(kValidPassphrase, true, true);
  GetClient(0)->AwaitPassphraseAccepted();
  GetClient(0)->AwaitSyncCycleCompletion("Initial sync.");

  ASSERT_TRUE(GetClient(1)->SetupSync());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(GetClient(1)->service()->observed_passphrase_required());
  GetClient(1)->service()->SetPassphrase(kValidPassphrase, true, true);
  GetClient(1)->AwaitPassphraseAccepted();
  ASSERT_FALSE(GetClient(1)->service()->observed_passphrase_required());
}

// TODO(sync): Remove DISABLED_ annotation after http://crbug.com/59867 and
// http://crbug.com/67862 are fixed.
IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest,
                       DISABLED_SetPassphraseTwice) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  GetClient(0)->service()->SetPassphrase(kValidPassphrase, true, true);
  GetClient(0)->AwaitPassphraseAccepted();
  GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1));
  ASSERT_TRUE(GetClient(1)->service()->observed_passphrase_required());

  GetClient(1)->service()->SetPassphrase(kValidPassphrase, true, true);
  GetClient(1)->AwaitPassphraseAccepted();
  ASSERT_FALSE(GetClient(1)->service()->observed_passphrase_required());

  GetClient(1)->service()->SetPassphrase(kValidPassphrase, true, true);
  GetClient(1)->AwaitPassphraseAccepted();
  ASSERT_FALSE(GetClient(1)->service()->observed_passphrase_required());
}
