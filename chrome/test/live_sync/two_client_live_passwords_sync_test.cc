// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/test/live_sync/live_passwords_sync_test.h"

using webkit_glue::PasswordForm;

IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest, Add) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form;
  form.origin = GURL("http://www.google.com/");
  form.username_value = ASCIIToUTF16("username");
  form.password_value = ASCIIToUTF16("password");

  AddLogin(GetVerififerPasswordStore(), form);
  AddLogin(GetPasswordStore(0), form);

  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  std::vector<PasswordForm> expected;
  GetLogins(GetVerififerPasswordStore(), form, expected);
  EXPECT_EQ(1U, expected.size());

  std::vector<PasswordForm> actual_zero;
  GetLogins(GetPasswordStore(0), form, actual_zero);
  EXPECT_TRUE(ContainsSamePasswordForms(expected, actual_zero));

  std::vector<PasswordForm> actual_one;
  GetLogins(GetPasswordStore(1), form, actual_one);
  EXPECT_TRUE(ContainsSamePasswordForms(expected, actual_one));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePasswordsSyncTest, Race) {
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

  EXPECT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));

  std::vector<PasswordForm> actual_zero;
  GetLogins(GetPasswordStore(0), form, actual_zero);
  EXPECT_EQ(1U, actual_zero.size());

  std::vector<PasswordForm> actual_one;
  GetLogins(GetPasswordStore(1), form, actual_one);
  EXPECT_EQ(1U, actual_one.size());

  EXPECT_TRUE(ContainsSamePasswordForms(actual_zero, actual_one));
}
