// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <string>

#include "base/basictypes.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/test/test_reg_util_win.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/common/guid.h"
#include "chrome/installer/gcapi/gcapi.h"
#include "chrome/installer/util/google_update_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
using base::win::RegKey;


class GCAPILastRunTest : public ::testing::Test {
 protected:
  void SetUp() {
    // Override keys - this is undone during destruction.
    std::wstring hklm_override = base::StringPrintf(
        L"hklm_override\\%ls", ASCIIToWide(guid::GenerateGUID()));
    override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE, hklm_override);
    std::wstring hkcu_override = base::StringPrintf(
        L"hkcu_override\\%ls", ASCIIToWide(guid::GenerateGUID()));
    override_manager_.OverrideRegistry(HKEY_CURRENT_USER, hkcu_override);

    // Create the client state keys in the right places.
    struct {
      HKEY hive;
      const wchar_t* path;
    } reg_data[] = {
      { HKEY_LOCAL_MACHINE, google_update::kRegPathClientStateMedium },
      { HKEY_CURRENT_USER, google_update::kRegPathClientState }
    };
    for (int i = 0; i < arraysize(reg_data); ++i) {
      std::wstring reg_path(reg_data[i].path);
      reg_path += L"\\";
      reg_path += google_update::kChromeUpgradeCode;
      RegKey client_state(reg_data[i].hive,
                          reg_path.c_str(),
                          KEY_CREATE_SUB_KEY);
      ASSERT_TRUE(client_state.Valid());

      // Place a bogus "pv" value in the right places to make the last run
      // checker believe Chrome is installed.
      std::wstring clients_path(google_update::kRegPathClients);
      clients_path += L"\\";
      clients_path += google_update::kChromeUpgradeCode;
      RegKey client_key(reg_data[i].hive,
                        clients_path.c_str(),
                        KEY_CREATE_SUB_KEY | KEY_SET_VALUE);
      ASSERT_TRUE(client_key.Valid());
      client_key.WriteValue(L"pv", L"1.2.3.4");
    }
  }

  bool SetLastRunTime(HKEY hive, int64 last_run_time) {
    return SetLastRunTimeString(hive, base::Int64ToString16(last_run_time));
  }

  bool SetLastRunTimeString(HKEY hive, const string16& last_run_time_string) {
    const wchar_t* base_path =
        (hive == HKEY_LOCAL_MACHINE) ?
            google_update::kRegPathClientStateMedium :
            google_update::kRegPathClientState;
    std::wstring path(base_path);
    path += L"\\";
    path += google_update::kChromeUpgradeCode;

    RegKey client_state(hive, path.c_str(), KEY_SET_VALUE);
    return (client_state.Valid() &&
            client_state.WriteValue(
                google_update::kRegLastRunTimeField,
                last_run_time_string.c_str()) == ERROR_SUCCESS);
  }

 private:
  registry_util::RegistryOverrideManager override_manager_;
};

TEST_F(GCAPILastRunTest, Basic_HKCU) {
  Time last_run = Time::NowFromSystemTime() - TimeDelta::FromDays(10);
  EXPECT_TRUE(SetLastRunTime(HKEY_CURRENT_USER, last_run.ToInternalValue()));

  int days_since_last_run = GoogleChromeDaysSinceLastRun();
  EXPECT_EQ(10, days_since_last_run);
}

TEST_F(GCAPILastRunTest, Basic_HKLM) {
  Time last_run = Time::NowFromSystemTime() - TimeDelta::FromDays(10);
  EXPECT_TRUE(SetLastRunTime(HKEY_LOCAL_MACHINE, last_run.ToInternalValue()));

  int days_since_last_run = GoogleChromeDaysSinceLastRun();
  EXPECT_EQ(10, days_since_last_run);
}

TEST_F(GCAPILastRunTest, HKLM_and_HKCU) {
  Time hklm_last_run = Time::NowFromSystemTime() - TimeDelta::FromDays(30);
  EXPECT_TRUE(SetLastRunTime(HKEY_LOCAL_MACHINE,
                             hklm_last_run.ToInternalValue()));

  Time hkcu_last_run = Time::NowFromSystemTime() - TimeDelta::FromDays(20);
  EXPECT_TRUE(SetLastRunTime(HKEY_CURRENT_USER,
                             hkcu_last_run.ToInternalValue()));

  int days_since_last_run = GoogleChromeDaysSinceLastRun();
  EXPECT_EQ(20, days_since_last_run);
}

TEST_F(GCAPILastRunTest, NoLastRun) {
  int days_since_last_run = GoogleChromeDaysSinceLastRun();
  EXPECT_EQ(-1, days_since_last_run);
}

TEST_F(GCAPILastRunTest, InvalidLastRun) {
  EXPECT_TRUE(SetLastRunTimeString(HKEY_CURRENT_USER, L"Hi Mum!"));
  int days_since_last_run = GoogleChromeDaysSinceLastRun();
  EXPECT_EQ(-1, days_since_last_run);
}

TEST_F(GCAPILastRunTest, OutOfRangeLastRun) {
  Time last_run = Time::NowFromSystemTime() - TimeDelta::FromDays(-42);
  EXPECT_TRUE(SetLastRunTime(HKEY_CURRENT_USER, last_run.ToInternalValue()));

  int days_since_last_run = GoogleChromeDaysSinceLastRun();
  EXPECT_EQ(-1, days_since_last_run);
}
