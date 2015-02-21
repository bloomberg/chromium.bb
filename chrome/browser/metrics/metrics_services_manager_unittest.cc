// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_services_manager.h"

#include <string>

#include "base/metrics/field_trial.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/rappor/rappor_pref_names.h"
#include "components/rappor/rappor_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if defined(GOOGLE_CHROME_BUILD)

void UseRapporOption() {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "RapporOption", "Enabled"));
}

#endif  // defined(GOOGLE_CHROME_BUILD)

}  // namespace

class MetricsServicesManagerTest : public testing::Test {
 public:
  MetricsServicesManagerTest()
      : test_profile_manager_(TestingBrowserProcess::GetGlobal()),
        manager_(&test_prefs_),
        field_trial_list_(NULL) {
    rappor::internal::RegisterPrefs(test_prefs_.registry());
  }

  void SetUp() override {
    ASSERT_TRUE(test_profile_manager_.SetUp());
  }

  void CreateProfile(const std::string& name, bool sb_enabled) {
    TestingProfile* profile = test_profile_manager_.CreateTestingProfile(name);
    profile->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, sb_enabled);
  }

  TestingPrefServiceSimple* test_prefs() { return &test_prefs_; }

  MetricsServicesManager* manager() { return &manager_; }

 private:
  TestingProfileManager test_profile_manager_;
  TestingPrefServiceSimple test_prefs_;
  MetricsServicesManager manager_;
  base::FieldTrialList field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(MetricsServicesManagerTest);
};

TEST_F(MetricsServicesManagerTest, CheckRapporDefaultDisable) {
  test_prefs()->ClearPref(rappor::prefs::kRapporEnabled);
  CreateProfile("profile1", true);
  CreateProfile("profile2", false);
  bool uma_enabled = false;

  EXPECT_FALSE(manager()->IsRapporEnabled(uma_enabled));
  EXPECT_TRUE(test_prefs()->HasPrefPath(rappor::prefs::kRapporEnabled));
  EXPECT_FALSE(test_prefs()->GetBoolean(rappor::prefs::kRapporEnabled));
}

TEST_F(MetricsServicesManagerTest, CheckRapporDefaultEnabledBySafeBrowsing) {
  test_prefs()->ClearPref(rappor::prefs::kRapporEnabled);
  CreateProfile("profile1", true);
  CreateProfile("profile2", true);
  bool uma_enabled = false;

  EXPECT_TRUE(manager()->IsRapporEnabled(uma_enabled));
  EXPECT_TRUE(test_prefs()->HasPrefPath(rappor::prefs::kRapporEnabled));
  EXPECT_TRUE(test_prefs()->GetBoolean(rappor::prefs::kRapporEnabled));
}

TEST_F(MetricsServicesManagerTest, CheckRapporDefaultEnabledByUMA) {
  test_prefs()->ClearPref(rappor::prefs::kRapporEnabled);
  CreateProfile("profile1", false);
  CreateProfile("profile2", false);
  bool uma_enabled = true;

  EXPECT_TRUE(manager()->IsRapporEnabled(uma_enabled));
  EXPECT_TRUE(test_prefs()->HasPrefPath(rappor::prefs::kRapporEnabled));
  EXPECT_TRUE(test_prefs()->GetBoolean(rappor::prefs::kRapporEnabled));
}

TEST_F(MetricsServicesManagerTest, CheckRapporEnable) {
  test_prefs()->SetBoolean(rappor::prefs::kRapporEnabled, true);
  CreateProfile("profile1", false);
  CreateProfile("profile2", false);
  bool uma_enabled = false;

  EXPECT_TRUE(manager()->IsRapporEnabled(uma_enabled));
  EXPECT_TRUE(test_prefs()->HasPrefPath(rappor::prefs::kRapporEnabled));
  EXPECT_TRUE(test_prefs()->GetBoolean(rappor::prefs::kRapporEnabled));
}

TEST_F(MetricsServicesManagerTest, CheckRapporDisable) {
  test_prefs()->SetBoolean(rappor::prefs::kRapporEnabled, false);
  CreateProfile("profile1", true);
  CreateProfile("profile2", true);
  bool uma_enabled = true;

  EXPECT_FALSE(manager()->IsRapporEnabled(uma_enabled));
  EXPECT_TRUE(test_prefs()->HasPrefPath(rappor::prefs::kRapporEnabled));
  EXPECT_FALSE(test_prefs()->GetBoolean(rappor::prefs::kRapporEnabled));
}

#if defined(GOOGLE_CHROME_BUILD)

TEST_F(MetricsServicesManagerTest, GetRecordingLevelDisabled) {
  UseRapporOption();
  test_prefs()->SetBoolean(rappor::prefs::kRapporEnabled, false);
  bool uma_enabled = true;

  EXPECT_EQ(rappor::RECORDING_DISABLED,
            manager()->GetRapporRecordingLevel(uma_enabled));
}

TEST_F(MetricsServicesManagerTest, GetRecordingLevelFine) {
  UseRapporOption();
  test_prefs()->SetBoolean(rappor::prefs::kRapporEnabled, true);
  bool uma_enabled = true;

  EXPECT_EQ(rappor::FINE_LEVEL,
            manager()->GetRapporRecordingLevel(uma_enabled));
}

TEST_F(MetricsServicesManagerTest, GetRecordingLevelCoarse) {
  UseRapporOption();
  test_prefs()->SetBoolean(rappor::prefs::kRapporEnabled, true);
  bool uma_enabled = false;

  EXPECT_EQ(rappor::COARSE_LEVEL,
            manager()->GetRapporRecordingLevel(uma_enabled));
}

#endif  // defined(GOOGLE_CHROME_BUILD)
