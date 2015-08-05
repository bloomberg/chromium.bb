// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_QUALITY_ESTIMATOR_H_
#define NET_BASE_NETWORK_QUALITY_ESTIMATOR_H_

#include <stdint.h>

#include <deque>
#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_quality.h"

namespace net {

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
  // |variation_params| is the map containing all field trial parameters
  // related to NetworkQualityEstimator field trial.
  explicit NetworkQualityEstimator(
      const std::map<std::string, std::string>& variation_params);

  ~NetworkQualityEstimator() override;

  // Returns the peak estimates (fastest RTT and peak throughput) of the
  // current network.
  // Virtualized for testing.
  virtual NetworkQuality GetPeakEstimate() const;

  // Sets |median| to the estimate of median network quality. The estimated
  // quality is computed using a weighted median algorithm that assigns higher
  // weight to the recent observations. |median| must not be nullptr. Returns
  // true only if an estimate of the network quality is available (enough
  // observations must be available to make an estimate). Virtualized for
  // testing. If the estimate is not available, |median| is set to the default
  // value.
  virtual bool GetEstimate(NetworkQuality* median) const;

  // Returns true if RTT is available and sets |rtt| to estimated RTT.
  bool GetRTTEstimate(base::TimeDelta* rtt) const;

  // Returns true if downlink throughput is available and sets |kbps| to
  // estimated downlink throughput (in Kilobits per second).
  bool GetDownlinkThroughputKbpsEstimate(int32_t* kbps) const;

  // Notifies NetworkQualityEstimator that a response has been received.
  // |cumulative_prefilter_bytes_read| is the count of the bytes received prior
  // to applying filters (e.g. decompression, SDCH) from request creation time
  // until now.
  // |prefiltered_bytes_read| is the count of the bytes received prior
  // to applying filters in the most recent read.
  void NotifyDataReceived(const URLRequest& request,
                          int64_t cumulative_prefilter_bytes_read,
                          int64_t prefiltered_bytes_read);

  // Returns the weighted median of RTT observations available since
  // |begin_timestamp|.
  base::TimeDelta GetMedianRTTSince(
      const base::TimeTicks& begin_timestamp) const;

 protected:
  // NetworkID is used to uniquely identify a network.
  // For the purpose of network quality estimation and caching, a network is
  // uniquely identified by a combination of |type| and
  // |id|. This approach is unable to distinguish networks with
  // same name (e.g., different Wi-Fi networks with same SSID).
  // This is a protected member to expose it to tests.
  struct NET_EXPORT_PRIVATE NetworkID {
    NetworkID(NetworkChangeNotifier::ConnectionType type, const std::string& id)
        : type(type), id(id) {}
    NetworkID(const NetworkID& other) : type(other.type), id(other.id) {}
    ~NetworkID() {}

    NetworkID& operator=(const NetworkID& other) {
      type = other.type;
      id = other.id;
      return *this;
    }

    // Overloaded because NetworkID is used as key in a map.
    bool operator<(const NetworkID& other) const {
      return type < other.type || (type == other.type && id < other.id);
    }

    // Connection type of the network.
    NetworkChangeNotifier::ConnectionType type;

    // Name of this network. This is set to:
    // - Wi-Fi SSID if the device is connected to a Wi-Fi access point and the
    //   SSID name is available, or
    // - MCC/MNC code of the cellular carrier if the device is connected to a
    //   cellular network, or
    // - "Ethernet" in case the device is connected to ethernet.
    // - An empty string in all other cases or if the network name is not
    //   exposed by platform APIs.
    std::string id;
  };

  // Construct a NetworkQualityEstimator instance allowing for test
  // configuration. Registers for network type change notifications so estimates
  // can be kept network specific.
  // |variation_params| is the map containing all field trial parameters for the
  // network quality estimator field trial.
  // |allow_local_host_requests_for_tests| should only be true when testing
  // against local HTTP server and allows the requests to local host to be
  // used for network quality estimation.
  // |allow_smaller_responses_for_tests| should only be true when testing.
  // Allows the responses smaller than |kMinTransferSizeInBytes| or shorter than
  // |kMinRequestDurationMicroseconds| to be used for network quality
  // estimation.
  NetworkQualityEstimator(
      const std::map<std::string, std::string>& variation_params,
      bool allow_local_host_requests_for_tests,
      bool allow_smaller_responses_for_tests);

  // Returns true if the cached network quality estimate was successfully read.
  bool ReadCachedNetworkQualityEstimate();

