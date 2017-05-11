// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_experiments.h"

#include <map>
#include <string>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "components/variations/variations_associated_data.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

namespace {

const char kClientSidePreviewsFieldTrial[] = "ClientSidePreviews";
const char kClientLoFiFieldTrial[] = "PreviewsClientLoFi";
const char kEnabled[] = "Enabled";

// Verifies that we can enable offline previews via field trial.
TEST(PreviewsExperimentsTest, TestFieldTrialOfflinePage) {
  EXPECT_FALSE(params::IsOfflinePreviewsEnabled());

  base::FieldTrialList field_trial_list(nullptr);

  std::map<std::string, std::string> params;
  params["show_offline_pages"] = "true";
  EXPECT_TRUE(base::AssociateFieldTrialParams(kClientSidePreviewsFieldTrial,
                                              kEnabled, params));
  EXPECT_TRUE(base::FieldTrialList::CreateFieldTrial(
      kClientSidePreviewsFieldTrial, kEnabled));

  EXPECT_TRUE(params::IsOfflinePreviewsEnabled());
  variations::testing::ClearAllVariationParams();
}

// Verifies that we can enable offline previews via comand line.
TEST(PreviewsExperimentsTest, TestCommandLineOfflinePage) {
  EXPECT_FALSE(params::IsOfflinePreviewsEnabled());

  std::unique_ptr<base::FeatureList> feature_list =
      base::MakeUnique<base::FeatureList>();

  // The feature is explicitly enabled on the command-line.
  feature_list->InitializeFromCommandLine("OfflinePreviews", "");
  base::FeatureList::ClearInstanceForTesting();
  base::FeatureList::SetInstance(std::move(feature_list));

  EXPECT_TRUE(params::IsOfflinePreviewsEnabled());
  base::FeatureList::ClearInstanceForTesting();
}

// Verifies that the default params are correct, and that custom params can be
// set, for both the previews blacklist and offline previews.
TEST(PreviewsExperimentsTest, TestParamsForBlackListAndOffline) {
  // Verify that the default params are correct.
  EXPECT_EQ(4u, params::MaxStoredHistoryLengthForPerHostBlackList());
  EXPECT_EQ(10u, params::MaxStoredHistoryLengthForHostIndifferentBlackList());
  EXPECT_EQ(100u, params::MaxInMemoryHostsInBlackList());
  EXPECT_EQ(2, params::PerHostBlackListOptOutThreshold());
  EXPECT_EQ(4, params::HostIndifferentBlackListOptOutThreshold());
  EXPECT_EQ(base::TimeDelta::FromDays(30), params::PerHostBlackListDuration());
  EXPECT_EQ(base::TimeDelta::FromDays(365 * 100),
            params::HostIndifferentBlackListPerHostDuration());
  EXPECT_EQ(base::TimeDelta::FromSeconds(60 * 5),
            params::SingleOptOutDuration());
  EXPECT_EQ(base::TimeDelta::FromDays(7),
            params::OfflinePreviewFreshnessDuration());
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
            params::DefaultEffectiveConnectionTypeThreshold());
  EXPECT_EQ(0, params::OfflinePreviewsVersion());

  base::FieldTrialList field_trial_list(nullptr);

  // Set some custom params. Somewhat random yet valid values.
  std::map<std::string, std::string> custom_params = {
      {"per_host_max_stored_history_length", "3"},
      {"host_indifferent_max_stored_history_length", "4"},
      {"max_hosts_in_blacklist", "13"},
      {"per_host_opt_out_threshold", "12"},
      {"host_indifferent_opt_out_threshold", "84"},
      {"per_host_black_list_duration_in_days", "99"},
      {"host_indifferent_black_list_duration_in_days", "64"},
      {"single_opt_out_duration_in_seconds", "28"},
      {"offline_preview_freshness_duration_in_days", "12"},
      {"max_allowed_effective_connection_type", "4G"},
      {"version", "10"},
  };
  EXPECT_TRUE(base::AssociateFieldTrialParams(kClientSidePreviewsFieldTrial,
                                              kEnabled, custom_params));
  EXPECT_TRUE(base::FieldTrialList::CreateFieldTrial(
      kClientSidePreviewsFieldTrial, kEnabled));

