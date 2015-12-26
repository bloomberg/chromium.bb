// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_quality_estimator.h"

#include <stdint.h>
#include <limits>
#include <map>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
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
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// Helps in setting the current network type and id.
class TestNetworkQualityEstimator : public net::NetworkQualityEstimator {
 public:
  TestNetworkQualityEstimator(
      const std::map<std::string, std::string>& variation_params,
      scoped_ptr<net::ExternalEstimateProvider> external_estimate_provider)
      : NetworkQualityEstimator(std::move(external_estimate_provider),
                                variation_params,
                                true,
                                true) {
    // Set up embedded test server.
    embedded_test_server_.ServeFilesFromDirectory(
        base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
    EXPECT_TRUE(embedded_test_server_.Start());
    embedded_test_server_.RegisterRequestHandler(base::Bind(
        &TestNetworkQualityEstimator::HandleRequest, base::Unretained(this)));
  }

  explicit TestNetworkQualityEstimator(
      const std::map<std::string, std::string>& variation_params)
      : TestNetworkQualityEstimator(
            variation_params,
            scoped_ptr<net::ExternalEstimateProvider>()) {}

  ~TestNetworkQualityEstimator() override {}

  // Overrides the current network type and id.
  // Notifies network quality estimator of change in connection.
  void SimulateNetworkChangeTo(net::NetworkChangeNotifier::ConnectionType type,
                               std::string network_id) {
    current_network_type_ = type;
    current_network_id_ = network_id;
    OnConnectionTypeChanged(type);
  }

  // Called by embedded server when a HTTP request is received.
  scoped_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    scoped_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse());
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("hello");
    http_response->set_content_type("text/plain");
    return std::move(http_response);
  }

  // Returns a GURL hosted at embedded test server.
  const GURL GetEchoURL() const {
    return embedded_test_server_.GetURL("/echo.html");
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

  // Embedded server used for testing.
  net::EmbeddedTestServer embedded_test_server_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkQualityEstimator);
};

class TestRTTObserver : public net::NetworkQualityEstimator::RTTObserver {
 public:
  struct Observation {
    Observation(int32_t ms,
                const base::TimeTicks& ts,
                net::NetworkQualityEstimator::ObservationSource src)
        : rtt_ms(ms), timestamp(ts), source(src) {}
    int32_t rtt_ms;
    base::TimeTicks timestamp;
    net::NetworkQualityEstimator::ObservationSource source;
  };

  std::vector<Observation>& observations() { return observations_; }

  // RttObserver implementation:
  void OnRTTObservation(
      int32_t rtt_ms,
      const base::TimeTicks& timestamp,
      net::NetworkQualityEstimator::ObservationSource source) override {
    observations_.push_back(Observation(rtt_ms, timestamp, source));
  }

 private:
  std::vector<Observation> observations_;
};

class TestThroughputObserver
    : public net::NetworkQualityEstimator::ThroughputObserver {
 public:
  struct Observation {
    Observation(int32_t kbps,
                const base::TimeTicks& ts,
                net::NetworkQualityEstimator::ObservationSource src)
        : throughput_kbps(kbps), timestamp(ts), source(src) {}
    int32_t throughput_kbps;
    base::TimeTicks timestamp;
    net::NetworkQualityEstimator::ObservationSource source;
  };

  std::vector<Observation>& observations() { return observations_; }

  // ThroughputObserver implementation:
  void OnThroughputObservation(
      int32_t throughput_kbps,
      const base::TimeTicks& timestamp,
      net::NetworkQualityEstimator::ObservationSource source) override {
    observations_.push_back(Observation(throughput_kbps, timestamp, source));
  }

 private:
  std::vector<Observation> observations_;
};

}  // namespace

