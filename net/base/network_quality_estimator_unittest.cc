// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_quality_estimator.h"

#include <limits>
#include <map>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_samples.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/external_estimate_provider.h"
#include "net/base/load_flags.h"
#include "net/base/network_change_notifier.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// Helps in setting the current network type and id.
class TestNetworkQualityEstimator : public net::NetworkQualityEstimator {
 public:
  TestNetworkQualityEstimator(
      const std::map<std::string, std::string>& variation_params)
      : NetworkQualityEstimator(scoped_ptr<net::ExternalEstimateProvider>(),
                                variation_params,
                                true,
                                true) {}

  ~TestNetworkQualityEstimator() override {}

  // Overrides the current network type and id.
  // Notifies network quality estimator of change in connection.
  void SimulateNetworkChangeTo(net::NetworkChangeNotifier::ConnectionType type,
                               std::string network_id) {
    current_network_type_ = type;
    current_network_id_ = network_id;
    OnConnectionTypeChanged(type);
  }

  using NetworkQualityEstimator::ReadCachedNetworkQualityEstimate;
  using NetworkQualityEstimator::OnConnectionTypeChanged;

 private:
  // NetworkQualityEstimator implementation that returns the overridden network
  // id (instead of invoking platform APIs).
  NetworkQualityEstimator::NetworkID GetCurrentNetworkID() const override {
    return NetworkQualityEstimator::NetworkID(current_network_type_,
                                              current_network_id_);
  }

  net::NetworkChangeNotifier::ConnectionType current_network_type_;
  std::string current_network_id_;
};

}  // namespace

