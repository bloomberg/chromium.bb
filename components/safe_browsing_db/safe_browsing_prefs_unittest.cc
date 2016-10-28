// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing_db/safe_browsing_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SafeBrowsingPrefsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    prefs_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingExtendedReportingEnabled, false);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingScoutReportingEnabled, false);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingScoutGroupSelected, false);

    ResetExperiments(/*can_show_scout=*/false, /*only_show_scout=*/false);
  }

  void ResetPrefs(bool sber_reporting, bool scout_reporting, bool scout_group) {
    prefs_.SetBoolean(prefs::kSafeBrowsingExtendedReportingEnabled,
                      sber_reporting);
    prefs_.SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                      scout_reporting);
    prefs_.SetBoolean(prefs::kSafeBrowsingScoutGroupSelected, scout_group);
  }

  void ResetExperiments(bool can_show_scout, bool only_show_scout) {
    std::vector<std::string> enabled_features;
    std::vector<std::string> disabled_features;

    auto* target_vector =
        can_show_scout ? &enabled_features : &disabled_features;
    target_vector->push_back(kCanShowScoutOptIn.name);

    target_vector = only_show_scout ? &enabled_features : &disabled_features;
    target_vector->push_back(kOnlyShowScoutOptIn.name);

    feature_list_.reset(new base::test::ScopedFeatureList);
    feature_list_->InitFromCommandLine(
        base::JoinString(enabled_features, ","),
        base::JoinString(disabled_features, ","));
  }

  std::string GetActivePref() { return GetExtendedReportingPrefName(prefs_); }

  // Convenience method for explicitly setting up all combinations of prefs and
  // experiments.
  void TestGetPrefName(bool sber_reporting,
                       bool scout_reporting,
                       bool scout_group,
                       bool can_show_scout,
                       bool only_show_scout,
                       const std::string& expected_pref) {
    ResetPrefs(sber_reporting, scout_reporting, scout_group);
    ResetExperiments(can_show_scout, only_show_scout);
    EXPECT_EQ(expected_pref, GetActivePref())
        << "sber=" << sber_reporting << " scout=" << scout_reporting
        << " scout_group=" << scout_group
        << " can_show_scout=" << can_show_scout
        << " only_show_scout=" << only_show_scout;
  }

 private:
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<base::test::ScopedFeatureList> feature_list_;
};

// This test ensures that we correctly select between SBER and Scout as the
// active preference in a number of common scenarios.
TEST_F(SafeBrowsingPrefsTest, GetExtendedReportingPrefName_Common) {
  const std::string& sber = prefs::kSafeBrowsingExtendedReportingEnabled;
  const std::string& scout = prefs::kSafeBrowsingScoutReportingEnabled;

  // By default (all prefs and experiment features disabled), SBER pref is used.
  TestGetPrefName(false, false, false, false, false, sber);

  // Changing any prefs (including ScoutGroupSelected) keeps SBER as the active
  // pref because the experiment remains in the Control group.
  TestGetPrefName(/*sber=*/true, false, false, false, false, sber);
  TestGetPrefName(false, /*scout=*/true, false, false, false, sber);
  TestGetPrefName(false, false, /*scout_group=*/true, false, false, sber);

  // Being in either experiment group with ScoutGroup selected makes Scout the
  // active pref.
  TestGetPrefName(false, false, /*scout_group=*/true, /*can_show_scout=*/true,
                  false, scout);
  TestGetPrefName(false, false, /*scout_group=*/true, false,
                  /*only_show_scout=*/true, scout);

  // When ScoutGroup is not selected then SBER remains the active pref,
  // regardless which experiment is enabled.
  TestGetPrefName(false, false, false, /*can_show_scout=*/true, false, sber);
  TestGetPrefName(false, false, false, false, /*only_show_scout=*/true, sber);
}

// Here we exhaustively check all combinations of pref and experiment states.
// This should help catch regressions.
TEST_F(SafeBrowsingPrefsTest, GetExtendedReportingPrefName_Exhaustive) {
  const std::string& sber = prefs::kSafeBrowsingExtendedReportingEnabled;
  const std::string& scout = prefs::kSafeBrowsingScoutReportingEnabled;
  TestGetPrefName(false, false, false, false, false, sber);
  TestGetPrefName(false, false, false, false, true, sber);
  TestGetPrefName(false, false, false, true, false, sber);
  TestGetPrefName(false, false, false, true, true, sber);
  TestGetPrefName(false, false, true, false, false, sber);
  TestGetPrefName(false, false, true, false, true, scout);
  TestGetPrefName(false, false, true, true, false, scout);
  TestGetPrefName(false, false, true, true, true, scout);
  TestGetPrefName(false, true, false, false, false, sber);
  TestGetPrefName(false, true, false, false, true, sber);
  TestGetPrefName(false, true, false, true, false, sber);
  TestGetPrefName(false, true, false, true, true, sber);
  TestGetPrefName(false, true, true, false, false, sber);
  TestGetPrefName(false, true, true, false, true, scout);
  TestGetPrefName(false, true, true, true, false, scout);
  TestGetPrefName(false, true, true, true, true, scout);
  TestGetPrefName(true, false, false, false, false, sber);
  TestGetPrefName(true, false, false, false, true, sber);
  TestGetPrefName(true, false, false, true, false, sber);
  TestGetPrefName(true, false, false, true, true, sber);
  TestGetPrefName(true, false, true, false, false, sber);
  TestGetPrefName(true, false, true, false, true, scout);
  TestGetPrefName(true, false, true, true, false, scout);
  TestGetPrefName(true, false, true, true, true, scout);
  TestGetPrefName(true, true, false, false, false, sber);
  TestGetPrefName(true, true, false, false, true, sber);
  TestGetPrefName(true, true, false, true, false, sber);
  TestGetPrefName(true, true, false, true, true, sber);
  TestGetPrefName(true, true, true, false, false, sber);
  TestGetPrefName(true, true, true, false, true, scout);
  TestGetPrefName(true, true, true, true, false, scout);
  TestGetPrefName(true, true, true, true, true, scout);
}

}  // namespace safe_browsing