namespace net {

TEST(NetworkQualityEstimatorTest, TestKbpsRTTUpdates) {
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
  TestURLRequestContext context(true);
  context.set_network_quality_estimator(&estimator);
  context.Init();

  scoped_ptr<URLRequest> request(context.CreateRequest(
      estimator.GetEchoURL(), DEFAULT_PRIORITY, &test_delegate));
  request->SetLoadFlags(request->load_flags() | LOAD_MAIN_FRAME);
  request->Start();
  base::RunLoop().Run();

  // Both RTT and downstream throughput should be updated.
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

  scoped_ptr<URLRequest> request2(context.CreateRequest(
      estimator.GetEchoURL(), DEFAULT_PRIORITY, &test_delegate));
  request2->SetLoadFlags(request2->load_flags() | LOAD_MAIN_FRAME);
  request2->Start();
  base::RunLoop().Run();

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
  std::map<std::string, std::string> variation_params;
  TestNetworkQualityEstimator estimator(variation_params);

  TestDelegate test_delegate;
  TestURLRequestContext context(true);
  context.set_network_quality_estimator(&estimator);
  context.Init();

  // Push 10 more observations than the maximum buffer size.
  for (size_t i = 0; i < estimator.kMaximumObservationsBufferSize + 10U; ++i) {
    scoped_ptr<URLRequest> request(context.CreateRequest(
        estimator.GetEchoURL(), DEFAULT_PRIORITY, &test_delegate));
    request->Start();
    base::RunLoop().Run();
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
        NetworkQualityEstimator::Observation(
            i, now, NetworkQualityEstimator::URL_REQUEST));
    estimator.rtt_msec_observations_.AddObservation(
        NetworkQualityEstimator::Observation(
            i, now, NetworkQualityEstimator::URL_REQUEST));
    EXPECT_TRUE(estimator.GetRTTEstimate(&rtt));
    EXPECT_TRUE(estimator.GetDownlinkThroughputKbpsEstimate(&kbps));
  }

