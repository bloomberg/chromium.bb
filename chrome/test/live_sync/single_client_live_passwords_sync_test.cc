// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/test/live_sync/live_passwords_sync_test.h"

using webkit_glue::PasswordForm;

IN_PROC_BROWSER_TEST_F(SingleClientLivePasswordsSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));

  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion("Added a login."));
  ASSERT_TRUE(ProfileContainsSamePasswordFormsAsVerifier(0));
  ASSERT_EQ(1, GetPasswordCount(0));
}
