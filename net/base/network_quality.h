// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_QUALITY_H_
#define NET_BASE_NETWORK_QUALITY_H_

#include <stdint.h>

#include "base/time/time.h"
#include "net/base/net_export.h"

namespace net {

// API that is used to report the network quality of a network connection as
// estimated by the NetworkQualityEstimator.
class NET_EXPORT_PRIVATE NetworkQuality {
 public:
  // |rtt| is the estimate of the round trip time.
  // |downstream_throughput_kbps| is the estimate of the downstream throughput.
  NetworkQuality(const base::TimeDelta& rtt,
                 int32_t downstream_throughput_kbps);

  ~NetworkQuality();

  // Returns the estimate of the round trip time.
  const base::TimeDelta& rtt() const { return rtt_; }

  // Returns the estimate of the downstream throughput in Kbps (Kilo bits per
  // second).
  int32_t downstream_throughput_kbps() const {
    return downstream_throughput_kbps_;
  }

 private:
  // Estimated round trip time.
  const base::TimeDelta rtt_;

  // Estimated downstream throughput in Kbps.
  const int32_t downstream_throughput_kbps_;
};

}  // namespace net

#endif  // NET_BASE_NETWORK_QUALITY_H_