  for (int i = 2; i <= 100; i += 2) {
    estimator.downstream_throughput_kbps_observations_.AddObservation(
        NetworkQualityEstimator::Observation(
            i, now, NetworkQualityEstimator::URL_REQUEST));
    estimator.rtt_msec_observations_.AddObservation(
        NetworkQualityEstimator::Observation(
            i, now, NetworkQualityEstimator::URL_REQUEST));
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
        NetworkQualityEstimator::Observation(
            i, very_old, NetworkQualityEstimator::URL_REQUEST));
    estimator.rtt_msec_observations_.AddObservation(
        NetworkQualityEstimator::Observation(
            i, very_old, NetworkQualityEstimator::URL_REQUEST));
  }

  // Next 50 (i.e., from 51 to 100) have recent timestamp.
  for (int i = 51; i <= 100; ++i) {
    estimator.downstream_throughput_kbps_observations_.AddObservation(
        NetworkQualityEstimator::Observation(
            i, now, NetworkQualityEstimator::URL_REQUEST));
    estimator.rtt_msec_observations_.AddObservation(
        NetworkQualityEstimator::Observation(
            i, now, NetworkQualityEstimator::URL_REQUEST));
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
  std::map<std::string, std::string> variation_params;
  TestNetworkQualityEstimator estimator(variation_params);

  EXPECT_EQ(NetworkQualityEstimator::InvalidRTT(),
            estimator.GetRTTEstimateInternal(base::TimeTicks(), 100));
  EXPECT_EQ(NetworkQualityEstimator::kInvalidThroughput,
            estimator.GetDownlinkThroughputKbpsEstimateInternal(
                base::TimeTicks(), 100));

  TestDelegate test_delegate;
  TestURLRequestContext context(true);
  context.set_network_quality_estimator(&estimator);
  context.Init();

  // Number of observations are more than the maximum buffer size.
  for (size_t i = 0; i < estimator.kMaximumObservationsBufferSize + 100U; ++i) {
    scoped_ptr<URLRequest> request(context.CreateRequest(
        estimator.GetEchoURL(), DEFAULT_PRIORITY, &test_delegate));
    request->Start();
    base::RunLoop().Run();
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
      NetworkQualityEstimator::Observation(
          1, base::TimeTicks::Now(), NetworkQualityEstimator::URL_REQUEST));
  estimator.rtt_msec_observations_.AddObservation(
      NetworkQualityEstimator::Observation(
          1000, base::TimeTicks::Now(), NetworkQualityEstimator::URL_REQUEST));
  estimator.SimulateNetworkChangeTo(
      NetworkChangeNotifier::ConnectionType::CONNECTION_2G, "test-1");
  EXPECT_EQ(expected_cache_size, estimator.cached_network_qualities_.size());

  // Entry will be added for (2G, "test1").
  // Also, set the network quality for (2G, "test1") so that it is stored in
  // the cache.
  estimator.downstream_throughput_kbps_observations_.AddObservation(
      NetworkQualityEstimator::Observation(
          1, base::TimeTicks::Now(), NetworkQualityEstimator::URL_REQUEST));
  estimator.rtt_msec_observations_.AddObservation(
      NetworkQualityEstimator::Observation(
          1000, base::TimeTicks::Now(), NetworkQualityEstimator::URL_REQUEST));

  estimator.SimulateNetworkChangeTo(
      NetworkChangeNotifier::ConnectionType::CONNECTION_3G, "test-1");
  ++expected_cache_size;
  EXPECT_EQ(expected_cache_size, estimator.cached_network_qualities_.size());

  // Entry will be added for (3G, "test1").
  // Also, set the network quality for (3G, "test1") so that it is stored in
  // the cache.
  estimator.downstream_throughput_kbps_observations_.AddObservation(
      NetworkQualityEstimator::Observation(
          2, base::TimeTicks::Now(), NetworkQualityEstimator::URL_REQUEST));
  estimator.rtt_msec_observations_.AddObservation(
      NetworkQualityEstimator::Observation(
          500, base::TimeTicks::Now(), NetworkQualityEstimator::URL_REQUEST));
  estimator.SimulateNetworkChangeTo(
      NetworkChangeNotifier::ConnectionType::CONNECTION_3G, "test-2");
  ++expected_cache_size;
  EXPECT_EQ(expected_cache_size, estimator.cached_network_qualities_.size());

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
        NetworkQualityEstimator::Observation(
            2, base::TimeTicks::Now(), NetworkQualityEstimator::URL_REQUEST));
    estimator.rtt_msec_observations_.AddObservation(
        NetworkQualityEstimator::Observation(
            500, base::TimeTicks::Now(), NetworkQualityEstimator::URL_REQUEST));

    if (i == 100)
      update_time_of_network_100 = base::TimeTicks::Now();

    estimator.SimulateNetworkChangeTo(
        net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI,
        base::SizeTToString(i));
    if (i < NetworkQualityEstimator::kMaximumNetworkQualityCacheSize)
      EXPECT_EQ(i, estimator.cached_network_qualities_.size());
    EXPECT_LE(estimator.cached_network_qualities_.size(),
              static_cast<size_t>(
                  NetworkQualityEstimator::kMaximumNetworkQualityCacheSize));
  }
  // One more call so that the last network is also written to cache.
  estimator.downstream_throughput_kbps_observations_.AddObservation(
      NetworkQualityEstimator::Observation(
          2, base::TimeTicks::Now(), NetworkQualityEstimator::URL_REQUEST));
  estimator.rtt_msec_observations_.AddObservation(
      NetworkQualityEstimator::Observation(
          500, base::TimeTicks::Now(), NetworkQualityEstimator::URL_REQUEST));
  estimator.SimulateNetworkChangeTo(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI,
      base::SizeTToString(network_count - 1));
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
  base::TimeTicks old = now - base::TimeDelta::FromMilliseconds(1);
  ASSERT_NE(old, now);

  // First sample has very old timestamp.
  estimator.downstream_throughput_kbps_observations_.AddObservation(
      NetworkQualityEstimator::Observation(
          1, old, NetworkQualityEstimator::URL_REQUEST));
  estimator.rtt_msec_observations_.AddObservation(
      NetworkQualityEstimator::Observation(
          1, old, NetworkQualityEstimator::URL_REQUEST));

  estimator.downstream_throughput_kbps_observations_.AddObservation(
      NetworkQualityEstimator::Observation(
          100, now, NetworkQualityEstimator::URL_REQUEST));
  estimator.rtt_msec_observations_.AddObservation(
      NetworkQualityEstimator::Observation(
          100, now, NetworkQualityEstimator::URL_REQUEST));

  base::TimeDelta rtt;
  EXPECT_FALSE(estimator.GetRecentMedianRTT(
      now + base::TimeDelta::FromSeconds(10), &rtt));
  EXPECT_TRUE(estimator.GetRecentMedianRTT(now, &rtt));
  EXPECT_EQ(100, rtt.InMilliseconds());

  int32_t downstream_throughput_kbps;
  EXPECT_FALSE(estimator.GetRecentMedianDownlinkThroughputKbps(
      now + base::TimeDelta::FromSeconds(10), &downstream_throughput_kbps));
  EXPECT_TRUE(estimator.GetRecentMedianDownlinkThroughputKbps(
      now, &downstream_throughput_kbps));
  EXPECT_EQ(100, downstream_throughput_kbps);
}

