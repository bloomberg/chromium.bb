// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/guid.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/installer/gcapi/gcapi.h"
#include "chrome/installer/gcapi/gcapi_omaha_experiment.h"
#include "chrome/installer/gcapi/gcapi_reactivation.h"
#include "chrome/installer/util/google_update_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
using base::win::RegKey;

namespace {

const wchar_t kExperimentLabels[] = L"experiment_labels";

const wchar_t* kExperimentAppGuids[] = {
    L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}",
    L"{8A69D345-D564-463C-AFF1-A69D9E530F96}",
};

}

class GCAPIReactivationTest : public ::testing::Test {
 protected:
  void SetUp() {
    // Override keys - this is undone during destruction.
    std::wstring hkcu_override = base::StringPrintf(
        L"hkcu_override\\%ls", base::ASCIIToWide(base::GenerateGUID()));
    override_manager_.OverrideRegistry(HKEY_CURRENT_USER, hkcu_override);
    std::wstring hklm_override = base::StringPrintf(
        L"hklm_override\\%ls", base::ASCIIToWide(base::GenerateGUID()));
    override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE, hklm_override);
  }

  bool SetChromeInstallMarker(HKEY hive) {
    // Create the client state keys in the right places.
    std::wstring reg_path(google_update::kRegPathClients);
    reg_path += L"\\";
    reg_path += google_update::kChromeUpgradeCode;
    RegKey client_state(hive,
                        reg_path.c_str(),
                        KEY_CREATE_SUB_KEY | KEY_SET_VALUE);
    return (client_state.Valid() &&
            client_state.WriteValue(
                google_update::kRegVersionField, L"1.2.3.4") == ERROR_SUCCESS);
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

  bool HasExperimentLabels(HKEY hive) {
    int label_count = 0;
    for (int i = 0; i < arraysize(kExperimentAppGuids); ++i) {
      string16 client_state_path(google_update::kRegPathClientState);
      client_state_path += L"\\";
      client_state_path += kExperimentAppGuids[i];

      RegKey client_state_key(hive,
                              client_state_path.c_str(),
                              KEY_QUERY_VALUE);
      if (client_state_key.Valid() &&
          client_state_key.HasValue(kExperimentLabels)) {
        label_count++;
      }
    }
    return label_count == arraysize(kExperimentAppGuids);
  }

  std::wstring GetReactivationString(HKEY hive) {
    const wchar_t* base_path =
        (hive == HKEY_LOCAL_MACHINE) ?
            google_update::kRegPathClientStateMedium :
            google_update::kRegPathClientState;
    std::wstring path(base_path);
    path += L"\\";
    path += google_update::kChromeUpgradeCode;

    RegKey client_state(hive, path.c_str(), KEY_QUERY_VALUE);
    if (client_state.Valid()) {
      std::wstring actual_brand;
      if (client_state.ReadValue(google_update::kRegRLZReactivationBrandField,
                                 &actual_brand) == ERROR_SUCCESS) {
        return actual_brand;
      }
    }

    return L"ERROR";
  }

 private:
  registry_util::RegistryOverrideManager override_manager_;
};

TEST_F(GCAPIReactivationTest, CheckSetReactivationBrandCode) {
  EXPECT_TRUE(SetReactivationBrandCode(L"GAGA", GCAPI_INVOKED_STANDARD_SHELL));
  EXPECT_EQ(L"GAGA", GetReactivationString(HKEY_CURRENT_USER));

  EXPECT_TRUE(HasBeenReactivated());

}

