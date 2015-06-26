// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_quality_estimator.h"

#include <float.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace {

// Maximum number of observations that can be held in the ObservationBuffer.
const size_t kMaximumObservationsBufferSize = 500;

// Default value of the half life (in seconds) for computing time weighted
// percentiles. Every half life, the weight of all observations reduces by
// half. Lowering the half life would reduce the weight of older values faster.
const int kDefaultHalfLifeSeconds = 60;

// Name of the variation parameter that holds the value of the half life (in
// seconds) of the observations.
const char kHalfLifeSecondsParamName[] = "HalfLifeSeconds";

// Name of the different connection types. Used for matching the connection
// type to the variation parameters. Names must be in the same order as
// NetworkChangeNotifier::ConnectionType enum.
const char* const kConnectionTypeNames[] =
    {"Unknown", "Ethernet", "WiFi", "2G", "3G", "4G", "None", "Bluetooth"};

// Suffix of the name of the variation parameter that contains the default RTT
// observation (in milliseconds). Complete name of the variation parameter
// would be |ConnectionType|.|kDefaultRTTMsecObservationSuffix| where
// |ConnectionType| is from |kConnectionTypeNames|. For example, variation
// parameter for Wi-Fi would be "WiFi.DefaultMedianRTTMsec".
const char kDefaultRTTMsecObservationSuffix[] = ".DefaultMedianRTTMsec";

// Suffix of the name of the variation parameter that contains the default
// downstream throughput observation (in Kbps).  Complete name of the variation
// parameter would be |ConnectionType|.|kDefaultKbpsObservationSuffix| where
// |ConnectionType| is from |kConnectionTypeNames|. For example, variation
// parameter for Wi-Fi would be "WiFi.DefaultMedianKbps".
const char kDefaultKbpsObservationSuffix[] = ".DefaultMedianKbps";

// Computes and returns the weight multiplier per second.
// |variation_params| is the map containing all field trial parameters
// related to NetworkQualityEstimator field trial.
double GetWeightMultiplierPerSecond(
    const std::map<std::string, std::string>& variation_params) {
  int half_life_seconds = kDefaultHalfLifeSeconds;
  int32_t variations_value = 0;
  auto it = variation_params.find(kHalfLifeSecondsParamName);
  if (it != variation_params.end() &&
      base::StringToInt(it->second, &variations_value) &&
      variations_value >= 1) {
    half_life_seconds = variations_value;
  }
  DCHECK_GT(half_life_seconds, 0);
  return exp(log(0.5) / half_life_seconds);
}

}  // namespace

namespace net {

NetworkQualityEstimator::NetworkQualityEstimator(
    const std::map<std::string, std::string>& variation_params)
    : NetworkQualityEstimator(variation_params, false, false) {
}

NetworkQualityEstimator::NetworkQualityEstimator(
    const std::map<std::string, std::string>& variation_params,
    bool allow_local_host_requests_for_tests,
    bool allow_smaller_responses_for_tests)
    : allow_localhost_requests_(allow_local_host_requests_for_tests),
      allow_small_responses_(allow_smaller_responses_for_tests),
      last_connection_change_(base::TimeTicks::Now()),
      current_connection_type_(NetworkChangeNotifier::GetConnectionType()),
      fastest_rtt_since_last_connection_change_(NetworkQuality::InvalidRTT()),
      peak_kbps_since_last_connection_change_(
          NetworkQuality::kInvalidThroughput),
      kbps_observations_(GetWeightMultiplierPerSecond(variation_params)),
      rtt_msec_observations_(GetWeightMultiplierPerSecond(variation_params)) {
  static_assert(kMinRequestDurationMicroseconds > 0,
                "Minimum request duration must be > 0");
  static_assert(kDefaultHalfLifeSeconds > 0,
                "Default half life duration must be > 0");
  static_assert(arraysize(kConnectionTypeNames) ==
                    NetworkChangeNotifier::CONNECTION_LAST + 1,
                "ConnectionType name count should match");

  ObtainOperatingParams(variation_params);
  AddDefaultEstimates();
  NetworkChangeNotifier::AddConnectionTypeObserver(this);
}

void NetworkQualityEstimator::ObtainOperatingParams(
    const std::map<std::string, std::string>& variation_params) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (size_t i = 0; i < arraysize(kConnectionTypeNames); ++i) {
    int32_t variations_value = kMinimumRTTVariationParameterMsec - 1;
    // Name of the parameter that holds the RTT value for this connection type.
    std::string rtt_parameter_name =
        std::string(kConnectionTypeNames[i])
            .append(kDefaultRTTMsecObservationSuffix);
    auto it = variation_params.find(rtt_parameter_name);
    if (it != variation_params.end() &&
        base::StringToInt(it->second, &variations_value) &&
        variations_value >= kMinimumRTTVariationParameterMsec) {
      default_observations_[i] =
          NetworkQuality(base::TimeDelta::FromMilliseconds(variations_value),
                         default_observations_[i].downstream_throughput_kbps());
    }

    variations_value = kMinimumThroughputVariationParameterKbps - 1;
    // Name of the parameter that holds the Kbps value for this connection
    // type.
    std::string kbps_parameter_name =
        std::string(kConnectionTypeNames[i])
            .append(kDefaultKbpsObservationSuffix);
    it = variation_params.find(kbps_parameter_name);
    if (it != variation_params.end() &&
        base::StringToInt(it->second, &variations_value) &&
        variations_value >= kMinimumThroughputVariationParameterKbps) {
      default_observations_[i] =
          NetworkQuality(default_observations_[i].rtt(), variations_value);
    }
  }
}

