// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/test/live_sync/live_passwords_sync_test.h"
#include "chrome/test/live_sync/performance/sync_timing_helper.h"

// TODO(braffert): Move kNumBenchmarkPoints and kBenchmarkPoints for all
// datatypes into a performance test base class, once it is possible to do so.
static const int kNumPasswords = 150;
static const int kNumBenchmarkPoints = 18;
static const int kBenchmarkPoints[] = {1, 10, 20, 30, 40, 50, 75, 100, 125,
                                       150, 175, 200, 225, 250, 300, 350, 400,
                                       500};

class PasswordsSyncPerfTest : public TwoClientLivePasswordsSyncTest {
 public:
  PasswordsSyncPerfTest() : password_number_(0) {}

  // Adds |num_logins| new unique passwords to |profile|.
  void AddLogins(int profile, int num_logins);

  // Updates the password for all logins for |profile|.
  void UpdateLogins(int profile);

  // Removes all logins for |profile|.
  void RemoveLogins(int profile);

  // Removes all logins for all profiles.  Called between benchmark iterations.
  void Cleanup();

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
  LivePasswordsSyncTest::RemoveLogins(GetPasswordStore(profile));
}

void PasswordsSyncPerfTest::Cleanup() {
  for (int i = 0; i < num_clients(); ++i) {
    RemoveLogins(i);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_EQ(0, GetPasswordCount(0));
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());
}

webkit_glue::PasswordForm PasswordsSyncPerfTest::NextLogin() {
  return CreateTestPasswordForm(password_number_++);
}

std::string PasswordsSyncPerfTest::NextPassword() {
  return base::StringPrintf("password%d", password_number_++);
}

IN_PROC_BROWSER_TEST_F(PasswordsSyncPerfTest, P0) {
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

IN_PROC_BROWSER_TEST_F(PasswordsSyncPerfTest, DISABLED_Benchmark) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  for (int i = 0; i < kNumBenchmarkPoints; ++i) {
    int num_passwords = kBenchmarkPoints[i];
    AddLogins(0, num_passwords);
    base::TimeDelta dt_add =
        SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(num_passwords, GetPasswordCount(0));
    ASSERT_TRUE(AllProfilesContainSamePasswordForms());
    VLOG(0) << std::endl << "Add: " << num_passwords << " "
            << dt_add.InSecondsF();

    UpdateLogins(0);
    base::TimeDelta dt_update =
        SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(num_passwords, GetPasswordCount(0));
    ASSERT_TRUE(AllProfilesContainSamePasswordForms());
    VLOG(0) << std::endl << "Update: " << num_passwords << " "
            << dt_update.InSecondsF();

    RemoveLogins(0);
    base::TimeDelta dt_delete =
        SyncTimingHelper::TimeMutualSyncCycle(GetClient(0), GetClient(1));
    ASSERT_EQ(0, GetPasswordCount(0));
    ASSERT_TRUE(AllProfilesContainSamePasswordForms());
    VLOG(0) << std::endl << "Delete: " << num_passwords << " "
            << dt_delete.InSecondsF();

    Cleanup();
  }
}
