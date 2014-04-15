// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/password_manager/core/browser/password_form_data.h"

using passwords_helper::AddLogin;
using passwords_helper::AwaitAllProfilesContainSamePasswordForms;
using passwords_helper::CreateTestPasswordForm;
using passwords_helper::GetPasswordCount;
using passwords_helper::GetPasswordStore;

using autofill::PasswordForm;

class MultipleClientPasswordsSyncTest : public SyncTest {
 public:
  MultipleClientPasswordsSyncTest() : SyncTest(MULTIPLE_CLIENT) {}
  virtual ~MultipleClientPasswordsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleClientPasswordsSyncTest);
};

// This test was flaky on the Mac buildbot and also appears to be flaky
// in local Linux (debug) builds. See crbug.com/363247.
IN_PROC_BROWSER_TEST_F(MultipleClientPasswordsSyncTest, DISABLED_Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  for (int i = 0; i < num_clients(); ++i) {
    PasswordForm form = CreateTestPasswordForm(i);
    AddLogin(GetPasswordStore(i), form);
  }

  ASSERT_TRUE(AwaitAllProfilesContainSamePasswordForms());
  ASSERT_EQ(num_clients(), GetPasswordCount(0));
}
