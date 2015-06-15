// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_quality_estimator.h"

#include <limits>
#include <string>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_util.h"
#include "net/base/network_quality.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace {

// Maximum number of observations that can be held in the ObservationBuffer.
const size_t kMaximumObservationsBufferSize = 500;

}  // namespace

namespace net {

NetworkQualityEstimator::NetworkQualityEstimator()
    : NetworkQualityEstimator(false, false) {
}

NetworkQualityEstimator::NetworkQualityEstimator(
    bool allow_local_host_requests_for_tests,
    bool allow_smaller_responses_for_tests)
    : allow_localhost_requests_(allow_local_host_requests_for_tests),
      allow_small_responses_(allow_smaller_responses_for_tests),
      last_connection_change_(base::TimeTicks::Now()),
      current_connection_type_(NetworkChangeNotifier::GetConnectionType()),
      fastest_rtt_since_last_connection_change_(base::TimeDelta::Max()),
      peak_kbps_since_last_connection_change_(0) {
  static_assert(kMinRequestDurationMicroseconds > 0,
                "Minimum request duration must be > 0");
  NetworkChangeNotifier::AddConnectionTypeObserver(this);
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

    // Check overflow errors. This may happen if the kbpsF is more than
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
  if (fastest_rtt_since_last_connection_change_ != base::TimeDelta::Max()) {
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

  if (peak_kbps_since_last_connection_change_) {
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
  peak_kbps_since_last_connection_change_ = 0;
  fastest_rtt_since_last_connection_change_ = base::TimeDelta::Max();
  kbps_observations_.Clear();
  rtt_msec_observations_.Clear();
  current_connection_type_ = type;
}

NetworkQuality NetworkQualityEstimator::GetPeakEstimate() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return NetworkQuality(fastest_rtt_since_last_connection_change_,
                        peak_kbps_since_last_connection_change_);
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

NetworkQualityEstimator::ObservationBuffer::ObservationBuffer() {
  static_assert(kMaximumObservationsBufferSize > 0U,
                "Minimum size of observation buffer must be > 0");
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

}  // namespace net
