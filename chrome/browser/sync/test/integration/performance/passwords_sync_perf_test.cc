// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/performance/sync_timing_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

using passwords_helper::AddLogin;
using passwords_helper::CreateTestPasswordForm;
using passwords_helper::GetLogins;
using passwords_helper::GetPasswordCount;
using passwords_helper::GetPasswordStore;
using passwords_helper::UpdateLogin;

static const int kNumPasswords = 150;

class PasswordsSyncPerfTest : public SyncTest {
 public:
  PasswordsSyncPerfTest() : SyncTest(TWO_CLIENT), password_number_(0) {}

  // Adds |num_logins| new unique passwords to |profile|.
  void AddLogins(int profile, int num_logins);

  // Updates the password for all logins for |profile|.
  void UpdateLogins(int profile);

  // Removes all logins for |profile|.
  void RemoveLogins(int profile);

 private:
  // Returns a new unique login.
  webkit_glue::PasswordForm NextLogin();

  // Returns a new unique password value.
  std::string NextPassword();

  int password_number_;
  DISALLOW_COPY_AND_ASSIGN(PasswordsSyncPerfTest);
};

void PasswordsSyncPerfTest::AddLogins(int profile, int num_logins) {
  for (int i = 0; i < num_logins; ++i) {
    AddLogin(GetPasswordStore(profile), NextLogin());
  }
}

void PasswordsSyncPerfTest::UpdateLogins(int profile) {
  std::vector<webkit_glue::PasswordForm> logins;
  GetLogins(GetPasswordStore(profile), logins);
  for (std::vector<webkit_glue::PasswordForm>::iterator it = logins.begin();
       it != logins.end(); ++it) {
    (*it).password_value = ASCIIToUTF16(NextPassword());
    UpdateLogin(GetPasswordStore(profile), (*it));
  }
}

void PasswordsSyncPerfTest::RemoveLogins(int profile) {
  passwords_helper::RemoveLogins(GetPasswordStore(profile));
}

webkit_glue::PasswordForm PasswordsSyncPerfTest::NextLogin() {
  return CreateTestPasswordForm(password_number_++);
}

std::string PasswordsSyncPerfTest::NextPassword() {
  return base::StringPrintf("password%d", password_number_++);
}

// Flaky on Windows, timing out on Mac, see http://crbug.com/105999
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_P0 DISABLED_P0
#else
#define MAYBE_P0 P0
#endif

IN_PROC_BROWSER_TEST_F(PasswordsSyncPerfTest, MAYBE_P0) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // TCM ID - 7367749.
  AddLogins(0, kNumPasswords);
  base::TimeDelta dt =
      SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumPasswords, GetPasswordCount(1));
  SyncTimingHelper::PrintResult("passwords", "add_passwords", dt);

  // TCM ID - 7365093.
  UpdateLogins(0);
  dt = SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(kNumPasswords, GetPasswordCount(1));
  SyncTimingHelper::PrintResult("passwords", "update_passwords", dt);

  // TCM ID - 7557852
  RemoveLogins(0);
  dt = SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
  ASSERT_EQ(0, GetPasswordCount(1));
  SyncTimingHelper::PrintResult("passwords", "delete_passwords", dt);
}