namespace net {

TEST(NetworkQualityEstimatorTest, TestKbpsRTTUpdates) {
  net::test_server::EmbeddedTestServer embedded_test_server;
  embedded_test_server.ServeFilesFromDirectory(
      base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  ASSERT_TRUE(embedded_test_server.InitializeAndWaitUntilReady());

  base::HistogramTester histogram_tester;
  // Enable requests to local host to be used for network quality estimation.
  std::map<std::string, std::string> variation_params;
  TestNetworkQualityEstimator estimator(variation_params);

  EXPECT_EQ(NetworkQualityEstimator::InvalidRTT(),
            estimator.GetRTTEstimateInternal(base::TimeTicks(), 100));
  EXPECT_EQ(NetworkQualityEstimator::kInvalidThroughput,
            estimator.GetDownlinkThroughputKbpsEstimateInternal(
                base::TimeTicks(), 100));

  TestDelegate test_delegate;
  TestURLRequestContext context(false);

  scoped_ptr<URLRequest> request(
      context.CreateRequest(embedded_test_server.GetURL("/echo.html"),
                            DEFAULT_PRIORITY, &test_delegate));
  request->SetLoadFlags(request->load_flags() | LOAD_MAIN_FRAME);
  request->Start();

  base::RunLoop().Run();

  // Both RTT and downstream throughput should be updated.
  estimator.NotifyHeadersReceived(*request);
  estimator.NotifyRequestCompleted(*request);
  EXPECT_NE(NetworkQualityEstimator::InvalidRTT(),
            estimator.GetRTTEstimateInternal(base::TimeTicks(), 100));
  EXPECT_NE(NetworkQualityEstimator::kInvalidThroughput,
            estimator.GetDownlinkThroughputKbpsEstimateInternal(
                base::TimeTicks(), 100));

  base::TimeDelta rtt = NetworkQualityEstimator::InvalidRTT();
  int32_t kbps = NetworkQualityEstimator::kInvalidThroughput;
  EXPECT_TRUE(estimator.GetRTTEstimate(&rtt));
  EXPECT_TRUE(estimator.GetDownlinkThroughputKbpsEstimate(&kbps));
  EXPECT_NE(NetworkQualityEstimator::InvalidRTT(), rtt);
  EXPECT_NE(NetworkQualityEstimator::kInvalidThroughput, kbps);

  EXPECT_NEAR(
      rtt.InMilliseconds(),
      estimator.GetRTTEstimateInternal(base::TimeTicks(), 100).InMilliseconds(),
      1);

  // Check UMA histograms.
  histogram_tester.ExpectTotalCount("NQE.PeakKbps.Unknown", 0);
  histogram_tester.ExpectTotalCount("NQE.FastestRTT.Unknown", 0);

  histogram_tester.ExpectTotalCount("NQE.RatioEstimatedToActualRTT.Unknown", 0);

  estimator.NotifyHeadersReceived(*request);
  estimator.NotifyRequestCompleted(*request);
  histogram_tester.ExpectTotalCount("NQE.RTTObservations.Unknown", 1);
  estimator.SimulateNetworkChangeTo(
      NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI, "test-1");
  histogram_tester.ExpectTotalCount("NQE.PeakKbps.Unknown", 1);
  histogram_tester.ExpectTotalCount("NQE.FastestRTT.Unknown", 1);

  histogram_tester.ExpectTotalCount("NQE.RatioMedianRTT.WiFi", 0);

  histogram_tester.ExpectTotalCount("NQE.RTT.Percentile0.Unknown", 1);
  histogram_tester.ExpectTotalCount("NQE.RTT.Percentile10.Unknown", 1);
  histogram_tester.ExpectTotalCount("NQE.RTT.Percentile50.Unknown", 1);
  histogram_tester.ExpectTotalCount("NQE.RTT.Percentile90.Unknown", 1);
  histogram_tester.ExpectTotalCount("NQE.RTT.Percentile100.Unknown", 1);

  EXPECT_EQ(NetworkQualityEstimator::InvalidRTT(),
            estimator.GetRTTEstimateInternal(base::TimeTicks(), 100));
  EXPECT_EQ(NetworkQualityEstimator::kInvalidThroughput,
            estimator.GetDownlinkThroughputKbpsEstimateInternal(
                base::TimeTicks(), 100));

  EXPECT_FALSE(estimator.GetRTTEstimate(&rtt));
  EXPECT_FALSE(estimator.GetDownlinkThroughputKbpsEstimate(&kbps));
  EXPECT_EQ(NetworkQualityEstimator::InvalidRTT(), rtt);
  EXPECT_EQ(NetworkQualityEstimator::kInvalidThroughput, kbps);

  estimator.SimulateNetworkChangeTo(
      NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI, std::string());
  histogram_tester.ExpectTotalCount("NQE.PeakKbps.Unknown", 1);
  histogram_tester.ExpectTotalCount("NQE.FastestRTT.Unknown", 1);

  EXPECT_EQ(NetworkQualityEstimator::InvalidRTT(),
            estimator.GetRTTEstimateInternal(base::TimeTicks(), 100));
  EXPECT_EQ(NetworkQualityEstimator::kInvalidThroughput,
            estimator.GetDownlinkThroughputKbpsEstimateInternal(
                base::TimeTicks(), 100));

  EXPECT_FALSE(estimator.GetRTTEstimate(&rtt));
  EXPECT_FALSE(estimator.GetDownlinkThroughputKbpsEstimate(&kbps));
}

TEST(NetworkQualityEstimatorTest, StoreObservations) {
  net::test_server::EmbeddedTestServer embedded_test_server;
  embedded_test_server.ServeFilesFromDirectory(
      base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  ASSERT_TRUE(embedded_test_server.InitializeAndWaitUntilReady());

  std::map<std::string, std::string> variation_params;
  TestNetworkQualityEstimator estimator(variation_params);
  TestDelegate test_delegate;
  TestURLRequestContext context(false);

  // Push 10 more observations than the maximum buffer size.
  for (size_t i = 0; i < estimator.kMaximumObservationsBufferSize + 10U; ++i) {
    scoped_ptr<URLRequest> request(
        context.CreateRequest(embedded_test_server.GetURL("/echo.html"),
                              DEFAULT_PRIORITY, &test_delegate));
    request->Start();
    base::RunLoop().Run();

    estimator.NotifyHeadersReceived(*request);
    estimator.NotifyRequestCompleted(*request);
  }

  EXPECT_EQ(static_cast<size_t>(
                NetworkQualityEstimator::kMaximumObservationsBufferSize),
            estimator.downstream_throughput_kbps_observations_.Size());
  EXPECT_EQ(static_cast<size_t>(
                NetworkQualityEstimator::kMaximumObservationsBufferSize),
            estimator.rtt_msec_observations_.Size());

  // Verify that the stored observations are cleared on network change.
  estimator.SimulateNetworkChangeTo(
      NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI, "test-2");
  EXPECT_EQ(0U, estimator.downstream_throughput_kbps_observations_.Size());
  EXPECT_EQ(0U, estimator.rtt_msec_observations_.Size());

  scoped_ptr<URLRequest> request(
      context.CreateRequest(embedded_test_server.GetURL("/echo.html"),
                            DEFAULT_PRIORITY, &test_delegate));
}

// Verifies that the percentiles are correctly computed. All observations have
// the same timestamp. Kbps percentiles must be in decreasing order. RTT
// percentiles must be in increasing order.
TEST(NetworkQualityEstimatorTest, PercentileSameTimestamps) {
  std::map<std::string, std::string> variation_params;
  TestNetworkQualityEstimator estimator(variation_params);
  base::TimeTicks now = base::TimeTicks::Now();

  // Network quality should be unavailable when no observations are available.
  base::TimeDelta rtt;
  EXPECT_FALSE(estimator.GetRTTEstimate(&rtt));
  int32_t kbps;
  EXPECT_FALSE(estimator.GetDownlinkThroughputKbpsEstimate(&kbps));

  // Insert samples from {1,2,3,..., 100}. First insert odd samples, then even
  // samples. This helps in verifying that the order of samples does not matter.
  for (int i = 1; i <= 99; i += 2) {
    estimator.downstream_throughput_kbps_observations_.AddObservation(
        NetworkQualityEstimator::Observation(i, now));
    estimator.rtt_msec_observations_.AddObservation(
        NetworkQualityEstimator::Observation(i, now));
    EXPECT_TRUE(estimator.GetRTTEstimate(&rtt));
    EXPECT_TRUE(estimator.GetDownlinkThroughputKbpsEstimate(&kbps));
  }

  for (int i = 2; i <= 100; i += 2) {
    estimator.downstream_throughput_kbps_observations_.AddObservation(
        NetworkQualityEstimator::Observation(i, now));
    estimator.rtt_msec_observations_.AddObservation(
        NetworkQualityEstimator::Observation(i, now));
    EXPECT_TRUE(estimator.GetRTTEstimate(&rtt));
    EXPECT_TRUE(estimator.GetDownlinkThroughputKbpsEstimate(&kbps));
  }

  for (int i = 0; i <= 100; ++i) {
    // Checks if the difference between the two integers is less than 1. This is
    // required because computed percentiles may be slightly different from
    // what is expected due to floating point computation errors and integer
    // rounding off errors.
    EXPECT_NEAR(estimator.GetDownlinkThroughputKbpsEstimateInternal(
                    base::TimeTicks(), i),
                100 - i, 1);
    EXPECT_NEAR(
        estimator.GetRTTEstimateInternal(base::TimeTicks(), i).InMilliseconds(),
        i, 1);
  }

  EXPECT_TRUE(estimator.GetRTTEstimate(&rtt));
  EXPECT_TRUE(estimator.GetDownlinkThroughputKbpsEstimate(&kbps));
  // |network_quality| should be equal to the 50 percentile value.
  EXPECT_TRUE(estimator.GetDownlinkThroughputKbpsEstimateInternal(
                  base::TimeTicks(), 50) > 0);
  EXPECT_TRUE(estimator.GetRTTEstimateInternal(base::TimeTicks(), 50) !=
              NetworkQualityEstimator::InvalidRTT());
}

// Verifies that the percentiles are correctly computed. Observations have
// different timestamps with half the observations being very old and the rest
// of them being very recent. Percentiles should factor in recent observations
// much more heavily than older samples. Kbps percentiles must be in decreasing
// order. RTT percentiles must be in increasing order.
TEST(NetworkQualityEstimatorTest, PercentileDifferentTimestamps) {
  std::map<std::string, std::string> variation_params;
  TestNetworkQualityEstimator estimator(variation_params);
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks very_old = base::TimeTicks::UnixEpoch();

  // First 50 samples have very old timestamp.
  for (int i = 1; i <= 50; ++i) {
    estimator.downstream_throughput_kbps_observations_.AddObservation(
        NetworkQualityEstimator::Observation(i, very_old));
    estimator.rtt_msec_observations_.AddObservation(
        NetworkQualityEstimator::Observation(i, very_old));
  }

  // Next 50 (i.e., from 51 to 100) have recent timestamp.
  for (int i = 51; i <= 100; ++i) {
    estimator.downstream_throughput_kbps_observations_.AddObservation(
        NetworkQualityEstimator::Observation(i, now));
    estimator.rtt_msec_observations_.AddObservation(
        NetworkQualityEstimator::Observation(i, now));
  }

  // Older samples have very little weight. So, all percentiles are >= 51
  // (lowest value among recent observations).
  for (int i = 1; i < 100; ++i) {
    // Checks if the difference between the two integers is less than 1. This is
    // required because computed percentiles may be slightly different from
    // what is expected due to floating point computation errors and integer
    // rounding off errors.
    EXPECT_NEAR(estimator.GetDownlinkThroughputKbpsEstimateInternal(
                    base::TimeTicks(), i),
                51 + 0.49 * (100 - i), 1);
    EXPECT_NEAR(
        estimator.GetRTTEstimateInternal(base::TimeTicks(), i).InMilliseconds(),
        51 + 0.49 * i, 1);
  }

  EXPECT_EQ(NetworkQualityEstimator::InvalidRTT(),
            estimator.GetRTTEstimateInternal(
                base::TimeTicks::Now() + base::TimeDelta::FromMinutes(10), 50));
  EXPECT_EQ(NetworkQualityEstimator::kInvalidThroughput,
            estimator.GetDownlinkThroughputKbpsEstimateInternal(
                base::TimeTicks::Now() + base::TimeDelta::FromMinutes(10), 50));
}

// This test notifies NetworkQualityEstimator of received data. Next,
// throughput and RTT percentiles are checked for correctness by doing simple
// verifications.
TEST(NetworkQualityEstimatorTest, ComputedPercentiles) {
  net::test_server::EmbeddedTestServer embedded_test_server;
  embedded_test_server.ServeFilesFromDirectory(
      base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  ASSERT_TRUE(embedded_test_server.InitializeAndWaitUntilReady());

  std::map<std::string, std::string> variation_params;
  TestNetworkQualityEstimator estimator(variation_params);

  EXPECT_EQ(NetworkQualityEstimator::InvalidRTT(),
            estimator.GetRTTEstimateInternal(base::TimeTicks(), 100));
  EXPECT_EQ(NetworkQualityEstimator::kInvalidThroughput,
            estimator.GetDownlinkThroughputKbpsEstimateInternal(
                base::TimeTicks(), 100));

  TestDelegate test_delegate;
  TestURLRequestContext context(false);

  // Number of observations are more than the maximum buffer size.
  for (size_t i = 0; i < estimator.kMaximumObservationsBufferSize + 100U; ++i) {
    scoped_ptr<URLRequest> request(
        context.CreateRequest(embedded_test_server.GetURL("/echo.html"),
                              DEFAULT_PRIORITY, &test_delegate));
    request->Start();
    base::RunLoop().Run();

    // Use different number of bytes to create variation.
    estimator.NotifyHeadersReceived(*request);
    estimator.NotifyRequestCompleted(*request);
  }

  // Verify the percentiles through simple tests.
  for (int i = 0; i <= 100; ++i) {
    EXPECT_GT(estimator.GetDownlinkThroughputKbpsEstimateInternal(
                  base::TimeTicks(), i),
              0);
    EXPECT_LT(estimator.GetRTTEstimateInternal(base::TimeTicks(), i),
              base::TimeDelta::Max());

    if (i != 0) {
      // Throughput percentiles are in decreasing order.
      EXPECT_LE(estimator.GetDownlinkThroughputKbpsEstimateInternal(
                    base::TimeTicks(), i),
                estimator.GetDownlinkThroughputKbpsEstimateInternal(
                    base::TimeTicks(), i - 1));

      // RTT percentiles are in increasing order.
      EXPECT_GE(estimator.GetRTTEstimateInternal(base::TimeTicks(), i),
                estimator.GetRTTEstimateInternal(base::TimeTicks(), i - 1));
    }
  }
}

TEST(NetworkQualityEstimatorTest, ObtainOperatingParams) {
  std::map<std::string, std::string> variation_params;
  variation_params["Unknown.DefaultMedianKbps"] = "100";
  variation_params["WiFi.DefaultMedianKbps"] = "200";
  variation_params["2G.DefaultMedianKbps"] = "300";

  variation_params["Unknown.DefaultMedianRTTMsec"] = "1000";
  variation_params["WiFi.DefaultMedianRTTMsec"] = "2000";
  // Negative variation value should not be used.
  variation_params["2G.DefaultMedianRTTMsec"] = "-5";

  TestNetworkQualityEstimator estimator(variation_params);
  EXPECT_EQ(1U, estimator.downstream_throughput_kbps_observations_.Size());
  EXPECT_EQ(1U, estimator.rtt_msec_observations_.Size());

  base::TimeDelta rtt;
  EXPECT_TRUE(estimator.GetRTTEstimate(&rtt));
  int32_t kbps;
  EXPECT_TRUE(estimator.GetDownlinkThroughputKbpsEstimate(&kbps));

  EXPECT_EQ(100, kbps);
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1000), rtt);
  auto it =
      estimator.downstream_throughput_kbps_observations_.observations_.begin();
  EXPECT_EQ(100, (*it).value);
  it = estimator.rtt_msec_observations_.observations_.begin();
  EXPECT_EQ(1000, (*it).value);

  // Simulate network change to Wi-Fi.
  estimator.SimulateNetworkChangeTo(
      NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI, "test-1");
  EXPECT_EQ(1U, estimator.downstream_throughput_kbps_observations_.Size());
  EXPECT_EQ(1U, estimator.rtt_msec_observations_.Size());

  EXPECT_TRUE(estimator.GetRTTEstimate(&rtt));
  EXPECT_TRUE(estimator.GetDownlinkThroughputKbpsEstimate(&kbps));
  EXPECT_EQ(200, kbps);
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2000), rtt);

