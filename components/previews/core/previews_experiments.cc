// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_experiments.h"

#include <map>
#include <string>

#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/variations/variations_associated_data.h"

namespace previews {

namespace {

// The group of client-side previews experiments.
const char kClientSidePreviewsFieldTrial[] = "ClientSidePreviews";

const char kEnabled[] = "Enabled";

// Allow offline pages to show for prohibitively slow networks.
const char kOfflinePagesSlowNetwork[] = "show_offline_pages";

// The maximum number of recent previews navigations the black list looks at to
// determine if a host is blacklisted.
const char kMaxStoredHistoryLength[] = "stored_history_length";

// The maximum number of hosts allowed in the in memory black list.
const char kMaxHostsInBlackList[] = "max_hosts_in_blacklist";

// The number of recent navigations that were opted out of that would trigger
// the host to be blacklisted.
const char kOptOutThreshold[] = "opt_out_threshold";

// The amount of time a host remains blacklisted due to opt outs.
const char kBlackListDurationInDays[] = "black_list_duration_in_days";

// The amount of time after any opt out that no previews should be shown.
const char kSingleOptOutDurationInSeconds[] =
    "single_opt_out_duration_in_seconds";

// The string that corresponds to enabled for the variation param experiments.
const char kExperimentEnabled[] = "true";

// In seconds. Hosts are blacklisted for 30 days.
const int kDefaultBlackListDurationInDays = 30;

// In seconds. Previews are not shown for 5 minutes after an opt out.
constexpr int kDefaultSingleOptOutDurationInSeconds = 60 * 5;

// Returns the parameter value of |param| as a string. If there is no value for
// |param|, returns an empty string.
std::string ParamValue(const std::string& param) {
  if (!IsIncludedInClientSidePreviewsExperimentsFieldTrial())
    return std::string();
  std::map<std::string, std::string> experiment_params;
  if (!variations::GetVariationParams(kClientSidePreviewsFieldTrial,
                                      &experiment_params)) {
    return std::string();
  }
  std::map<std::string, std::string>::const_iterator it =
      experiment_params.find(param);
  return it == experiment_params.end() ? std::string() : it->second;
}

}  // namespace

namespace params {

size_t MaxStoredHistoryLengthForBlackList() {
  std::string param_value = ParamValue(kMaxStoredHistoryLength);
  size_t history_length;
  if (!base::StringToSizeT(param_value, &history_length)) {
    return 4;
  }
  return history_length;
}

size_t MaxInMemoryHostsInBlackList() {
  std::string param_value = ParamValue(kMaxHostsInBlackList);
  size_t max_hosts;
  if (!base::StringToSizeT(param_value, &max_hosts)) {
    return 100;
  }
  return max_hosts;
}

int BlackListOptOutThreshold() {
  std::string param_value = ParamValue(kOptOutThreshold);
  int opt_out_threshold;
  if (!base::StringToInt(param_value, &opt_out_threshold)) {
    return 2;
  }
  return opt_out_threshold;
}

base::TimeDelta BlackListDuration() {
  std::string param_value = ParamValue(kBlackListDurationInDays);
  int duration;
  if (!base::StringToInt(param_value, &duration)) {
    return base::TimeDelta::FromDays(kDefaultBlackListDurationInDays);
  }
  return base::TimeDelta::FromDays(duration);
}

base::TimeDelta SingleOptOutDuration() {
  std::string param_value = ParamValue(kSingleOptOutDurationInSeconds);
  int duration;
  if (!base::StringToInt(param_value, &duration)) {
    return base::TimeDelta::FromSeconds(kDefaultSingleOptOutDurationInSeconds);
  }
  return base::TimeDelta::FromSeconds(duration);
}

}  // namespace params

bool IsIncludedInClientSidePreviewsExperimentsFieldTrial() {
  // By convention, an experiment in the client-side previews study enables use
  // of at least one client-side previews optimization if its name begins with
  // "Enabled."
  return base::StartsWith(
      base::FieldTrialList::FindFullName(kClientSidePreviewsFieldTrial),
      kEnabled, base::CompareCase::SENSITIVE);
}

bool IsPreviewsTypeEnabled(PreviewsType type) {
  switch (type) {
    case PreviewsType::OFFLINE:
      return ParamValue(kOfflinePagesSlowNetwork) == kExperimentEnabled;
    default:
      NOTREACHED();
      return false;
  }
}

bool EnableOfflinePreviewsForTesting() {
  std::map<std::string, std::string> params;
  params[kOfflinePagesSlowNetwork] = kExperimentEnabled;
  return variations::AssociateVariationParams(kClientSidePreviewsFieldTrial,
                                              kEnabled, params) &&
         base::FieldTrialList::CreateFieldTrial(kClientSidePreviewsFieldTrial,
                                                kEnabled);
}

}  // namespace previews
