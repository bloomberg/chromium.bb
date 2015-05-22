// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_QUALITY_ESTIMATOR_H_
#define NET_BASE_NETWORK_QUALITY_ESTIMATOR_H_

#include <stdint.h>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "net/base/network_change_notifier.h"

namespace net {

struct NetworkQuality;

// NetworkQualityEstimator provides network quality estimates (quality of the
// full paths to all origins that have been connected to).
// The estimates are based on the observed organic traffic.
// A NetworkQualityEstimator instance is attached to URLRequestContexts and
// observes the traffic of URLRequests spawned from the URLRequestContexts.
// A single instance of NQE can be attached to multiple URLRequestContexts,
// thereby increasing the single NQE instance's accuracy by providing more
// observed traffic characteristics.
class NET_EXPORT_PRIVATE NetworkQualityEstimator
    : public NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  // Creates a new NetworkQualityEstimator.
  NetworkQualityEstimator();

  ~NetworkQualityEstimator() override;

  // Returns an estimate of the current network quality.
  NetworkQuality GetEstimate() const;

  // Notifies NetworkQualityEstimator that a response has been received.
  // |prefilter_bytes_read| is the count of the bytes received prior to
  // applying filters (e.g. decompression, SDCH) from request creation time
  // until now.
  void NotifyDataReceived(const URLRequest& request,
                          int64_t prefilter_bytes_read);

 private:
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest,
                           TestPeakKbpsFastestRTTUpdates);
  FRIEND_TEST_ALL_PREFIXES(URLRequestTestHTTP, NetworkQualityEstimator);

  // Tiny transfer sizes may give inaccurate throughput results.
  // Minimum size of the transfer over which the throughput is computed.
  static const int kMinTransferSizeInBytes = 10000;

  // Minimum duration (in microseconds) of the transfer over which the
  // throughput is computed.
  static const int kMinRequestDurationMicroseconds = 1000;

  // Construct a NetworkQualityEstimator instance allowing for test
  // configuration.
  // Registers for network type change notifications so estimates can be kept
  // network specific.
  // |allow_local_host_requests_for_tests| should only be true when testing
  // against local HTTP server and allows the requests to local host to be
  // used for network quality estimation.
  explicit NetworkQualityEstimator(bool allow_local_host_requests_for_tests);

  // NetworkChangeNotifier::ConnectionTypeObserver implementation.
  void OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType type) override;

  // Determines if the requests to local host can be used in estimating the
  // network quality. Set to true only for tests.
  const bool allow_localhost_requests_;

  // Time when last connection change was observed.
  base::TimeTicks last_connection_change_;

  // Last value passed to |OnConnectionTypeChanged|. This indicates the
  // current connection type.
  NetworkChangeNotifier::ConnectionType current_connection_type_;

  // Set if any network data has been received since last connectivity change.
  bool bytes_read_since_last_connection_change_;

  // Fastest round-trip-time (RTT) since last connectivity change. RTT measured
  // from URLRequest creation until first byte received.
  base::TimeDelta fastest_RTT_since_last_connection_change_;

  // Rough measurement of downlink peak Kbps witnessed since last connectivity
  // change. The accuracy is decreased by ignoring these factors:
  // 1) Multiple URLRequests can occur concurrently.
  // 2) The transfer time includes at least one RTT while no bytes are read.
  uint64_t peak_kbps_since_last_connection_change_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(NetworkQualityEstimator);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_QUALITY_ESTIMATOR_H_
