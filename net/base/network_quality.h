// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_QUALITY_H_
#define NET_BASE_NETWORK_QUALITY_H_

#include <stdint.h>

#include "base/time/time.h"

namespace net {

// API that is used to report the current network quality as estimated by the
// NetworkQualityEstimator.
struct NET_EXPORT_PRIVATE NetworkQuality {
  NetworkQuality(const base::TimeDelta& fastest_rtt,
                 double fastest_rtt_confidence,
                 uint64_t peak_throughput_kbps,
                 double peak_throughput_kbps_confidence)
      : fastest_rtt(fastest_rtt),
        fastest_rtt_confidence(fastest_rtt_confidence),
        peak_throughput_kbps(peak_throughput_kbps),
        peak_throughput_kbps_confidence(peak_throughput_kbps_confidence) {}

  ~NetworkQuality() {}

  // The fastest round trip time observed for the current connection.
  const base::TimeDelta fastest_rtt;

  // Confidence of the |fastest_rtt| estimate. Value lies between 0.0 and 1.0
  // with 0.0 being no confidence and 1.0 implying that estimates are same as
  // ground truth.
  // TODO(tbansal): Define units so values intermediate between 0.0 and 1.0 are
  // meaningful.
  const double fastest_rtt_confidence;

  // Peak throughput in Kbps observed for the current connection.
  const uint64_t peak_throughput_kbps;

  // Confidence of the |peak_throughput_kbps| estimate. Value lies between 0.0
  // and 1.0 with 0.0 being no confidence and 1.0 implying that estimates are
  // same as ground truth.
  // TODO(tbansal): Define units so values intermediate between 0.0 and 1.0 are
  // meaningful.
  const double peak_throughput_kbps_confidence;
};

}  // namespace net

#endif  // NET_BASE_NETWORK_QUALITY_H_
