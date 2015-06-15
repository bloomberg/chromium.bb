// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_QUALITY_ESTIMATOR_H_
#define NET_BASE_NETWORK_QUALITY_ESTIMATOR_H_

#include <stdint.h>

#include <deque>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"

namespace net {

class NetworkQuality;

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

  // Returns the peak estimates (fastest RTT and peak throughput) of the
  // current network.
  // Virtualized for testing.
  virtual NetworkQuality GetPeakEstimate() const;

  // Notifies NetworkQualityEstimator that a response has been received.
  // |cumulative_prefilter_bytes_read| is the count of the bytes received prior
  // to applying filters (e.g. decompression, SDCH) from request creation time
  // until now.
  // |prefiltered_bytes_read| is the count of the bytes received prior
  // to applying filters in the most recent read.
  void NotifyDataReceived(const URLRequest& request,
                          int64_t cumulative_prefilter_bytes_read,
                          int64_t prefiltered_bytes_read);

 private:
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest, StoreObservations);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest,
                           TestPeakKbpsFastestRTTUpdates);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest, TestAddObservation);
  FRIEND_TEST_ALL_PREFIXES(URLRequestTestHTTP, NetworkQualityEstimator);

  // Records the round trip time or throughput observation, along with the time
  // the observation was made.
  struct Observation {
    Observation(int32_t value, base::TimeTicks timestamp);

    ~Observation();

    // Value of the observation.
    const int32_t value;

    // Time when the observation was taken.
    const base::TimeTicks timestamp;
  };

  // Stores observations sorted by time.
  class ObservationBuffer {
   public:
    ObservationBuffer();

    ~ObservationBuffer();

    // Adds |observation| to the buffer. The oldest observation in the buffer
    // will be evicted to make room if the buffer is already full.
    void AddObservation(const Observation& observation);

    // Returns the number of observations in this buffer.
    size_t Size() const;

    // Clears the observations stored in this buffer.
    void Clear();

   private:
    FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest, StoreObservations);

    // Holds observations sorted by time, with the oldest observation at the
    // front of the queue.
    std::deque<Observation> observations_;

    DISALLOW_COPY_AND_ASSIGN(ObservationBuffer);
  };

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
  // |allow_smaller_responses_for_tests| should only be true when testing
  // against local HTTP server and allows the responses smaller than
  // |kMinTransferSizeInBytes| or shorter than |kMinRequestDurationMicroseconds|
  // to be used for network quality estimation.
  NetworkQualityEstimator(bool allow_local_host_requests_for_tests,
                          bool allow_smaller_responses_for_tests);

  // Returns the maximum size of the observation buffer.
  // Used for testing.
  size_t GetMaximumObservationBufferSizeForTests() const;

  // Returns true if the size of all observation buffers is equal to the
  // |expected_size|. Used for testing.
  bool VerifyBufferSizeForTests(size_t expected_size) const;

  // NetworkChangeNotifier::ConnectionTypeObserver implementation.
  void OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType type) override;

  // Determines if the requests to local host can be used in estimating the
  // network quality. Set to true only for tests.
  const bool allow_localhost_requests_;

  // Determines if the responses smaller than |kMinTransferSizeInBytes|
  // or shorter than |kMinTransferSizeInBytes| can be used in estimating the
  // network quality. Set to true only for tests.
  const bool allow_small_responses_;

  // Time when last connection change was observed.
  base::TimeTicks last_connection_change_;

  // Last value passed to |OnConnectionTypeChanged|. This indicates the
  // current connection type.
  NetworkChangeNotifier::ConnectionType current_connection_type_;

  // Fastest round-trip-time (RTT) since last connectivity change. RTT measured
  // from URLRequest creation until first byte received.
  base::TimeDelta fastest_rtt_since_last_connection_change_;

  // Rough measurement of downstream peak Kbps witnessed since last connectivity
  // change. The accuracy is decreased by ignoring these factors:
  // 1) Multiple URLRequests can occur concurrently.
  // 2) The transfer time includes at least one RTT while no bytes are read.
  int32_t peak_kbps_since_last_connection_change_;

  // Buffer that holds Kbps observations.
  ObservationBuffer kbps_observations_;

  // Buffer that holds RTT (in milliseconds) observations.
  ObservationBuffer rtt_msec_observations_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(NetworkQualityEstimator);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_QUALITY_ESTIMATOR_H_