void NetworkQualityEstimator::AddDefaultEstimates() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (default_observations_[current_connection_type_].rtt() !=
      NetworkQuality::InvalidRTT()) {
    rtt_msec_observations_.AddObservation(Observation(
        default_observations_[current_connection_type_].rtt().InMilliseconds(),
        base::TimeTicks::Now()));
  }

  if (default_observations_[current_connection_type_]
          .downstream_throughput_kbps() != NetworkQuality::kInvalidThroughput) {
    kbps_observations_.AddObservation(
        Observation(default_observations_[current_connection_type_]
                        .downstream_throughput_kbps(),
                    base::TimeTicks::Now()));
  }
}

NetworkQualityEstimator::~NetworkQualityEstimator() {
  DCHECK(thread_checker_.CalledOnValidThread());
  NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void NetworkQualityEstimator::NotifyDataReceived(
    const URLRequest& request,
    int64_t cumulative_prefilter_bytes_read,
    int64_t prefiltered_bytes_read) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GT(cumulative_prefilter_bytes_read, 0);
  DCHECK_GT(prefiltered_bytes_read, 0);

  if (!request.url().is_valid() ||
      (!allow_localhost_requests_ && IsLocalhost(request.url().host())) ||
      !request.url().SchemeIsHTTPOrHTTPS() ||
      // Verify that response headers are received, so it can be ensured that
      // response is not cached.
      request.response_info().response_time.is_null() || request.was_cached() ||
      request.creation_time() < last_connection_change_) {
    return;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  LoadTimingInfo load_timing_info;
  request.GetLoadTimingInfo(&load_timing_info);

  // If the load timing info is unavailable, it probably means that the request
  // did not go over the network.
  if (load_timing_info.send_start.is_null() ||
      load_timing_info.receive_headers_end.is_null()) {
    return;
  }

  // Time when the resource was requested.
  base::TimeTicks request_start_time = load_timing_info.send_start;

  // Time when the headers were received.
  base::TimeTicks headers_received_time = load_timing_info.receive_headers_end;

  // Only add RTT observation if this is the first read for this response.
  if (cumulative_prefilter_bytes_read == prefiltered_bytes_read) {
    // Duration between when the resource was requested and when response
    // headers were received.
    base::TimeDelta observed_rtt = headers_received_time - request_start_time;
    DCHECK_GE(observed_rtt, base::TimeDelta());
    if (observed_rtt < fastest_rtt_since_last_connection_change_)
      fastest_rtt_since_last_connection_change_ = observed_rtt;

    rtt_msec_observations_.AddObservation(
        Observation(observed_rtt.InMilliseconds(), now));
  }

  // Time since the resource was requested.
  base::TimeDelta since_request_start = now - request_start_time;
  DCHECK_GE(since_request_start, base::TimeDelta());

  // Ignore tiny transfers which will not produce accurate rates.
  // Ignore short duration transfers.
  // Skip the checks if |allow_small_responses_| is true.
  if (allow_small_responses_ ||
      (cumulative_prefilter_bytes_read >= kMinTransferSizeInBytes &&
       since_request_start >= base::TimeDelta::FromMicroseconds(
                                  kMinRequestDurationMicroseconds))) {
    double kbps_f = cumulative_prefilter_bytes_read * 8.0 / 1000.0 /
                    since_request_start.InSecondsF();
    DCHECK_GE(kbps_f, 0.0);

    // Check overflow errors. This may happen if the kbps_f is more than
    // 2 * 10^9 (= 2000 Gbps).
    if (kbps_f >= std::numeric_limits<int32_t>::max())
      kbps_f = std::numeric_limits<int32_t>::max() - 1;

    int32_t kbps = static_cast<int32_t>(kbps_f);

    // If the |kbps| is less than 1, we set it to 1 to differentiate from case
    // when there is no connection.
    if (kbps_f > 0.0 && kbps == 0)
      kbps = 1;

    if (kbps > 0) {
      if (kbps > peak_kbps_since_last_connection_change_)
        peak_kbps_since_last_connection_change_ = kbps;

      kbps_observations_.AddObservation(Observation(kbps, now));
    }
  }
}

void NetworkQualityEstimator::OnConnectionTypeChanged(
    NetworkChangeNotifier::ConnectionType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (fastest_rtt_since_last_connection_change_ !=
      NetworkQuality::InvalidRTT()) {
    switch (current_connection_type_) {
      case NetworkChangeNotifier::CONNECTION_UNKNOWN:
        UMA_HISTOGRAM_TIMES("NQE.FastestRTT.Unknown",
                            fastest_rtt_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_ETHERNET:
        UMA_HISTOGRAM_TIMES("NQE.FastestRTT.Ethernet",
                            fastest_rtt_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_WIFI:
        UMA_HISTOGRAM_TIMES("NQE.FastestRTT.Wifi",
                            fastest_rtt_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_2G:
        UMA_HISTOGRAM_TIMES("NQE.FastestRTT.2G",
                            fastest_rtt_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_3G:
        UMA_HISTOGRAM_TIMES("NQE.FastestRTT.3G",
                            fastest_rtt_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_4G:
        UMA_HISTOGRAM_TIMES("NQE.FastestRTT.4G",
                            fastest_rtt_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_NONE:
        UMA_HISTOGRAM_TIMES("NQE.FastestRTT.None",
                            fastest_rtt_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_BLUETOOTH:
        UMA_HISTOGRAM_TIMES("NQE.FastestRTT.Bluetooth",
                            fastest_rtt_since_last_connection_change_);
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  if (peak_kbps_since_last_connection_change_ !=
      NetworkQuality::kInvalidThroughput) {
    switch (current_connection_type_) {
      case NetworkChangeNotifier::CONNECTION_UNKNOWN:
        UMA_HISTOGRAM_COUNTS("NQE.PeakKbps.Unknown",
                             peak_kbps_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_ETHERNET:
        UMA_HISTOGRAM_COUNTS("NQE.PeakKbps.Ethernet",
                             peak_kbps_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_WIFI:
        UMA_HISTOGRAM_COUNTS("NQE.PeakKbps.Wifi",
                             peak_kbps_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_2G:
        UMA_HISTOGRAM_COUNTS("NQE.PeakKbps.2G",
                             peak_kbps_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_3G:
        UMA_HISTOGRAM_COUNTS("NQE.PeakKbps.3G",
                             peak_kbps_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_4G:
        UMA_HISTOGRAM_COUNTS("NQE.PeakKbps.4G",
                             peak_kbps_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_NONE:
        UMA_HISTOGRAM_COUNTS("NQE.PeakKbps.None",
                             peak_kbps_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_BLUETOOTH:
        UMA_HISTOGRAM_COUNTS("NQE.PeakKbps.Bluetooth",
                             peak_kbps_since_last_connection_change_);
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  last_connection_change_ = base::TimeTicks::Now();
  peak_kbps_since_last_connection_change_ = NetworkQuality::kInvalidThroughput;
  fastest_rtt_since_last_connection_change_ = NetworkQuality::InvalidRTT();
  kbps_observations_.Clear();
  rtt_msec_observations_.Clear();
  current_connection_type_ = type;

  AddDefaultEstimates();
}

NetworkQuality NetworkQualityEstimator::GetPeakEstimate() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return NetworkQuality(fastest_rtt_since_last_connection_change_,
                        peak_kbps_since_last_connection_change_);
}

bool NetworkQualityEstimator::GetEstimate(NetworkQuality* median) const {
  if (kbps_observations_.Size() == 0 || rtt_msec_observations_.Size() == 0) {
    *median = NetworkQuality();
    return false;
  }
  *median = GetEstimate(50);
  return true;
}

size_t NetworkQualityEstimator::GetMaximumObservationBufferSizeForTests()
    const {
  return kMaximumObservationsBufferSize;
}

bool NetworkQualityEstimator::VerifyBufferSizeForTests(
    size_t expected_size) const {
  return kbps_observations_.Size() == expected_size &&
         rtt_msec_observations_.Size() == expected_size;
}

NetworkQualityEstimator::Observation::Observation(int32_t value,
                                                  base::TimeTicks timestamp)
    : value(value), timestamp(timestamp) {
  DCHECK_GE(value, 0);
  DCHECK(!timestamp.is_null());
}

NetworkQualityEstimator::Observation::~Observation() {
}

NetworkQualityEstimator::ObservationBuffer::ObservationBuffer(
    double weight_multiplier_per_second)
    : weight_multiplier_per_second_(weight_multiplier_per_second) {
  static_assert(kMaximumObservationsBufferSize > 0U,
                "Minimum size of observation buffer must be > 0");
  DCHECK_GE(weight_multiplier_per_second_, 0.0);
  DCHECK_LE(weight_multiplier_per_second_, 1.0);
}

NetworkQualityEstimator::ObservationBuffer::~ObservationBuffer() {
}

void NetworkQualityEstimator::ObservationBuffer::AddObservation(
    const Observation& observation) {
  DCHECK_LE(observations_.size(), kMaximumObservationsBufferSize);
  // Evict the oldest element if the buffer is already full.
  if (observations_.size() == kMaximumObservationsBufferSize)
    observations_.pop_front();

  observations_.push_back(observation);
  DCHECK_LE(observations_.size(), kMaximumObservationsBufferSize);
}

size_t NetworkQualityEstimator::ObservationBuffer::Size() const {
  return observations_.size();
}

void NetworkQualityEstimator::ObservationBuffer::Clear() {
  observations_.clear();
  DCHECK(observations_.empty());
}

NetworkQuality NetworkQualityEstimator::GetEstimate(int percentile) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GE(percentile, 0);
  DCHECK_LE(percentile, 100);
  DCHECK_GT(kbps_observations_.Size(), 0U);
  DCHECK_GT(rtt_msec_observations_.Size(), 0U);

  // RTT observations are sorted by duration from shortest to longest, thus
  // a higher percentile RTT will have a longer RTT than a lower percentile.
  // Throughput observations are sorted by kbps from slowest to fastest,
  // thus a higher percentile throughput will be faster than a lower one.
  return NetworkQuality(base::TimeDelta::FromMilliseconds(
                            rtt_msec_observations_.GetPercentile(percentile)),
                        kbps_observations_.GetPercentile(100 - percentile));
}

void NetworkQualityEstimator::ObservationBuffer::ComputeWeightedObservations(
    std::vector<WeightedObservation>& weighted_observations,
    double* total_weight) const {
  weighted_observations.clear();
  double total_weight_observations = 0.0;
  base::TimeTicks now = base::TimeTicks::Now();

  for (const auto& observation : observations_) {
    base::TimeDelta time_since_sample_taken = now - observation.timestamp;
    double weight =
        pow(weight_multiplier_per_second_, time_since_sample_taken.InSeconds());
    weight = std::max(DBL_MIN, std::min(1.0, weight));

    weighted_observations.push_back(
        WeightedObservation(observation.value, weight));
    total_weight_observations += weight;
  }

  // Sort the samples by value in ascending order.
  std::sort(weighted_observations.begin(), weighted_observations.end());
  *total_weight = total_weight_observations;
}

int32_t NetworkQualityEstimator::ObservationBuffer::GetPercentile(
    int percentile) const {
  DCHECK(!observations_.empty());

  // Stores WeightedObservation in increasing order of value.
  std::vector<WeightedObservation> weighted_observations;

  // Total weight of all observations in |weighted_observations|.
  double total_weight = 0.0;

  ComputeWeightedObservations(weighted_observations, &total_weight);
  DCHECK(!weighted_observations.empty());
  DCHECK_GT(total_weight, 0.0);
  DCHECK_EQ(observations_.size(), weighted_observations.size());

  double desired_weight = percentile / 100.0 * total_weight;

  double cumulative_weight_seen_so_far = 0.0;
  for (const auto& weighted_observation : weighted_observations) {
    cumulative_weight_seen_so_far += weighted_observation.weight;

    // TODO(tbansal): Consider interpolating between observations.
    if (cumulative_weight_seen_so_far >= desired_weight)
      return weighted_observation.value;
  }

  // Computation may reach here due to floating point errors. This may happen
  // if |percentile| was 100 (or close to 100), and |desired_weight| was
  // slightly larger than |total_weight| (due to floating point errors).
  // In this case, we return the highest |value| among all observations.
  // This is same as value of the last observation in the sorted vector.
  return weighted_observations.at(weighted_observations.size() - 1).value;
}

}  // namespace net