// An external estimate provider that does not have a valid RTT or throughput
// estimate.
class InvalidExternalEstimateProvider : public ExternalEstimateProvider {
 public:
  InvalidExternalEstimateProvider() : get_rtt_count_(0) {}
  ~InvalidExternalEstimateProvider() override {}

  // ExternalEstimateProvider implementation:
  bool GetRTT(base::TimeDelta* rtt) const override {
    DCHECK(rtt);
    get_rtt_count_++;
    return false;
  }

  // ExternalEstimateProvider implementation:
  bool GetDownstreamThroughputKbps(
      int32_t* downstream_throughput_kbps) const override {
    DCHECK(downstream_throughput_kbps);
    return false;
  }

  // ExternalEstimateProvider implementation:
  bool GetUpstreamThroughputKbps(
      int32_t* upstream_throughput_kbps) const override {
    // NetworkQualityEstimator does not support upstream throughput.
    ADD_FAILURE();
    return false;
  }

  // ExternalEstimateProvider implementation:
  bool GetTimeSinceLastUpdate(
      base::TimeDelta* time_since_last_update) const override {
    *time_since_last_update = base::TimeDelta::FromMilliseconds(1);
    return true;
  }

  // ExternalEstimateProvider implementation:
  void SetUpdatedEstimateDelegate(UpdatedEstimateDelegate* delegate) override {}

  // ExternalEstimateProvider implementation:
  void Update() const override {}

  size_t get_rtt_count() const { return get_rtt_count_; }

 private:
  // Keeps track of number of times different functions were called.
  mutable size_t get_rtt_count_;

  DISALLOW_COPY_AND_ASSIGN(InvalidExternalEstimateProvider);
};

// Tests if the RTT value from external estimate provider is discarded if the
// external estimate provider is invalid.
TEST(NetworkQualityEstimatorTest, InvalidExternalEstimateProvider) {
  InvalidExternalEstimateProvider* invalid_external_estimate_provider =
      new InvalidExternalEstimateProvider();
  scoped_ptr<ExternalEstimateProvider> external_estimate_provider(
      invalid_external_estimate_provider);

  TestNetworkQualityEstimator estimator(std::map<std::string, std::string>(),
                                        std::move(external_estimate_provider));

  base::TimeDelta rtt;
  int32_t kbps;
  EXPECT_EQ(1U, invalid_external_estimate_provider->get_rtt_count());
  EXPECT_FALSE(estimator.GetRTTEstimate(&rtt));
  EXPECT_FALSE(estimator.GetDownlinkThroughputKbpsEstimate(&kbps));
}