TEST_F(GCAPIReactivationTest, CanOfferReactivation_Basic) {
  DWORD error;

  // We're not installed yet. Make sure CanOfferReactivation fails.
  EXPECT_FALSE(CanOfferReactivation(L"GAGA",
                                    GCAPI_INVOKED_STANDARD_SHELL,
                                    &error));
  EXPECT_EQ(REACTIVATE_ERROR_NOTINSTALLED, error);

  // Now pretend to be installed. CanOfferReactivation should pass.
  EXPECT_TRUE(SetChromeInstallMarker(HKEY_CURRENT_USER));
  EXPECT_TRUE(CanOfferReactivation(L"GAGA",
                                   GCAPI_INVOKED_STANDARD_SHELL,
                                   &error));

  // Now set a recent last_run value. CanOfferReactivation should fail again.
  Time hkcu_last_run = Time::NowFromSystemTime() - TimeDelta::FromDays(20);
  EXPECT_TRUE(SetLastRunTime(HKEY_CURRENT_USER,
                             hkcu_last_run.ToInternalValue()));
  EXPECT_FALSE(CanOfferReactivation(L"GAGA",
                                    GCAPI_INVOKED_STANDARD_SHELL,
                                    &error));
  EXPECT_EQ(REACTIVATE_ERROR_NOTDORMANT, error);

  // Now set a last_run value that exceeds the threshold.
  hkcu_last_run = Time::NowFromSystemTime() -
      TimeDelta::FromDays(kReactivationMinDaysDormant);
  EXPECT_TRUE(SetLastRunTime(HKEY_CURRENT_USER,
                             hkcu_last_run.ToInternalValue()));
  EXPECT_TRUE(CanOfferReactivation(L"GAGA",
                                   GCAPI_INVOKED_STANDARD_SHELL,
                                   &error));

  // Test some invalid inputs
  EXPECT_FALSE(CanOfferReactivation(NULL,
                                    GCAPI_INVOKED_STANDARD_SHELL,
                                    &error));
  EXPECT_EQ(REACTIVATE_ERROR_INVALID_INPUT, error);

  // One more valid one
  EXPECT_TRUE(CanOfferReactivation(L"GAGA",
                                   GCAPI_INVOKED_STANDARD_SHELL,
                                   &error));

  // Check that the previous brands check works:
  EXPECT_TRUE(SetReactivationBrandCode(L"GOOGOO",
                                       GCAPI_INVOKED_STANDARD_SHELL));
  EXPECT_FALSE(CanOfferReactivation(L"GAGA",
                                    GCAPI_INVOKED_STANDARD_SHELL,
                                    &error));
  EXPECT_EQ(REACTIVATE_ERROR_ALREADY_REACTIVATED, error);
}

TEST_F(GCAPIReactivationTest, Reactivation_Flow) {
  DWORD error;

  // Set us up as a candidate for reactivation.
  EXPECT_TRUE(SetChromeInstallMarker(HKEY_CURRENT_USER));

  Time hkcu_last_run = Time::NowFromSystemTime() -
      TimeDelta::FromDays(kReactivationMinDaysDormant);
  EXPECT_TRUE(SetLastRunTime(HKEY_CURRENT_USER,
                             hkcu_last_run.ToInternalValue()));

  EXPECT_TRUE(ReactivateChrome(L"GAGA",
                               GCAPI_INVOKED_STANDARD_SHELL,
                               &error));
  EXPECT_EQ(L"GAGA", GetReactivationString(HKEY_CURRENT_USER));

  // Make sure we can't reactivate again:
  EXPECT_FALSE(ReactivateChrome(L"GAGA",
                                GCAPI_INVOKED_STANDARD_SHELL,
                                &error));
  EXPECT_EQ(REACTIVATE_ERROR_ALREADY_REACTIVATED, error);

  // Should not be able to reactivate under other brands:
  EXPECT_FALSE(ReactivateChrome(L"MAMA",
                                GCAPI_INVOKED_STANDARD_SHELL,
                                &error));
  EXPECT_EQ(L"GAGA", GetReactivationString(HKEY_CURRENT_USER));

  // Validate that previous_brands are rejected:
  EXPECT_FALSE(ReactivateChrome(L"PFFT",
                                GCAPI_INVOKED_STANDARD_SHELL,
                                &error));
  EXPECT_EQ(REACTIVATE_ERROR_ALREADY_REACTIVATED, error);
  EXPECT_EQ(L"GAGA", GetReactivationString(HKEY_CURRENT_USER));
}

TEST_F(GCAPIReactivationTest, ExperimentLabelCheck) {
  DWORD error;

  // Set us up as a candidate for reactivation.
  EXPECT_TRUE(SetChromeInstallMarker(HKEY_CURRENT_USER));

  Time hkcu_last_run = Time::NowFromSystemTime() -
      TimeDelta::FromDays(kReactivationMinDaysDormant);
  EXPECT_TRUE(SetLastRunTime(HKEY_CURRENT_USER,
                             hkcu_last_run.ToInternalValue()));

  EXPECT_TRUE(ReactivateChrome(L"GAGA",
                               GCAPI_INVOKED_STANDARD_SHELL,
                               &error));
  EXPECT_EQ(L"GAGA", GetReactivationString(HKEY_CURRENT_USER));

  EXPECT_TRUE(HasExperimentLabels(HKEY_CURRENT_USER));
}
