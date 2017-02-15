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

// The group of client-side previews experiments. Actually, this group is only
// expected to control one PreviewsType (OFFLINE) as well as the blacklist.
// Other PreviewsType's will be control by different field trial groups.
const char kClientSidePreviewsFieldTrial[] = "ClientSidePreviews";

const char kEnabled[] = "Enabled";

// Allow offline pages to show for prohibitively slow networks.
const char kOfflinePagesSlowNetwork[] = "show_offline_pages";

// Name for the version parameter of a field trial. Version changes will
// result in older blacklist entries being removed.
const char kVersion[] = "version";

// The maximum number of recent previews navigations the black list looks at to
// determine if a host is blacklisted.
const char kMaxStoredHistoryLengthPerHost[] =
    "per_host_max_stored_history_length";

// The maximum number of recent previews navigations the black list looks at to
// determine if all previews navigations should be disallowed.
const char kMaxStoredHistoryLengthHostIndifferent[] =
    "host_indifferent_max_stored_history_length";

// The maximum number of hosts allowed in the in memory black list.
const char kMaxHostsInBlackList[] = "max_hosts_in_blacklist";

// The number of recent navigations that were opted out of that would trigger
// the host to be blacklisted.
const char kPerHostOptOutThreshold[] = "per_host_opt_out_threshold";

// The number of recent navigations that were opted out of that would trigger
// all previews navigations to be disallowed.
const char kHostIndifferentOptOutThreshold[] =
    "host_indifferent_opt_out_threshold";

// The amount of time a host remains blacklisted due to opt outs.
const char kPerHostBlackListDurationInDays[] =
    "per_host_black_list_duration_in_days";

// The amount of time a host remains blacklisted due to opt outs.
const char kHostIndifferentBlackListDurationInDays[] =
    "host_indifferent_black_list_duration_in_days";

// The amount of time after any opt out that no previews should be shown.
const char kSingleOptOutDurationInSeconds[] =
    "single_opt_out_duration_in_seconds";

// The amount of time that an offline page is considered fresh enough to be
// shown as a preview.
const char kOfflinePreviewFreshnessDurationInDays[] =
    "offline_preview_freshness_duration_in_days";

// The threshold of EffectiveConnectionType above which previews will not be
// served.
// See net/nqe/effective_connection_type.h for mapping from string to value.
const char kEffectiveConnectionTypeThreshold[] =
    "max_allowed_effective_connection_type";

// The string that corresponds to enabled for the variation param experiments.
const char kExperimentEnabled[] = "true";

// Returns the ClientSidePreviews parameter value of |param| as a string.
// If there is no value for |param|, returns an empty string.
std::string ClientSidePreviewsParamValue(const std::string& param) {
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

size_t MaxStoredHistoryLengthForPerHostBlackList() {
  std::string param_value =
      ClientSidePreviewsParamValue(kMaxStoredHistoryLengthPerHost);
  size_t history_length;
  if (!base::StringToSizeT(param_value, &history_length))
    history_length = 4;
  return history_length;
}

size_t MaxStoredHistoryLengthForHostIndifferentBlackList() {
  std::string param_value =
      ClientSidePreviewsParamValue(kMaxStoredHistoryLengthHostIndifferent);
  size_t history_length;
  if (!base::StringToSizeT(param_value, &history_length))
    history_length = 10;
  return history_length;
}

size_t MaxInMemoryHostsInBlackList() {
  std::string param_value = ClientSidePreviewsParamValue(kMaxHostsInBlackList);
  size_t max_hosts;
  if (!base::StringToSizeT(param_value, &max_hosts))
    max_hosts = 100;
  return max_hosts;
}

int PerHostBlackListOptOutThreshold() {
  std::string param_value =
      ClientSidePreviewsParamValue(kPerHostOptOutThreshold);
  int opt_out_threshold;
  if (!base::StringToInt(param_value, &opt_out_threshold))
    opt_out_threshold = 2;
  return opt_out_threshold;
}

int HostIndifferentBlackListOptOutThreshold() {
  std::string param_value =
      ClientSidePreviewsParamValue(kHostIndifferentOptOutThreshold);
  int opt_out_threshold;
  if (!base::StringToInt(param_value, &opt_out_threshold))
    opt_out_threshold = 4;
  return opt_out_threshold;
}

base::TimeDelta PerHostBlackListDuration() {
  std::string param_value =
      ClientSidePreviewsParamValue(kPerHostBlackListDurationInDays);
  int duration;
  if (!base::StringToInt(param_value, &duration))
    duration = 30;
  return base::TimeDelta::FromDays(duration);
}

base::TimeDelta HostIndifferentBlackListPerHostDuration() {
  std::string param_value =
      ClientSidePreviewsParamValue(kHostIndifferentBlackListDurationInDays);
  int duration;
  if (!base::StringToInt(param_value, &duration))
    duration = 365 * 100;
  return base::TimeDelta::FromDays(duration);
}

base::TimeDelta SingleOptOutDuration() {
  std::string param_value =
      ClientSidePreviewsParamValue(kSingleOptOutDurationInSeconds);
  int duration;
  if (!base::StringToInt(param_value, &duration))
    duration = 60 * 5;
  return base::TimeDelta::FromSeconds(duration);
}

base::TimeDelta OfflinePreviewFreshnessDuration() {
  std::string param_value =
      ClientSidePreviewsParamValue(kOfflinePreviewFreshnessDurationInDays);
  int duration;
  if (!base::StringToInt(param_value, &duration))
    duration = 7;
  return base::TimeDelta::FromDays(duration);
}

net::EffectiveConnectionType EffectiveConnectionTypeThreshold() {
  std::string param_value =
      ClientSidePreviewsParamValue(kEffectiveConnectionTypeThreshold);
  net::EffectiveConnectionType effective_connection_type;
  if (!net::GetEffectiveConnectionTypeForName(param_value,
                                              &effective_connection_type)) {
    effective_connection_type = net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G;
  }
  return effective_connection_type;
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
      return ClientSidePreviewsParamValue(kOfflinePagesSlowNetwork) ==
             kExperimentEnabled;
    default:
      NOTREACHED();
      return false;
  }
}

int GetPreviewsTypeVersion(PreviewsType type) {
  int version = 0;  // default
  switch (type) {
    case PreviewsType::OFFLINE:
      base::StringToInt(ClientSidePreviewsParamValue(kVersion), &version);
      return version;
    // List remaining enum cases vs. default to catch when new one is added.
    case PreviewsType::NONE:
      break;
    case PreviewsType::LAST:
      break;
  }
  NOTREACHED();
  return -1;
}

std::unique_ptr<PreviewsTypeList> GetEnabledPreviews() {
  std::unique_ptr<PreviewsTypeList> enabled_previews(new PreviewsTypeList());

  // Loop across all previews types (relies on sequential enum values).
  for (int i = static_cast<int>(PreviewsType::NONE) + 1;
       i < static_cast<int>(PreviewsType::LAST); ++i) {
    PreviewsType type = static_cast<PreviewsType>(i);
    if (IsPreviewsTypeEnabled(type)) {
      enabled_previews->push_back({type, GetPreviewsTypeVersion(type)});
    }
  }
  return enabled_previews;
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