class TestExternalEstimateProvider : public ExternalEstimateProvider {
 public:
  TestExternalEstimateProvider(base::TimeDelta rtt,
                               int32_t downstream_throughput_kbps)
      : rtt_(rtt),
        downstream_throughput_kbps_(downstream_throughput_kbps),
        time_since_last_update_(base::TimeDelta::FromSeconds(1)),
        get_time_since_last_update_count_(0),
        get_rtt_count_(0),
        get_downstream_throughput_kbps_count_(0),
        update_count_(0) {}
  ~TestExternalEstimateProvider() override {}

  // ExternalEstimateProvider implementation:
  bool GetRTT(base::TimeDelta* rtt) const override {
    *rtt = rtt_;
    get_rtt_count_++;
    return true;
  }

  // ExternalEstimateProvider implementation:
  bool GetDownstreamThroughputKbps(
      int32_t* downstream_throughput_kbps) const override {
    *downstream_throughput_kbps = downstream_throughput_kbps_;
    get_downstream_throughput_kbps_count_++;
    return true;
  }

  // ExternalEstimateProvider implementation:
  bool GetUpstreamThroughputKbps(
      int32_t* upstream_throughput_kbps) const override {
    // NetworkQualityEstimator does not support upstream throughput.
    ADD_FAILURE();
    return false;
  }

  // ExternalEstimateProvider implementation:
  bool GetTimeSinceLastUpdate(
      base::TimeDelta* time_since_last_update) const override {
    *time_since_last_update = time_since_last_update_;
    get_time_since_last_update_count_++;
    return true;
  }

  // ExternalEstimateProvider implementation:
  void SetUpdatedEstimateDelegate(UpdatedEstimateDelegate* delegate) override {}

  // ExternalEstimateProvider implementation:
  void Update() const override { update_count_++; }

  void set_time_since_last_update(base::TimeDelta time_since_last_update) {
    time_since_last_update_ = time_since_last_update;
  }

  size_t get_time_since_last_update_count() const {
    return get_time_since_last_update_count_;
  }
  size_t get_rtt_count() const { return get_rtt_count_; }
  size_t get_downstream_throughput_kbps_count() const {
    return get_downstream_throughput_kbps_count_;
  }
  size_t update_count() const { return update_count_; }

 private:
  // RTT and downstream throughput estimates.
  const base::TimeDelta rtt_;
  const int32_t downstream_throughput_kbps_;

  base::TimeDelta time_since_last_update_;

  // Keeps track of number of times different functions were called.
  mutable size_t get_time_since_last_update_count_;
  mutable size_t get_rtt_count_;
  mutable size_t get_downstream_throughput_kbps_count_;
  mutable size_t update_count_;

  DISALLOW_COPY_AND_ASSIGN(TestExternalEstimateProvider);
};

