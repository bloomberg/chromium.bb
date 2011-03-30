// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/test/live_sync/live_passwords_sync_test.h"

using webkit_glue::PasswordForm;

IN_PROC_BROWSER_TEST_F(MultipleClientLivePasswordsSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  for (int i = 0; i < num_clients(); ++i) {
    PasswordForm form = CreateTestPasswordForm(i);
    AddLogin(GetPasswordStore(i), form);
  }
  ASSERT_TRUE(AwaitQuiescence());

  std::vector<PasswordForm> forms0;
  GetLogins(GetPasswordStore(0), forms0);
  ASSERT_EQ((size_t) num_clients(), forms0.size());

  for (int i = 1; i < num_clients(); ++i) {
    std::vector<PasswordForm> forms;
    GetLogins(GetPasswordStore(i), forms);
    ASSERT_TRUE(ContainsSamePasswordForms(forms0, forms));
  }
}
