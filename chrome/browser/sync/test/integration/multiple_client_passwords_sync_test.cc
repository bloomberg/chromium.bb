// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_form_data.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"

using passwords_helper::AddLogin;
using passwords_helper::AllProfilesContainSamePasswordForms;
using passwords_helper::CreateTestPasswordForm;
using passwords_helper::GetPasswordCount;
using passwords_helper::GetPasswordStore;

using webkit_glue::PasswordForm;

class MultipleClientPasswordsSyncTest : public SyncTest {
 public:
  MultipleClientPasswordsSyncTest() : SyncTest(MULTIPLE_CLIENT) {}
  virtual ~MultipleClientPasswordsSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleClientPasswordsSyncTest);
};

// TODO(rsimha): Enable after crbug.com/77993 is fixed.
IN_PROC_BROWSER_TEST_F(MultipleClientPasswordsSyncTest, DISABLED_Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  for (int i = 0; i < num_clients(); ++i) {
    PasswordForm form = CreateTestPasswordForm(i);
    AddLogin(GetPasswordStore(i), form);
  }
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(num_clients(), GetPasswordCount(0));
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());
}
