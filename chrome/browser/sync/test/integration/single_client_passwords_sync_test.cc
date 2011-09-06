// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"

using passwords_helper::AddLogin;
using passwords_helper::CreateTestPasswordForm;
using passwords_helper::GetPasswordCount;
using passwords_helper::GetPasswordStore;
using passwords_helper::GetVerifierPasswordCount;
using passwords_helper::GetVerifierPasswordStore;
using passwords_helper::ProfileContainsSamePasswordFormsAsVerifier;

using webkit_glue::PasswordForm;

class SingleClientPasswordsSyncTest : public SyncTest {
 public:
  SingleClientPasswordsSyncTest() : SyncTest(SINGLE_CLIENT) {}
  virtual ~SingleClientPasswordsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientPasswordsSyncTest);
};

// TODO(sync): Enable after MockKeychain is fixed. http://crbug.com/89808.
#if defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsSyncTest, DISABLED_Sanity) {
#else
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsSyncTest, Sanity) {
#endif
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
