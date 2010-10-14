// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/test/live_sync/live_passwords_sync_test.h"

using webkit_glue::PasswordForm;

IN_PROC_BROWSER_TEST_F(MultipleClientLivePasswordsSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  for (int i = 0; i < num_clients(); ++i) {
    PasswordForm form;
    form.origin = GURL(StringPrintf("http://www.google.com/%d", i));
    form.username_value = ASCIIToUTF16(StringPrintf("username%d", i));
    form.password_value = ASCIIToUTF16(StringPrintf("password%d", i));

    AddLogin(GetPasswordStore(i), form);
  }

  EXPECT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));

  PasswordForm form;  // Don't set any fields, so that all logins match.
  std::vector<PasswordForm> expected;
  GetLogins(GetPasswordStore(0), form, expected);
  EXPECT_EQ((size_t) num_clients(), expected.size());

  for (int i = 1; i < num_clients(); ++i) {
    std::vector<PasswordForm> actual;
    GetLogins(GetPasswordStore(i), form, actual);

    EXPECT_TRUE(ContainsSamePasswordForms(expected, actual));
  }
}