// Tests if the external estimate provider is called in the constructor and
// on network change notification.
TEST(NetworkQualityEstimatorTest, TestExternalEstimateProvider) {
  TestExternalEstimateProvider* test_external_estimate_provider =
      new TestExternalEstimateProvider(base::TimeDelta::FromMilliseconds(1),
                                       100);
  scoped_ptr<ExternalEstimateProvider> external_estimate_provider(
      test_external_estimate_provider);
  std::map<std::string, std::string> variation_params;
  TestNetworkQualityEstimator estimator(variation_params,
                                        std::move(external_estimate_provider));

  base::TimeDelta rtt;
  int32_t kbps;
  EXPECT_TRUE(estimator.GetRTTEstimate(&rtt));
  EXPECT_TRUE(estimator.GetDownlinkThroughputKbpsEstimate(&kbps));

  EXPECT_EQ(
      1U, test_external_estimate_provider->get_time_since_last_update_count());
  EXPECT_EQ(1U, test_external_estimate_provider->get_rtt_count());
  EXPECT_EQ(
      1U,
      test_external_estimate_provider->get_downstream_throughput_kbps_count());

  // Change network type to WiFi. Number of queries to External estimate
  // provider must increment.
  estimator.SimulateNetworkChangeTo(
      NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI, "test-1");
  EXPECT_TRUE(estimator.GetRTTEstimate(&rtt));
  EXPECT_TRUE(estimator.GetDownlinkThroughputKbpsEstimate(&kbps));
  EXPECT_EQ(
      2U, test_external_estimate_provider->get_time_since_last_update_count());
  EXPECT_EQ(2U, test_external_estimate_provider->get_rtt_count());
  EXPECT_EQ(
      2U,
      test_external_estimate_provider->get_downstream_throughput_kbps_count());

  // Change network type to 2G. Number of queries to External estimate provider
  // must increment.
  estimator.SimulateNetworkChangeTo(
      NetworkChangeNotifier::ConnectionType::CONNECTION_2G, "test-1");
  EXPECT_EQ(
      3U, test_external_estimate_provider->get_time_since_last_update_count());
  EXPECT_EQ(3U, test_external_estimate_provider->get_rtt_count());
  EXPECT_EQ(
      3U,
      test_external_estimate_provider->get_downstream_throughput_kbps_count());

  // Set the external estimate as old. Network Quality estimator should request
  // an update on connection type change.
  EXPECT_EQ(0U, test_external_estimate_provider->update_count());
  test_external_estimate_provider->set_time_since_last_update(
      base::TimeDelta::Max());

  estimator.SimulateNetworkChangeTo(
      NetworkChangeNotifier::ConnectionType::CONNECTION_2G, "test-2");
  EXPECT_EQ(
      4U, test_external_estimate_provider->get_time_since_last_update_count());
  EXPECT_EQ(3U, test_external_estimate_provider->get_rtt_count());
  EXPECT_EQ(
      3U,
      test_external_estimate_provider->get_downstream_throughput_kbps_count());
  EXPECT_EQ(1U, test_external_estimate_provider->update_count());

  // Estimates are unavailable because external estimate provider never
  // notifies network quality estimator of the updated estimates.
  EXPECT_FALSE(estimator.GetRTTEstimate(&rtt));
  EXPECT_FALSE(estimator.GetDownlinkThroughputKbpsEstimate(&kbps));
}

// Tests if the estimate from the external estimate provider is merged with the
// observations collected from the HTTP requests.
TEST(NetworkQualityEstimatorTest, TestExternalEstimateProviderMergeEstimates) {
  const base::TimeDelta external_estimate_provider_rtt =
      base::TimeDelta::FromMilliseconds(1);
  const int32_t external_estimate_provider_downstream_throughput = 100;
  TestExternalEstimateProvider* test_external_estimate_provider =
      new TestExternalEstimateProvider(
          external_estimate_provider_rtt,
          external_estimate_provider_downstream_throughput);
  scoped_ptr<ExternalEstimateProvider> external_estimate_provider(
      test_external_estimate_provider);

  std::map<std::string, std::string> variation_params;
  TestNetworkQualityEstimator estimator(variation_params,
                                        std::move(external_estimate_provider));

  base::TimeDelta rtt;
  // Estimate provided by network quality estimator should match the estimate
  // provided by external estimate provider.
  EXPECT_TRUE(estimator.GetRTTEstimate(&rtt));
  EXPECT_EQ(external_estimate_provider_rtt, rtt);

  int32_t kbps;
  EXPECT_TRUE(estimator.GetDownlinkThroughputKbpsEstimate(&kbps));
  EXPECT_EQ(external_estimate_provider_downstream_throughput, kbps);

  EXPECT_EQ(1U, estimator.rtt_msec_observations_.Size());
  EXPECT_EQ(1U, estimator.downstream_throughput_kbps_observations_.Size());

  TestDelegate test_delegate;
  TestURLRequestContext context(true);
  context.set_network_quality_estimator(&estimator);
  context.Init();

  scoped_ptr<URLRequest> request(context.CreateRequest(
      estimator.GetEchoURL(), DEFAULT_PRIORITY, &test_delegate));
  request->Start();
  base::RunLoop().Run();

  EXPECT_EQ(2U, estimator.rtt_msec_observations_.Size());
  EXPECT_EQ(2U, estimator.downstream_throughput_kbps_observations_.Size());
}