  it = estimator.downstream_throughput_kbps_observations_.observations_.begin();
  EXPECT_EQ(200, (*it).value);
  it = estimator.rtt_msec_observations_.observations_.begin();
  EXPECT_EQ(2000, (*it).value);

  // Peak network quality should not be affected by the network quality
  // estimator field trial.
  EXPECT_EQ(NetworkQualityEstimator::InvalidRTT(),
            estimator.peak_network_quality_.rtt());
  EXPECT_EQ(NetworkQualityEstimator::kInvalidThroughput,
            estimator.peak_network_quality_.downstream_throughput_kbps());

  // Simulate network change to 2G. Only the Kbps default estimate should be
  // available.
  estimator.SimulateNetworkChangeTo(
      NetworkChangeNotifier::ConnectionType::CONNECTION_2G, "test-2");
  EXPECT_EQ(1U, estimator.downstream_throughput_kbps_observations_.Size());
  EXPECT_EQ(0U, estimator.rtt_msec_observations_.Size());

  EXPECT_FALSE(estimator.GetRTTEstimate(&rtt));
  EXPECT_TRUE(estimator.GetDownlinkThroughputKbpsEstimate(&kbps));

  it = estimator.downstream_throughput_kbps_observations_.observations_.begin();
  EXPECT_EQ(300, (*it).value);

