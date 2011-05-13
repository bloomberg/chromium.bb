// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/test/live_sync/live_passwords_sync_test.h"

using webkit_glue::PasswordForm;

// TODO(rsimha): This test fails intermittently -- see http://crbug.com/77993.
IN_PROC_BROWSER_TEST_F(MultipleClientLivePasswordsSyncTest, FAILS_Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  for (int i = 0; i < num_clients(); ++i) {
    PasswordForm form = CreateTestPasswordForm(i);
    AddLogin(GetPasswordStore(i), form);
  }
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(num_clients(), GetPasswordCount(0));
  for (int i = 1; i < num_clients(); ++i) {
    ASSERT_TRUE(ProfilesContainSamePasswordForms(0, i));
  }
}
