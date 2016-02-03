// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_retrieval_params.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"

namespace {

// The trial group prefix for which an experiment is considered enabled.
const char kEnabled[] = "Enabled";

// The minimum interval (in seconds) at which the config retrieval can occur.
const int kConfigFetchMinimumIntervalSeconds = 300;

// The default latency base for retrieving the config.
const int kConfigFetchDefaultRoundtripMillisecondsBase = 100;

// The default latency base for retrieving the config.
const double kConfigFetchDefaultRoundtripMultiplier = 1.0;

// The default latency base for retrieving the config.
const int kConfigFetchDefaultRoundtripMillisecondsIncrement = 100;

// The minimum Data Reduction Proxy configuration expiration.
const int kConfigFetchMinimumExpirationSeconds = 5 * 60;

// The default Data Reduction Proxy configuration expiration.
const int kConfigFetchDefaultExpirationSeconds = 24 * 60 * 60;

// Based on a histogram prefix |histogram| and |suffix|, retrieves the
// histogram for the expanded histogram name. The histogram is identical to
// the used in UMA_HISTOGRAM_COUNTS.
base::HistogramBase* GetHistogramWithSuffix(const char* histogram, int suffix) {
  std::string full_histogram_name = histogram + base::IntToString(suffix);
  return base::Histogram::FactoryGet(
      full_histogram_name, 1, 1000000, 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
}

// Retrieves the boolean stored in |variation| from the field trial group
// |group|. If the value is not present or cannot be parsed, returns
// |default_value|.
bool GetVariationBoolWithDefault(const char* group,
                                 const char* variation,
                                 bool default_value) {
  std::string variation_value =
      variations::GetVariationParamValue(group, variation);
  int64_t variation_numeric;
  if (variation_value.empty() ||
      !base::StringToInt64(variation_value, &variation_numeric)) {
    return default_value;
  }

  return variation_numeric != 0;
}

// Retrieves the int64_t stored in |variation| from the field trial group
// |group|. If the value is not present, cannot be parsed, or is less than
// |min_value|, returns |default_value|.
int64_t GetVariationInt64WithDefault(const char* group,
                                     const char* variation,
                                     int64_t default_value,
                                     int64_t min_value) {
  DCHECK(default_value >= min_value);
  std::string variation_value =
      variations::GetVariationParamValue(group, variation);
  int64_t variation_numeric;
  if (variation_value.empty() ||
      !base::StringToInt64(variation_value, &variation_numeric) ||
      variation_numeric < min_value) {
    return default_value;
  }

  return variation_numeric;
}

// Retrieves the double stored in |variation| from the field trial group
// |group|. If the value is not present, cannot be parsed, or is less than
// |min_value|, returns |default_value|.
double GetVariationDoubleWithDefault(const char* group,
                                     const char* variation,
                                     double default_value,
                                     double min_value) {
  DCHECK(default_value >= min_value);
  std::string variation_value =
      variations::GetVariationParamValue(group, variation);
  double variation_numeric;
  if (variation_value.empty() ||
      !base::StringToDouble(variation_value, &variation_numeric) ||
      variation_numeric < min_value) {
    return default_value;
  }

  return variation_numeric;
}

}  // namespace

namespace data_reduction_proxy {

const int kConfigFetchGroups = 10;

const char kConfigFetchTrialGroup[] = "DataReductionProxyConfigFetchBytes";

const char kConfigRoundtripMillisecondsBaseParam[] =
    "config_roundtrip_milliseconds_base";

const char kConfigRoundtripMultiplierParam[] = "config_roundtrip_multiplier";

const char kConfigRoundtripMillisecondsIncrementParam[] =
    "config_roundtrip_milliseconds_increment";

const char kConfigExpirationSecondsParam[] = "config_expiration_seconds";

const char kConfigAlwaysStaleParam[] = "always_stale";

const int kConfigFetchBufferSeconds = 300;

// static
scoped_ptr<DataReductionProxyConfigRetrievalParams>
DataReductionProxyConfigRetrievalParams::Create(PrefService* pref_service) {
  std::string group_value =
      base::FieldTrialList::FindFullName(kConfigFetchTrialGroup);
  base::StringPiece group = group_value;
  if (!group.starts_with(kEnabled))
    return scoped_ptr<DataReductionProxyConfigRetrievalParams>();

  base::Time now = base::Time::Now();
  base::Time config_retrieve;
  bool config_always_stale = GetVariationBoolWithDefault(
      kConfigFetchTrialGroup, kConfigAlwaysStaleParam, false);
  if (config_always_stale) {
    config_retrieve = base::Time();
  } else {
    int64_t config_retrieve_value =
        pref_service->GetInt64(prefs::kSimulatedConfigRetrieveTime);
    config_retrieve = base::Time::FromInternalValue(config_retrieve_value);
    if (config_retrieve > now)
      config_retrieve = base::Time();
  }

  int64_t config_expiration_interval_seconds = GetVariationInt64WithDefault(
      kConfigFetchTrialGroup, kConfigExpirationSecondsParam,
      kConfigFetchDefaultExpirationSeconds,
      kConfigFetchMinimumExpirationSeconds);
  base::TimeDelta config_expiration_interval =
      base::TimeDelta::FromSeconds(config_expiration_interval_seconds);
  base::Time config_expiration = config_retrieve + config_expiration_interval;
  std::vector<Variation> variations;
  bool expired_config = (now > config_expiration);
  if (expired_config) {
    config_expiration = now + config_expiration_interval;

    int64_t config_roundtrip_milliseconds = GetVariationInt64WithDefault(
        kConfigFetchTrialGroup, kConfigRoundtripMillisecondsBaseParam,
        kConfigFetchDefaultRoundtripMillisecondsBase, 0);
    double config_roundtrip_multiplier = GetVariationDoubleWithDefault(
        kConfigFetchTrialGroup, kConfigRoundtripMultiplierParam,
        kConfigFetchDefaultRoundtripMultiplier, 1.0);
    int64_t roundtrip_milliseconds_increment = GetVariationInt64WithDefault(
        kConfigFetchTrialGroup, kConfigRoundtripMillisecondsIncrementParam,
        kConfigFetchDefaultRoundtripMillisecondsIncrement, 0);

    for (int params_index = 0; params_index < kConfigFetchGroups;
         ++params_index) {
      base::Time config_retrieved = now + base::TimeDelta::FromMilliseconds(
                                              config_roundtrip_milliseconds);
      variations.push_back(Variation(params_index, config_retrieved));
      config_roundtrip_milliseconds *= config_roundtrip_multiplier;
      config_roundtrip_milliseconds += roundtrip_milliseconds_increment;
    }
  }

  return scoped_ptr<DataReductionProxyConfigRetrievalParams>(
      new DataReductionProxyConfigRetrievalParams(expired_config, variations,
                                                  config_expiration,
                                                  config_expiration_interval));
}

DataReductionProxyConfigRetrievalParams::Variation::Variation(
    int index,
    const base::Time& simulated_config_retrieved)
    : simulated_config_retrieved_(simulated_config_retrieved) {
  lost_bytes_ocl_ = GetHistogramWithSuffix(
      "DataReductionProxy.ConfigFetchLostBytesOCL_", index);
  lost_bytes_rcl_ = GetHistogramWithSuffix(
      "DataReductionProxy.ConfigFetchLostBytesCL_", index);
  lost_bytes_diff_ = GetHistogramWithSuffix(
      "DataReductionProxy.ConfigFetchLostBytesDiff_", index);
}

DataReductionProxyConfigRetrievalParams::ConfigState
DataReductionProxyConfigRetrievalParams::Variation::GetState(
    const base::Time& request_time,
    const base::Time& config_expiration) const {
  if (!simulated_config_retrieved_.is_null() &&
      request_time < simulated_config_retrieved_) {
    return DataReductionProxyConfigRetrievalParams::RETRIEVING;
  } else if (request_time < config_expiration) {
    return DataReductionProxyConfigRetrievalParams::VALID;
  }

  return DataReductionProxyConfigRetrievalParams::EXPIRED;
}

void DataReductionProxyConfigRetrievalParams::Variation::RecordStats(
    int64_t received_content_length,
    int64_t original_content_length) const {
  lost_bytes_rcl_->Add(received_content_length);
  lost_bytes_ocl_->Add(original_content_length);
  int64_t content_length_diff =
      original_content_length - received_content_length;
  if (content_length_diff > 0)
    lost_bytes_diff_->Add(content_length_diff);
}

DataReductionProxyConfigRetrievalParams::
    DataReductionProxyConfigRetrievalParams(
        bool loaded_expired_config,
        const std::vector<Variation>& variations,
        const base::Time& config_expiration,
        const base::TimeDelta& config_expiration_interval)
    : loaded_expired_config_(loaded_expired_config),
      config_expiration_(config_expiration),
      config_expiration_interval_(config_expiration_interval),
      variations_(variations) {
  config_refresh_interval_ =
      config_expiration_interval_ -
      base::TimeDelta::FromSeconds(kConfigFetchBufferSeconds);
  if (config_refresh_interval_.InSeconds() <
      kConfigFetchMinimumIntervalSeconds) {
    config_refresh_interval_ =
        base::TimeDelta::FromSeconds(kConfigFetchMinimumIntervalSeconds);
  }
}

DataReductionProxyConfigRetrievalParams::
    ~DataReductionProxyConfigRetrievalParams() {
}

void DataReductionProxyConfigRetrievalParams::RecordStats(
    const base::Time& request_time,
    int64_t received_content_length,
    int64_t original_content_length) const {
  for (const auto& variation : variations_) {
    switch (variation.GetState(request_time, config_expiration_)) {
      case VALID:
        break;
      case RETRIEVING:
      case EXPIRED:
        variation.RecordStats(received_content_length, original_content_length);
        break;
      default:
        NOTREACHED();
    }
  }
}

void DataReductionProxyConfigRetrievalParams::RefreshConfig() {
  config_expiration_ = base::Time::Now() + config_expiration_interval_;
}

}  // namespace data_reduction_proxy
