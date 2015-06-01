// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_quality_estimator.h"

#include <string>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "net/base/net_util.h"
#include "net/base/network_quality.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace net {

NetworkQualityEstimator::NetworkQualityEstimator()
    : NetworkQualityEstimator(false) {
}

NetworkQualityEstimator::NetworkQualityEstimator(
    bool allow_local_host_requests_for_tests)
    : allow_localhost_requests_(allow_local_host_requests_for_tests),
      last_connection_change_(base::TimeTicks::Now()),
      current_connection_type_(NetworkChangeNotifier::GetConnectionType()),
      bytes_read_since_last_connection_change_(false),
      peak_kbps_since_last_connection_change_(0) {
  static_assert(kMinRequestDurationMicroseconds > 0,
                "Minimum request duration must be > 0");
  NetworkChangeNotifier::AddConnectionTypeObserver(this);
}

NetworkQualityEstimator::~NetworkQualityEstimator() {
  DCHECK(thread_checker_.CalledOnValidThread());
  NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void NetworkQualityEstimator::NotifyDataReceived(const URLRequest& request,
                                                 int64_t prefilter_bytes_read) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GT(prefilter_bytes_read, 0);

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
  base::TimeDelta request_duration = now - request.creation_time();
  DCHECK_GE(request_duration, base::TimeDelta());
  if (!bytes_read_since_last_connection_change_)
    fastest_RTT_since_last_connection_change_ = request_duration;

  bytes_read_since_last_connection_change_ = true;
  if (request_duration < fastest_RTT_since_last_connection_change_)
    fastest_RTT_since_last_connection_change_ = request_duration;

  // Ignore tiny transfers which will not produce accurate rates.
  // Ignore short duration transfers.
  if (prefilter_bytes_read >= kMinTransferSizeInBytes &&
      request_duration >=
          base::TimeDelta::FromMicroseconds(kMinRequestDurationMicroseconds)) {
    uint64_t kbps = static_cast<uint64_t>(prefilter_bytes_read * 8 * 1000 /
                                          request_duration.InMicroseconds());
    if (kbps > peak_kbps_since_last_connection_change_)
      peak_kbps_since_last_connection_change_ = kbps;
  }
}

void NetworkQualityEstimator::OnConnectionTypeChanged(
    NetworkChangeNotifier::ConnectionType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (bytes_read_since_last_connection_change_) {
    switch (current_connection_type_) {
      case NetworkChangeNotifier::CONNECTION_UNKNOWN:
        UMA_HISTOGRAM_TIMES("NQE.FastestRTT.Unknown",
                            fastest_RTT_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_ETHERNET:
        UMA_HISTOGRAM_TIMES("NQE.FastestRTT.Ethernet",
                            fastest_RTT_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_WIFI:
        UMA_HISTOGRAM_TIMES("NQE.FastestRTT.Wifi",
                            fastest_RTT_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_2G:
        UMA_HISTOGRAM_TIMES("NQE.FastestRTT.2G",
                            fastest_RTT_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_3G:
        UMA_HISTOGRAM_TIMES("NQE.FastestRTT.3G",
                            fastest_RTT_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_4G:
        UMA_HISTOGRAM_TIMES("NQE.FastestRTT.4G",
                            fastest_RTT_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_NONE:
        UMA_HISTOGRAM_TIMES("NQE.FastestRTT.None",
                            fastest_RTT_since_last_connection_change_);
        break;
      case NetworkChangeNotifier::CONNECTION_BLUETOOTH:
        UMA_HISTOGRAM_TIMES("NQE.FastestRTT.Bluetooth",
                            fastest_RTT_since_last_connection_change_);
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
  bytes_read_since_last_connection_change_ = false;
  peak_kbps_since_last_connection_change_ = 0;
  current_connection_type_ = type;
}

NetworkQuality NetworkQualityEstimator::GetEstimate() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!bytes_read_since_last_connection_change_) {
    return NetworkQuality(fastest_RTT_since_last_connection_change_, 0,
                          peak_kbps_since_last_connection_change_, 0);
  }
  if (!peak_kbps_since_last_connection_change_) {
    return NetworkQuality(fastest_RTT_since_last_connection_change_, 0.1,
                          peak_kbps_since_last_connection_change_, 0);
  }
  return NetworkQuality(fastest_RTT_since_last_connection_change_, 0.1,
                        peak_kbps_since_last_connection_change_, 0.1);
}

}  // namespace net
