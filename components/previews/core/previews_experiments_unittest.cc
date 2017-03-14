// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_experiments.h"

#include <string>

#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/variations/variations_associated_data.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

namespace {

using PreviewsExperimentsTest = testing::Test;

// Used as field trial values. Somewhat random yet valid values.
const char kMaxStoredHistoryLengthPerHost[] = "3";
const char kMaxStoredHistoryLengthHostIndifferent[] = "4";
const char kMaxHostsInBlackList[] = "13";
const char kPerHostOptOutThreshold[] = "12";
const char kHostIndifferentOptOutThreshold[] = "84";
const char kPerHostBlackListDurationInDays[] = "99";
const char kHostIndifferentBlackListDurationInDays[] = "64";
const char kSingleOptOutDurationInSeconds[] = "28";
const char kOfflinePreviewFreshnessDurationInDays[] = "12";
const char kEffectiveConnectionTypeThreshold[] = "4G";

// Verifies that the default params are the expected values.
void VerifyDefaultParams() {
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
            params::EffectiveConnectionTypeThreshold());
}

// Creates a map of the params names to the const string values above.
std::map<std::string, std::string> GetFieldTrialParams() {
  std::map<std::string, std::string> params;
  // Assign different values than defaults.
  params["per_host_max_stored_history_length"] = kMaxStoredHistoryLengthPerHost;
  params["host_indifferent_max_stored_history_length"] =
      kMaxStoredHistoryLengthHostIndifferent;
  params["max_hosts_in_blacklist"] = kMaxHostsInBlackList;
  params["per_host_opt_out_threshold"] = kPerHostOptOutThreshold;
  params["host_indifferent_opt_out_threshold"] =
      kHostIndifferentOptOutThreshold;
  params["per_host_black_list_duration_in_days"] =
      kPerHostBlackListDurationInDays;
  params["host_indifferent_black_list_duration_in_days"] =
      kHostIndifferentBlackListDurationInDays;
  params["single_opt_out_duration_in_seconds"] = kSingleOptOutDurationInSeconds;
  params["offline_preview_freshness_duration_in_days"] =
      kOfflinePreviewFreshnessDurationInDays;
  params["max_allowed_effective_connection_type"] =
      kEffectiveConnectionTypeThreshold;

  return params;
}

// Verifies that calling the params methods returns the correct values based on
// the const string values above.
void VerifyNewParams() {
  {
    size_t history_length;
    EXPECT_TRUE(
        base::StringToSizeT(kMaxStoredHistoryLengthPerHost, &history_length));
    EXPECT_EQ(history_length,
              params::MaxStoredHistoryLengthForPerHostBlackList());
  }
  {
    size_t history_length;
    EXPECT_TRUE(base::StringToSizeT(kMaxStoredHistoryLengthHostIndifferent,
                                    &history_length));
    EXPECT_EQ(history_length,
              params::MaxStoredHistoryLengthForHostIndifferentBlackList());
  }
  {
    size_t history_length;
    EXPECT_TRUE(base::StringToSizeT(kMaxHostsInBlackList, &history_length));
    EXPECT_EQ(history_length, params::MaxInMemoryHostsInBlackList());
  }
  {
    int threshold;
    EXPECT_TRUE(base::StringToInt(kPerHostOptOutThreshold, &threshold));
    EXPECT_EQ(threshold, params::PerHostBlackListOptOutThreshold());
  }
  {
    int threshold;
    EXPECT_TRUE(base::StringToInt(kHostIndifferentOptOutThreshold, &threshold));
    EXPECT_EQ(threshold, params::HostIndifferentBlackListOptOutThreshold());
  }
  {
    int days;
    EXPECT_TRUE(base::StringToInt(kPerHostBlackListDurationInDays, &days));
    EXPECT_EQ(base::TimeDelta::FromDays(days),
              params::PerHostBlackListDuration());
  }
  {
    int days;
    EXPECT_TRUE(
        base::StringToInt(kHostIndifferentBlackListDurationInDays, &days));
    EXPECT_EQ(base::TimeDelta::FromDays(days),
              params::HostIndifferentBlackListPerHostDuration());
  }
  {
    int seconds;
    EXPECT_TRUE(base::StringToInt(kSingleOptOutDurationInSeconds, &seconds));
    EXPECT_EQ(base::TimeDelta::FromSeconds(seconds),
              params::SingleOptOutDuration());
  }
  {
    int days;
    EXPECT_TRUE(
        base::StringToInt(kOfflinePreviewFreshnessDurationInDays, &days));
    EXPECT_EQ(base::TimeDelta::FromDays(days),
              params::OfflinePreviewFreshnessDuration());
  }
  {
    net::EffectiveConnectionType effective_connection_type;
    EXPECT_TRUE(net::GetEffectiveConnectionTypeForName(
        kEffectiveConnectionTypeThreshold, &effective_connection_type));
    EXPECT_EQ(effective_connection_type,
              params::EffectiveConnectionTypeThreshold());
  }
}

}  // namespace

// Verifies that we can enable offline previews via field trial.
TEST_F(PreviewsExperimentsTest, TestFieldTrialOfflinePage) {
  EXPECT_FALSE(IsIncludedInClientSidePreviewsExperimentsFieldTrial());
  EXPECT_FALSE(IsPreviewsTypeEnabled(PreviewsType::OFFLINE));

  base::FieldTrialList field_trial_list(nullptr);
  ASSERT_TRUE(EnableOfflinePreviewsForTesting());

  EXPECT_TRUE(IsIncludedInClientSidePreviewsExperimentsFieldTrial());
  EXPECT_TRUE(IsPreviewsTypeEnabled(PreviewsType::OFFLINE));
  variations::testing::ClearAllVariationParams();
}

// Verifies that we can enable offline previews via field trial and that the
// default params for previews field trials are accurate.
TEST_F(PreviewsExperimentsTest, TestAllDefaultParams) {
  VerifyDefaultParams();
  base::FieldTrialList field_trial_list(nullptr);
  EXPECT_TRUE(variations::AssociateVariationParams(
      "ClientSidePreviews", "Enabled", GetFieldTrialParams()));
  EXPECT_TRUE(
      base::FieldTrialList::CreateFieldTrial("ClientSidePreviews", "Enabled"));
  VerifyNewParams();
  variations::testing::ClearAllVariationParams();
}

}  // namespace previews