  // Simulate network change to 3G. Default estimates should be unavailable.
  estimator.SimulateNetworkChangeTo(
      NetworkChangeNotifier::ConnectionType::CONNECTION_3G, "test-3");

  EXPECT_FALSE(estimator.GetRTTEstimate(&rtt));
  EXPECT_FALSE(estimator.GetDownlinkThroughputKbpsEstimate(&kbps));
  EXPECT_EQ(0U, estimator.downstream_throughput_kbps_observations_.Size());
  EXPECT_EQ(0U, estimator.rtt_msec_observations_.Size());
}

TEST(NetworkQualityEstimatorTest, HalfLifeParam) {
  // Verifies if |weight_multiplier_per_second_| is set to correct value for
  // various values of half life parameter.
  std::map<std::string, std::string> variation_params;
  {
    // Half life parameter is not set. Default value of
    // |weight_multiplier_per_second_| should be used.
    TestNetworkQualityEstimator estimator(variation_params);
    EXPECT_NEAR(0.988, estimator.downstream_throughput_kbps_observations_
                           .weight_multiplier_per_second_,
                0.001);
  }

  variation_params["HalfLifeSeconds"] = "-100";
  {
    // Half life parameter is set to a negative value.  Default value of
    // |weight_multiplier_per_second_| should be used.
    TestNetworkQualityEstimator estimator(variation_params);
    EXPECT_NEAR(0.988, estimator.downstream_throughput_kbps_observations_
                           .weight_multiplier_per_second_,
                0.001);
  }

  variation_params["HalfLifeSeconds"] = "0";
  {
    // Half life parameter is set to zero.  Default value of
    // |weight_multiplier_per_second_| should be used.
    TestNetworkQualityEstimator estimator(variation_params);
    EXPECT_NEAR(0.988, estimator.downstream_throughput_kbps_observations_
                           .weight_multiplier_per_second_,
                0.001);
  }

  variation_params["HalfLifeSeconds"] = "10";
  {
    // Half life parameter is set to a valid value.
    TestNetworkQualityEstimator estimator(variation_params);
    EXPECT_NEAR(0.933, estimator.downstream_throughput_kbps_observations_
                           .weight_multiplier_per_second_,
                0.001);
  }
}

