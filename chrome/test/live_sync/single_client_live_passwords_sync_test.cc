// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/test/live_sync/live_passwords_sync_test.h"

using webkit_glue::PasswordForm;

// TODO(sync): Remove FAILS_ annotation after http://crbug.com/59867 is fixed.
#if defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(SingleClientLivePasswordsSyncTest, FAILS_Sanity) {
#else
IN_PROC_BROWSER_TEST_F(SingleClientLivePasswordsSyncTest, Sanity) {
#endif
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form;
  form.origin = GURL("http://www.google.com/");
  form.username_value = ASCIIToUTF16("username");
  form.password_value = ASCIIToUTF16("password");

  AddLogin(GetVerifierPasswordStore(), form);
  AddLogin(GetPasswordStore(0), form);

  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion(
      "Waiting for passwords change."));

  std::vector<PasswordForm> expected;
  GetLogins(GetVerifierPasswordStore(), form, expected);
  ASSERT_EQ(1U, expected.size());

  std::vector<PasswordForm> actual;
  GetLogins(GetPasswordStore(0), form, actual);
  ASSERT_TRUE(ContainsSamePasswordForms(expected, actual));
}
