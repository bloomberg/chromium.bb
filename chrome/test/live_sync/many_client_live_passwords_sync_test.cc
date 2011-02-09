// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/test/live_sync/live_passwords_sync_test.h"

using webkit_glue::PasswordForm;

// TODO(rsimha): Enable once http://crbug.com/69604 is fixed.
IN_PROC_BROWSER_TEST_F(ManyClientLivePasswordsSyncTest, DISABLED_Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form;
  form.origin = GURL("http://www.google.com/");
  form.username_value = ASCIIToUTF16("username");
  form.password_value = ASCIIToUTF16("password");

  AddLogin(GetVerifierPasswordStore(), form);
  AddLogin(GetPasswordStore(0), form);

  ASSERT_TRUE(GetClient(0)->AwaitGroupSyncCycleCompletion(clients()));

  std::vector<PasswordForm> expected;
  GetLogins(GetVerifierPasswordStore(), form, expected);
  ASSERT_EQ(1U, expected.size());

  for (int i = 0; i < num_clients(); ++i) {
    std::vector<PasswordForm> actual;
    GetLogins(GetPasswordStore(i), form, actual);

    ASSERT_TRUE(ContainsSamePasswordForms(expected, actual));
  }
}
