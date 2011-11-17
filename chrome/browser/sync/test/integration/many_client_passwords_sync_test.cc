// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"

using passwords_helper::AddLogin;
using passwords_helper::AllProfilesContainSamePasswordFormsAsVerifier;
using passwords_helper::CreateTestPasswordForm;
using passwords_helper::GetPasswordCount;
using passwords_helper::GetPasswordStore;
using passwords_helper::GetVerifierPasswordCount;
using passwords_helper::GetVerifierPasswordStore;

using webkit_glue::PasswordForm;

class ManyClientPasswordsSyncTest : public SyncTest {
 public:
  ManyClientPasswordsSyncTest() : SyncTest(MANY_CLIENT) {}
  virtual ~ManyClientPasswordsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ManyClientPasswordsSyncTest);
};

IN_PROC_BROWSER_TEST_F(ManyClientPasswordsSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));
  ASSERT_TRUE(GetClient(0)->AwaitGroupSyncCycleCompletion(clients()));

  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
  ASSERT_EQ(1, GetPasswordCount(0));
}