// Test if the network estimates are cached when network change notification
// is invoked.
TEST(NetworkQualityEstimatorTest, TestCaching) {
  std::map<std::string, std::string> variation_params;
  TestNetworkQualityEstimator estimator(variation_params);
  size_t expected_cache_size = 0;
  EXPECT_EQ(expected_cache_size, estimator.cached_network_qualities_.size());

  // Cache entry will not be added for (NONE, "").
  estimator.downstream_throughput_kbps_observations_.AddObservation(
      NetworkQualityEstimator::Observation(1, base::TimeTicks::Now()));
  estimator.rtt_msec_observations_.AddObservation(
      NetworkQualityEstimator::Observation(1000, base::TimeTicks::Now()));
  estimator.SimulateNetworkChangeTo(
      NetworkChangeNotifier::ConnectionType::CONNECTION_2G, "test-1");
  EXPECT_EQ(expected_cache_size, estimator.cached_network_qualities_.size());

  // Entry will be added for (2G, "test1").
  // Also, set the network quality for (2G, "test1") so that it is stored in
  // the cache.
  estimator.downstream_throughput_kbps_observations_.AddObservation(
      NetworkQualityEstimator::Observation(1, base::TimeTicks::Now()));
  estimator.rtt_msec_observations_.AddObservation(
      NetworkQualityEstimator::Observation(1000, base::TimeTicks::Now()));

  estimator.SimulateNetworkChangeTo(
      NetworkChangeNotifier::ConnectionType::CONNECTION_3G, "test-1");
  EXPECT_EQ(++expected_cache_size, estimator.cached_network_qualities_.size());

  // Entry will be added for (3G, "test1").
  // Also, set the network quality for (3G, "test1") so that it is stored in
  // the cache.
  estimator.downstream_throughput_kbps_observations_.AddObservation(
      NetworkQualityEstimator::Observation(2, base::TimeTicks::Now()));
  estimator.rtt_msec_observations_.AddObservation(
      NetworkQualityEstimator::Observation(500, base::TimeTicks::Now()));
  estimator.SimulateNetworkChangeTo(
      NetworkChangeNotifier::ConnectionType::CONNECTION_3G, "test-2");
  EXPECT_EQ(++expected_cache_size, estimator.cached_network_qualities_.size());

  // Entry will not be added for (3G, "test2").
  estimator.SimulateNetworkChangeTo(
      NetworkChangeNotifier::ConnectionType::CONNECTION_2G, "test-1");
  EXPECT_EQ(expected_cache_size, estimator.cached_network_qualities_.size());

  // Read the network quality for (2G, "test-1").
  EXPECT_TRUE(estimator.ReadCachedNetworkQualityEstimate());

  base::TimeDelta rtt;
  int32_t kbps;
  EXPECT_TRUE(estimator.GetRTTEstimate(&rtt));
  EXPECT_TRUE(estimator.GetDownlinkThroughputKbpsEstimate(&kbps));
  EXPECT_EQ(1, kbps);
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1000), rtt);
  // No new entry should be added for (2G, "test-1") since it already exists
  // in the cache.
  estimator.SimulateNetworkChangeTo(
      NetworkChangeNotifier::ConnectionType::CONNECTION_3G, "test-1");
  EXPECT_EQ(expected_cache_size, estimator.cached_network_qualities_.size());

  // Read the network quality for (3G, "test-1").
  EXPECT_TRUE(estimator.ReadCachedNetworkQualityEstimate());
  EXPECT_TRUE(estimator.GetRTTEstimate(&rtt));
  EXPECT_TRUE(estimator.GetDownlinkThroughputKbpsEstimate(&kbps));
  EXPECT_EQ(2, kbps);
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(500), rtt);
  // No new entry should be added for (3G, "test1") since it already exists
  // in the cache.
  estimator.SimulateNetworkChangeTo(
      NetworkChangeNotifier::ConnectionType::CONNECTION_3G, "test-2");
  EXPECT_EQ(expected_cache_size, estimator.cached_network_qualities_.size());

  // Reading quality of (3G, "test-2") should return false.
  EXPECT_FALSE(estimator.ReadCachedNetworkQualityEstimate());

  // Reading quality of (2G, "test-3") should return false.
  estimator.SimulateNetworkChangeTo(
      NetworkChangeNotifier::ConnectionType::CONNECTION_2G, "test-3");
  EXPECT_FALSE(estimator.ReadCachedNetworkQualityEstimate());
}