TEST(NetworkQualityEstimatorTest, TestObservers) {
  TestRTTObserver rtt_observer;
  TestThroughputObserver throughput_observer;
  std::map<std::string, std::string> variation_params;
  TestNetworkQualityEstimator estimator(variation_params);
  estimator.AddRTTObserver(&rtt_observer);
  estimator.AddThroughputObserver(&throughput_observer);

  TestDelegate test_delegate;
  TestURLRequestContext context(true);
  context.set_network_quality_estimator(&estimator);
  context.Init();

  EXPECT_EQ(0U, rtt_observer.observations().size());
  EXPECT_EQ(0U, throughput_observer.observations().size());
  base::TimeTicks then = base::TimeTicks::Now();

  scoped_ptr<URLRequest> request(context.CreateRequest(
      estimator.GetEchoURL(), DEFAULT_PRIORITY, &test_delegate));
  request->SetLoadFlags(request->load_flags() | LOAD_MAIN_FRAME);
  request->Start();
  base::RunLoop().Run();

  scoped_ptr<URLRequest> request2(context.CreateRequest(
      estimator.GetEchoURL(), DEFAULT_PRIORITY, &test_delegate));
  request2->SetLoadFlags(request->load_flags() | LOAD_MAIN_FRAME);
  request2->Start();
  base::RunLoop().Run();

  // Both RTT and downstream throughput should be updated.
  EXPECT_NE(NetworkQualityEstimator::InvalidRTT(),
            estimator.GetRTTEstimateInternal(base::TimeTicks(), 100));
  EXPECT_NE(NetworkQualityEstimator::kInvalidThroughput,
            estimator.GetDownlinkThroughputKbpsEstimateInternal(
                base::TimeTicks(), 100));

  EXPECT_EQ(2U, rtt_observer.observations().size());
  EXPECT_EQ(2U, throughput_observer.observations().size());
  for (auto observation : rtt_observer.observations()) {
    EXPECT_LE(0, observation.rtt_ms);
    EXPECT_LE(0, (observation.timestamp - then).InMilliseconds());
    EXPECT_EQ(NetworkQualityEstimator::URL_REQUEST, observation.source);
  }
  for (auto observation : throughput_observer.observations()) {
    EXPECT_LE(0, observation.throughput_kbps);
    EXPECT_LE(0, (observation.timestamp - then).InMilliseconds());
    EXPECT_EQ(NetworkQualityEstimator::URL_REQUEST, observation.source);
  }

  // Verify that observations from TCP and QUIC are passed on to the observers.
  base::TimeDelta tcp_rtt(base::TimeDelta::FromMilliseconds(1));
  base::TimeDelta quic_rtt(base::TimeDelta::FromMilliseconds(2));

  scoped_ptr<SocketPerformanceWatcher> tcp_watcher =
      estimator.CreateSocketPerformanceWatcher(
          SocketPerformanceWatcherFactory::PROTOCOL_TCP);
  scoped_ptr<SocketPerformanceWatcher> quic_watcher =
      estimator.CreateSocketPerformanceWatcher(
          SocketPerformanceWatcherFactory::PROTOCOL_QUIC);
  tcp_watcher->OnUpdatedRTTAvailable(tcp_rtt);
  quic_watcher->OnUpdatedRTTAvailable(quic_rtt);

  EXPECT_EQ(4U, rtt_observer.observations().size());
  EXPECT_EQ(2U, throughput_observer.observations().size());

  EXPECT_EQ(tcp_rtt.InMilliseconds(), rtt_observer.observations().at(2).rtt_ms);
  EXPECT_EQ(quic_rtt.InMilliseconds(),
            rtt_observer.observations().at(3).rtt_ms);
}

}  // namespace net
