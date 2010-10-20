// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/test/live_sync/live_passwords_sync_test.h"

using webkit_glue::PasswordForm;

IN_PROC_BROWSER_TEST_F(ManyClientLivePasswordsSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form;
  form.origin = GURL("http://www.google.com/");
  form.username_value = ASCIIToUTF16("username");
  form.password_value = ASCIIToUTF16("password");

  AddLogin(GetVerifierPasswordStore(), form);
  AddLogin(GetPasswordStore(0), form);

  EXPECT_TRUE(GetClient(0)->AwaitGroupSyncCycleCompletion(clients()));

  std::vector<PasswordForm> expected;
  GetLogins(GetVerifierPasswordStore(), form, expected);
  EXPECT_EQ(1U, expected.size());

  for (int i = 0; i < num_clients(); ++i) {
    std::vector<PasswordForm> actual;
    GetLogins(GetPasswordStore(i), form, actual);

    EXPECT_TRUE(ContainsSamePasswordForms(expected, actual));
  }
}