  // NetworkChangeNotifier::ConnectionTypeObserver implementation.
  void OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType type) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest, StoreObservations);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest, TestKbpsRTTUpdates);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest, TestAddObservation);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest, ObtainOperatingParams);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest, HalfLifeParam);
  FRIEND_TEST_ALL_PREFIXES(URLRequestTestHTTP, NetworkQualityEstimator);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest,
                           PercentileSameTimestamps);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest,
                           PercentileDifferentTimestamps);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest, ComputedPercentiles);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest, TestCaching);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest,
                           TestLRUCacheMaximumSize);
  FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest, TestGetMedianRTTSince);

  // CachedNetworkQuality stores the quality of a previously seen network.
  class NET_EXPORT_PRIVATE CachedNetworkQuality {
   public:
    explicit CachedNetworkQuality(const NetworkQuality& network_quality);
    CachedNetworkQuality(const CachedNetworkQuality& other);
    ~CachedNetworkQuality();

    // Returns the network quality associated with this cached entry.
    const NetworkQuality network_quality() const { return network_quality_; }

    // Returns true if this cache entry was updated before
    // |cached_network_quality|.
    bool OlderThan(const CachedNetworkQuality& cached_network_quality) const;

    // Time when this cache entry was last updated.
    const base::TimeTicks last_update_time_;

    // Quality of this cached network.
    const NetworkQuality network_quality_;

   private:
    DISALLOW_ASSIGN(CachedNetworkQuality);
  };

  // Records the round trip time or throughput observation, along with the time
  // the observation was made.
  struct NET_EXPORT_PRIVATE Observation {
    Observation(int32_t value, base::TimeTicks timestamp);
    ~Observation();

    // Value of the observation.
    const int32_t value;

    // Time when the observation was taken.
    const base::TimeTicks timestamp;
  };

  // Holds an observation and its weight.
  struct NET_EXPORT_PRIVATE WeightedObservation {
    WeightedObservation(int32_t value, double weight)
        : value(value), weight(weight) {}
    WeightedObservation(const WeightedObservation& other)
        : WeightedObservation(other.value, other.weight) {}

    WeightedObservation& operator=(const WeightedObservation& other) {
      value = other.value;
      weight = other.weight;
      return *this;
    }

    // Required for sorting the samples in the ascending order of values.
    bool operator<(const WeightedObservation& other) const {
      return (value < other.value);
    }

    // Value of the sample.
    int32_t value;

    // Weight of the sample. This is computed based on how much time has passed
    // since the sample was taken.
    double weight;
  };

  // Stores observations sorted by time.
  class NET_EXPORT_PRIVATE ObservationBuffer {
   public:
    explicit ObservationBuffer(double weight_multiplier_per_second);
    ~ObservationBuffer();

    // Adds |observation| to the buffer. The oldest observation in the buffer
    // will be evicted to make room if the buffer is already full.
    void AddObservation(const Observation& observation);

    // Returns the number of observations in this buffer.
    size_t Size() const;

    // Clears the observations stored in this buffer.
    void Clear();

    // Returns true iff the |percentile| value of the observations in this
    // buffer is available. Sets |result| to the computed |percentile|
    // value among all observations since |begin_timestamp|. If the value is
    // unavailable, false is returned and |result| is not modified. Percentile
    // value is unavailable if all the values in observation buffer are older
    // than |begin_timestamp|.
    // |result| must not be null.
    bool GetPercentile(const base::TimeTicks& begin_timestamp,
                       int32_t* result,
                       int percentile) const;

   private:
    FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest, StoreObservations);
    FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest,
                             ObtainOperatingParams);
    FRIEND_TEST_ALL_PREFIXES(NetworkQualityEstimatorTest, HalfLifeParam);

    // Computes the weighted observations and stores them in
    // |weighted_observations| sorted by ascending |WeightedObservation.value|.
    // Only the observations with timestamp later than |begin_timestamp| are
    // considered. Also, sets |total_weight| to the total weight of all
    // observations. Should be called only when there is at least one
    // observation in the buffer.
    void ComputeWeightedObservations(
        const base::TimeTicks& begin_timestamp,
        std::vector<WeightedObservation>& weighted_observations,
        double* total_weight) const;

    // Holds observations sorted by time, with the oldest observation at the
    // front of the queue.
    std::deque<Observation> observations_;

    // The factor by which the weight of an observation reduces every second.
    // For example, if an observation is 6 seconds old, its weight would be:
    //     weight_multiplier_per_second_ ^ 6
    // Calculated from |kHalfLifeSeconds| by solving the following equation:
    //     weight_multiplier_per_second_ ^ kHalfLifeSeconds = 0.5
    const double weight_multiplier_per_second_;

    DISALLOW_COPY_AND_ASSIGN(ObservationBuffer);
  };

  // This does not use a unordered_map or hash_map for code simplicity (key just
  // implements operator<, rather than hash and equality) and because the map is
  // tiny.
  typedef std::map<NetworkID, CachedNetworkQuality> CachedNetworkQualities;

  // Tiny transfer sizes may give inaccurate throughput results.
  // Minimum size of the transfer over which the throughput is computed.
  static const int kMinTransferSizeInBytes = 10000;

  // Minimum duration (in microseconds) of the transfer over which the
  // throughput is computed.
  static const int kMinRequestDurationMicroseconds = 1000;

  // Minimum valid value of the variation parameter that holds RTT (in
  // milliseconds) values.
  static const int kMinimumRTTVariationParameterMsec = 1;

  // Minimum valid value of the variation parameter that holds throughput (in
  // kbps) values.
  static const int kMinimumThroughputVariationParameterKbps = 1;

  // Maximum size of the cache that holds network quality estimates.
  // Smaller size may reduce the cache hit rate due to frequent evictions.
  // Larger size may affect performance.
  static const size_t kMaximumNetworkQualityCacheSize = 10;

  // Maximum number of observations that can be held in the ObservationBuffer.
  static const size_t kMaximumObservationsBufferSize = 300;

  // Obtains operating parameters from the field trial parameters.
  void ObtainOperatingParams(
      const std::map<std::string, std::string>& variation_params);

  // Adds the default median RTT and downstream throughput estimate for the
  // current connection type to the observation buffer.
  void AddDefaultEstimates();

  // Returns an estimate of network quality at the specified |percentile|.
  // Only the observations later than |begin_timestamp| are taken into account.
  // |percentile| must be between 0 and 100 (both inclusive) with higher
  // percentiles indicating less performant networks. For example, if
  // |percentile| is 90, then the network is expected to be faster than the
  // returned estimate with 0.9 probability. Similarly, network is expected to
  // be slower than the returned estimate with 0.1 probability.
  NetworkQuality GetEstimateInternal(const base::TimeTicks& begin_timestamp,
                                     int percentile) const;
  base::TimeDelta GetRTTEstimateInternal(const base::TimeTicks& begin_timestamp,
                                         int percentile) const;
  int32_t GetDownlinkThroughputKbpsEstimateInternal(
      const base::TimeTicks& begin_timestamp,
      int percentile) const;

  // Returns the current network ID checking by calling the platform APIs.
  // Virtualized for testing.
  virtual NetworkID GetCurrentNetworkID() const;

  // Writes the estimated quality of the current network to the cache.
  void CacheNetworkQualityEstimate();

  // Records the UMA related to RTT.
  void RecordRTTUMA(int32_t estimated_value_msec,
                    int32_t actual_value_msec) const;

  // Determines if the requests to local host can be used in estimating the
  // network quality. Set to true only for tests.
  const bool allow_localhost_requests_;

  // Determines if the responses smaller than |kMinTransferSizeInBytes|
  // or shorter than |kMinTransferSizeInBytes| can be used in estimating the
  // network quality. Set to true only for tests.
  const bool allow_small_responses_;

  // Time when last connection change was observed.
  base::TimeTicks last_connection_change_;

  // ID of the current network.
  NetworkID current_network_id_;

  // Peak network quality (fastest round-trip-time (RTT) and highest
  // downstream throughput) measured since last connectivity change. RTT is
  // measured from time the request is sent until the first byte received.
  // The accuracy is decreased by ignoring these factors:
  // 1) Multiple URLRequests can occur concurrently.
  // 2) Includes server processing time.
  NetworkQuality peak_network_quality_;

  // Cache that stores quality of previously seen networks.
  CachedNetworkQualities cached_network_qualities_;

  // Buffer that holds Kbps observations sorted by timestamp.
  ObservationBuffer kbps_observations_;

  // Buffer that holds RTT (in milliseconds) observations sorted by timestamp.
  ObservationBuffer rtt_msec_observations_;

  // Default network quality observations obtained from the network quality
  // estimator field trial parameters. The observations are indexed by
  // ConnectionType.
  NetworkQuality
      default_observations_[NetworkChangeNotifier::CONNECTION_LAST + 1];

  // Estimated network quality. Updated on mainframe requests.
  NetworkQuality estimated_median_network_quality_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(NetworkQualityEstimator);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_QUALITY_ESTIMATOR_H_