// Tests if the cache size remains bounded. Also, ensure that the cache is
// LRU.
TEST(NetworkQualityEstimatorTest, TestLRUCacheMaximumSize) {
  std::map<std::string, std::string> variation_params;
  TestNetworkQualityEstimator estimator(variation_params);
  estimator.SimulateNetworkChangeTo(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI,
      std::string());
  EXPECT_EQ(0U, estimator.cached_network_qualities_.size());

  // Add 100 more networks than the maximum size of the cache.
  size_t network_count =
      NetworkQualityEstimator::kMaximumNetworkQualityCacheSize + 100;

  base::TimeTicks update_time_of_network_100;
  for (size_t i = 0; i < network_count; ++i) {
    estimator.downstream_throughput_kbps_observations_.AddObservation(
        NetworkQualityEstimator::Observation(2, base::TimeTicks::Now()));
    estimator.rtt_msec_observations_.AddObservation(
        NetworkQualityEstimator::Observation(500, base::TimeTicks::Now()));

    if (i == 100)
      update_time_of_network_100 = base::TimeTicks::Now();

    estimator.SimulateNetworkChangeTo(
        net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI,
        base::IntToString(i));
    if (i < NetworkQualityEstimator::kMaximumNetworkQualityCacheSize)
      EXPECT_EQ(i, estimator.cached_network_qualities_.size());
    EXPECT_LE(estimator.cached_network_qualities_.size(),
              static_cast<size_t>(
                  NetworkQualityEstimator::kMaximumNetworkQualityCacheSize));
  }
  // One more call so that the last network is also written to cache.
  estimator.downstream_throughput_kbps_observations_.AddObservation(
      NetworkQualityEstimator::Observation(2, base::TimeTicks::Now()));
  estimator.rtt_msec_observations_.AddObservation(
      NetworkQualityEstimator::Observation(500, base::TimeTicks::Now()));
  estimator.SimulateNetworkChangeTo(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI,
      base::IntToString(network_count - 1));
  EXPECT_EQ(static_cast<size_t>(
                NetworkQualityEstimator::kMaximumNetworkQualityCacheSize),
            estimator.cached_network_qualities_.size());

  // Test that the cache is LRU by examining its contents. Networks in cache
  // must all be newer than the 100th network.
  for (NetworkQualityEstimator::CachedNetworkQualities::iterator it =
           estimator.cached_network_qualities_.begin();
       it != estimator.cached_network_qualities_.end(); ++it) {
    EXPECT_GE((it->second).last_update_time_, update_time_of_network_100);
  }
}

TEST(NetworkQualityEstimatorTest, TestGetMedianRTTSince) {
  std::map<std::string, std::string> variation_params;
  TestNetworkQualityEstimator estimator(variation_params);
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks old =
      base::TimeTicks::Now() - base::TimeDelta::FromMilliseconds(1);

  // First sample has very old timestamp.
  estimator.downstream_throughput_kbps_observations_.AddObservation(
      NetworkQualityEstimator::Observation(1, old));
  estimator.rtt_msec_observations_.AddObservation(
      NetworkQualityEstimator::Observation(1, old));

  estimator.downstream_throughput_kbps_observations_.AddObservation(
      NetworkQualityEstimator::Observation(100, now));
  estimator.rtt_msec_observations_.AddObservation(
      NetworkQualityEstimator::Observation(100, now));

  base::TimeDelta rtt;
  EXPECT_FALSE(estimator.GetMedianRTTSince(
      now + base::TimeDelta::FromSeconds(10), &rtt));
  EXPECT_TRUE(estimator.GetMedianRTTSince(now, &rtt));
  EXPECT_EQ(100, rtt.InMilliseconds());
}

}  // namespace net
