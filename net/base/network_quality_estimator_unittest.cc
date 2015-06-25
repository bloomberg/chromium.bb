// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_quality_estimator.h"

#include <limits>
#include <map>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_quality.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

TEST(NetworkQualityEstimatorTest, TestPeakKbpsFastestRTTUpdates) {
  net::test_server::EmbeddedTestServer embedded_test_server;
  embedded_test_server.ServeFilesFromDirectory(
      base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  ASSERT_TRUE(embedded_test_server.InitializeAndWaitUntilReady());

  // Enable requests to local host to be used for network quality estimation.
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimator estimator(variation_params, true, true);
  {
    NetworkQuality network_quality = estimator.GetPeakEstimate();
    EXPECT_EQ(NetworkQuality::InvalidRTT(), network_quality.rtt());
    EXPECT_EQ(NetworkQuality::kInvalidThroughput,
              network_quality.downstream_throughput_kbps());
  }

  TestDelegate test_delegate;
  TestURLRequestContext context(false);

  scoped_ptr<URLRequest> request(
      context.CreateRequest(embedded_test_server.GetURL("/echo.html"),
                            DEFAULT_PRIORITY, &test_delegate));
  request->Start();

  base::RunLoop().Run();

  // Both RTT and downstream throughput should be updated.
  estimator.NotifyDataReceived(*request, 1000, 1000);
  {
    NetworkQuality network_quality = estimator.GetPeakEstimate();
    EXPECT_LT(network_quality.rtt(), base::TimeDelta::Max());
    EXPECT_GE(network_quality.downstream_throughput_kbps(), 1);
  }

  // Check UMA histograms.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("NQE.PeakKbps.Unknown", 0);
  histogram_tester.ExpectTotalCount("NQE.FastestRTT.Unknown", 0);

  estimator.OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
  histogram_tester.ExpectTotalCount("NQE.PeakKbps.Unknown", 1);
  histogram_tester.ExpectTotalCount("NQE.FastestRTT.Unknown", 1);
  {
    NetworkQuality network_quality = estimator.GetPeakEstimate();
    EXPECT_EQ(estimator.current_connection_type_,
              NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
    EXPECT_EQ(NetworkQuality::InvalidRTT(), network_quality.rtt());
    EXPECT_EQ(NetworkQuality::kInvalidThroughput,
              network_quality.downstream_throughput_kbps());
  }
}

TEST(NetworkQualityEstimatorTest, StoreObservations) {
  net::test_server::EmbeddedTestServer embedded_test_server;
  embedded_test_server.ServeFilesFromDirectory(
      base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  ASSERT_TRUE(embedded_test_server.InitializeAndWaitUntilReady());

  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimator estimator(variation_params, true, true);
  TestDelegate test_delegate;
  TestURLRequestContext context(false);

  // Push 10 more observations than the maximum buffer size.
  for (size_t i = 0;
       i < estimator.GetMaximumObservationBufferSizeForTests() + 10U; ++i) {
    scoped_ptr<URLRequest> request(
        context.CreateRequest(embedded_test_server.GetURL("/echo.html"),
                              DEFAULT_PRIORITY, &test_delegate));
    request->Start();
    base::RunLoop().Run();

    estimator.NotifyDataReceived(*request, 1000, 1000);
  }

  EXPECT_TRUE(estimator.VerifyBufferSizeForTests(
      estimator.GetMaximumObservationBufferSizeForTests()));

  // Verify that the stored observations are cleared on network change.
  estimator.OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
  EXPECT_TRUE(estimator.VerifyBufferSizeForTests(0U));

  scoped_ptr<URLRequest> request(
      context.CreateRequest(embedded_test_server.GetURL("/echo.html"),
                            DEFAULT_PRIORITY, &test_delegate));

  // Verify that overflow protection works.
  request->Start();
  base::RunLoop().Run();
  estimator.NotifyDataReceived(*request, std::numeric_limits<int64_t>::max(),
                               std::numeric_limits<int64_t>::max());
  {
    NetworkQuality network_quality = estimator.GetPeakEstimate();
    EXPECT_EQ(std::numeric_limits<int32_t>::max() - 1,
              network_quality.downstream_throughput_kbps());
  }
}

// Verifies that the percentiles are correctly computed. All observations have
// the same timestamp. Kbps percentiles must be in decreasing order. RTT
// percentiles must be in increasing order.
TEST(NetworkQualityEstimatorTest, PercentileSameTimestamps) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimator estimator(variation_params);
  base::TimeTicks now = base::TimeTicks::Now();

  // Network quality should be unavailable when no observations are available.
  NetworkQuality network_quality;
  EXPECT_FALSE(estimator.GetEstimate(&network_quality));

  // Insert samples from {1,2,3,..., 100}. First insert odd samples, then even
  // samples. This helps in verifying that the order of samples does not matter.
  for (int i = 1; i <= 99; i += 2) {
    estimator.kbps_observations_.AddObservation(
        NetworkQualityEstimator::Observation(i, now));
    estimator.rtt_msec_observations_.AddObservation(
        NetworkQualityEstimator::Observation(i, now));
    EXPECT_TRUE(estimator.GetEstimate(&network_quality));
  }

  for (int i = 2; i <= 100; i += 2) {
    estimator.kbps_observations_.AddObservation(
        NetworkQualityEstimator::Observation(i, now));
    estimator.rtt_msec_observations_.AddObservation(
        NetworkQualityEstimator::Observation(i, now));
    EXPECT_TRUE(estimator.GetEstimate(&network_quality));
  }

  for (int i = 0; i <= 100; ++i) {
    // Checks if the difference between the two integers is less than 1. This is
    // required because computed percentiles may be slightly different from
    // what is expected due to floating point computation errors and integer
    // rounding off errors.
    EXPECT_NEAR(estimator.GetEstimate(i).downstream_throughput_kbps(), 100 - i,
                1);
    EXPECT_NEAR(estimator.GetEstimate(i).rtt().InMilliseconds(), i, 1);
  }

  EXPECT_TRUE(estimator.GetEstimate(&network_quality));
  // |network_quality| should be equal to the 50 percentile value.
  EXPECT_EQ(estimator.GetEstimate(50).downstream_throughput_kbps(),
            network_quality.downstream_throughput_kbps());
  EXPECT_EQ(estimator.GetEstimate(50).rtt(), network_quality.rtt());
}

// Verifies that the percentiles are correctly computed. Observations have
// different timestamps with half the observations being very old and the rest
// of them being very recent. Percentiles should factor in recent observations
// much more heavily than older samples. Kbps percentiles must be in decreasing
// order. RTT percentiles must be in increasing order.
TEST(NetworkQualityEstimatorTest, PercentileDifferentTimestamps) {
  std::map<std::string, std::string> variation_params;
  NetworkQualityEstimator estimator(variation_params);
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks very_old = base::TimeTicks::UnixEpoch();

  // First 50 samples have very old timestamp.
  for (int i = 1; i <= 50; ++i) {
    estimator.kbps_observations_.AddObservation(
        NetworkQualityEstimator::Observation(i, very_old));
    estimator.rtt_msec_observations_.AddObservation(
        NetworkQualityEstimator::Observation(i, very_old));
  }

  // Next 50 (i.e., from 51 to 100) have recent timestamp.
  for (int i = 51; i <= 100; ++i) {
    estimator.kbps_observations_.AddObservation(
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
    EXPECT_NEAR(estimator.GetEstimate(i).downstream_throughput_kbps(),
                51 + 0.49 * (100 - i), 1);
    EXPECT_NEAR(estimator.GetEstimate(i).rtt().InMilliseconds(), 51 + 0.49 * i,
                1);
  }
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
  NetworkQualityEstimator estimator(variation_params, true, true);
  NetworkQuality network_quality = estimator.GetPeakEstimate();
  EXPECT_EQ(NetworkQuality::InvalidRTT(), network_quality.rtt());
  EXPECT_EQ(NetworkQuality::kInvalidThroughput,
            network_quality.downstream_throughput_kbps());

  TestDelegate test_delegate;
  TestURLRequestContext context(false);

  uint64 min_transfer_size_in_bytes =
      NetworkQualityEstimator::kMinTransferSizeInBytes;

  // Number of observations are more than the maximum buffer size.
  for (size_t i = 0;
       i < estimator.GetMaximumObservationBufferSizeForTests() + 100U; ++i) {
    scoped_ptr<URLRequest> request(
        context.CreateRequest(embedded_test_server.GetURL("/echo.html"),
                              DEFAULT_PRIORITY, &test_delegate));
    request->Start();
    base::RunLoop().Run();

    // Use different number of bytes to create variation.
    estimator.NotifyDataReceived(*request, min_transfer_size_in_bytes + i * 100,
                                 min_transfer_size_in_bytes + i * 100);
  }

  // Verify the percentiles through simple tests.
  for (int i = 0; i <= 100; ++i) {
    EXPECT_GT(estimator.GetEstimate(i).downstream_throughput_kbps(), 0);
    EXPECT_LT(estimator.GetEstimate(i).rtt(), base::TimeDelta::Max());

    if (i != 0) {
      // Throughput percentiles are in decreasing order.
      EXPECT_LE(estimator.GetEstimate(i).downstream_throughput_kbps(),
                estimator.GetEstimate(i - 1).downstream_throughput_kbps());

      // RTT percentiles are in increasing order.
      EXPECT_GE(estimator.GetEstimate(i).rtt(),
                estimator.GetEstimate(i - 1).rtt());
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

  NetworkQualityEstimator estimator(variation_params, true, true);
  EXPECT_EQ(1U, estimator.kbps_observations_.Size());
  EXPECT_EQ(1U, estimator.rtt_msec_observations_.Size());
  NetworkQuality network_quality;
  EXPECT_TRUE(estimator.GetEstimate(&network_quality));
  EXPECT_EQ(100, network_quality.downstream_throughput_kbps());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1000), network_quality.rtt());
  auto it = estimator.kbps_observations_.observations_.begin();
  EXPECT_EQ(100, (*it).value);
  it = estimator.rtt_msec_observations_.observations_.begin();
  EXPECT_EQ(1000, (*it).value);

  // Simulate network change to Wi-Fi.
  estimator.OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
  EXPECT_EQ(1U, estimator.kbps_observations_.Size());
  EXPECT_EQ(1U, estimator.rtt_msec_observations_.Size());
  EXPECT_TRUE(estimator.GetEstimate(&network_quality));
  EXPECT_EQ(200, network_quality.downstream_throughput_kbps());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(2000), network_quality.rtt());
  it = estimator.kbps_observations_.observations_.begin();
  EXPECT_EQ(200, (*it).value);
  it = estimator.rtt_msec_observations_.observations_.begin();
  EXPECT_EQ(2000, (*it).value);

  // Peak network quality should not be affected by the network quality
  // estimator field trial.
  EXPECT_EQ(NetworkQuality::InvalidRTT(),
            estimator.fastest_rtt_since_last_connection_change_);
  EXPECT_EQ(NetworkQuality::kInvalidThroughput,
            estimator.peak_kbps_since_last_connection_change_);

  // Simulate network change to 2G. Only the Kbps default estimate should be
  // available.
  estimator.OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType::CONNECTION_2G);
  EXPECT_EQ(1U, estimator.kbps_observations_.Size());
  EXPECT_EQ(0U, estimator.rtt_msec_observations_.Size());
  // For GetEstimate() to return true, at least one observation must be
  // available for both RTT and downstream throughput.
  EXPECT_FALSE(estimator.GetEstimate(&network_quality));
  it = estimator.kbps_observations_.observations_.begin();
  EXPECT_EQ(300, (*it).value);

  // Simulate network change to 3G. Default estimates should be unavailable.
  estimator.OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType::CONNECTION_3G);
  EXPECT_FALSE(estimator.GetEstimate(&network_quality));
  EXPECT_EQ(0U, estimator.kbps_observations_.Size());
  EXPECT_EQ(0U, estimator.rtt_msec_observations_.Size());
}

}  // namespace net
