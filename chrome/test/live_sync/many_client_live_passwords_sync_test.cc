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

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  AddLogin(GetPasswordStore(0), form);
  ASSERT_TRUE(GetClient(0)->AwaitGroupSyncCycleCompletion(clients()));

  std::vector<PasswordForm> verifier_forms;
  GetLogins(GetVerifierPasswordStore(), verifier_forms);
  ASSERT_EQ(1U, verifier_forms.size());

  for (int i = 0; i < num_clients(); ++i) {
    std::vector<PasswordForm> forms;
    GetLogins(GetPasswordStore(i), forms);
    ASSERT_TRUE(ContainsSamePasswordForms(verifier_forms, forms));
  }
}