  EXPECT_EQ(3u, params::MaxStoredHistoryLengthForPerHostBlackList());
  EXPECT_EQ(4u, params::MaxStoredHistoryLengthForHostIndifferentBlackList());
  EXPECT_EQ(13u, params::MaxInMemoryHostsInBlackList());
  EXPECT_EQ(12, params::PerHostBlackListOptOutThreshold());
  EXPECT_EQ(84, params::HostIndifferentBlackListOptOutThreshold());
  EXPECT_EQ(base::TimeDelta::FromDays(99), params::PerHostBlackListDuration());
  EXPECT_EQ(base::TimeDelta::FromDays(64),
            params::HostIndifferentBlackListPerHostDuration());
  EXPECT_EQ(base::TimeDelta::FromSeconds(28), params::SingleOptOutDuration());
  EXPECT_EQ(base::TimeDelta::FromDays(12),
            params::OfflinePreviewFreshnessDuration());
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_4G,
            params::DefaultEffectiveConnectionTypeThreshold());
  EXPECT_EQ(10, params::OfflinePreviewsVersion());

  variations::testing::ClearAllVariationParams();
}

TEST(PreviewsExperimentsTest, TestClientLoFiDisabledByDefault) {
  base::FieldTrialList field_trial_list(nullptr);
  EXPECT_FALSE(params::IsClientLoFiEnabled());
}

TEST(PreviewsExperimentsTest, TestClientLoFiExplicitlyDisabled) {
  base::FieldTrialList field_trial_list(nullptr);
  EXPECT_TRUE(
      base::FieldTrialList::CreateFieldTrial(kClientLoFiFieldTrial, kEnabled));
  EXPECT_TRUE(params::IsClientLoFiEnabled());
}

TEST(PreviewsExperimentsTest, TestClientLoFiEnabled) {
  base::FieldTrialList field_trial_list(nullptr);
  EXPECT_TRUE(
      base::FieldTrialList::CreateFieldTrial(kClientLoFiFieldTrial, kEnabled));
  EXPECT_TRUE(params::IsClientLoFiEnabled());
}

TEST(PreviewsExperimentsTest, TestEnableClientLoFiWithDefaultParams) {
  base::FieldTrialList field_trial_list(nullptr);
  EXPECT_TRUE(
      base::FieldTrialList::CreateFieldTrial(kClientLoFiFieldTrial, kEnabled));

  EXPECT_TRUE(params::IsClientLoFiEnabled());
  EXPECT_EQ(0, params::ClientLoFiVersion());
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            params::EffectiveConnectionTypeThresholdForClientLoFi());
}

TEST(PreviewsExperimentsTest, TestEnableClientLoFiWithCustomParams) {
  base::FieldTrialList field_trial_list(nullptr);

  // Set some custom params for Client LoFi.
  std::map<std::string, std::string> custom_params = {
      {"version", "10"}, {"max_allowed_effective_connection_type", "3G"},
  };
  EXPECT_TRUE(base::AssociateFieldTrialParams(kClientLoFiFieldTrial, kEnabled,
                                              custom_params));
  EXPECT_TRUE(
      base::FieldTrialList::CreateFieldTrial(kClientLoFiFieldTrial, kEnabled));

  EXPECT_TRUE(params::IsClientLoFiEnabled());
  EXPECT_EQ(10, params::ClientLoFiVersion());
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_3G,
            params::EffectiveConnectionTypeThresholdForClientLoFi());

  variations::testing::ClearAllVariationParams();
}

// Verifies that we can enable offline previews via comand line.
TEST(PreviewsExperimentsTest, TestCommandLineClientLoFi) {
  EXPECT_FALSE(params::IsClientLoFiEnabled());

  std::unique_ptr<base::FeatureList> feature_list =
      base::MakeUnique<base::FeatureList>();

  // The feature is explicitly enabled on the command-line.
  feature_list->InitializeFromCommandLine("ClientLoFi", "");
  base::FeatureList::ClearInstanceForTesting();
  base::FeatureList::SetInstance(std::move(feature_list));

  EXPECT_TRUE(params::IsClientLoFiEnabled());
  base::FeatureList::ClearInstanceForTesting();
}

}  // namespace

}  // namespace previews
