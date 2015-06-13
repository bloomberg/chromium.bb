// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_quality_estimator.h"

#include <limits>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/threading/platform_thread.h"
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
  NetworkQualityEstimator estimator(true);
  {
    NetworkQuality network_quality = estimator.GetPeakEstimate();
    EXPECT_EQ(network_quality.rtt(), base::TimeDelta::Max());
    EXPECT_EQ(network_quality.downstream_throughput_kbps(), 0);
  }

  TestDelegate test_delegate;
  TestURLRequestContext context(false);

  uint64_t min_transfer_size_in_bytes =
      NetworkQualityEstimator::kMinTransferSizeInBytes;
  base::TimeDelta request_duration = base::TimeDelta::FromMicroseconds(
      NetworkQualityEstimator::kMinRequestDurationMicroseconds);

  scoped_ptr<URLRequest> request(
      context.CreateRequest(embedded_test_server.GetURL("/echo.html"),
                            DEFAULT_PRIORITY, &test_delegate));
  request->Start();

  base::RunLoop().Run();

  base::PlatformThread::Sleep(request_duration);

  // With smaller transfer, RTT will be updated but not the downstream
  // throughput.
  estimator.NotifyDataReceived(*request, min_transfer_size_in_bytes - 1,
                               min_transfer_size_in_bytes - 1);
  {
    NetworkQuality network_quality = estimator.GetPeakEstimate();
    EXPECT_GT(network_quality.rtt(), base::TimeDelta());
    EXPECT_EQ(network_quality.downstream_throughput_kbps(), 0);
  }

  // With large transfer, both RTT and downlink throughput will be updated.
  estimator.NotifyDataReceived(*request, min_transfer_size_in_bytes,
                               min_transfer_size_in_bytes);
  {
    NetworkQuality network_quality = estimator.GetPeakEstimate();
    EXPECT_GE(network_quality.rtt(), request_duration);
    EXPECT_GT(network_quality.downstream_throughput_kbps(), 0);
    EXPECT_LE(
        network_quality.downstream_throughput_kbps(),
        min_transfer_size_in_bytes * 8.0 / request_duration.InMilliseconds());
  }
  EXPECT_LT(estimator.fastest_rtt_since_last_connection_change_,
            base::TimeDelta::Max());

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
    EXPECT_EQ(network_quality.rtt(), base::TimeDelta::Max());
    EXPECT_EQ(network_quality.downstream_throughput_kbps(), 0);
  }
}

TEST(NetworkQualityEstimatorTest, StoreObservations) {
  net::test_server::EmbeddedTestServer embedded_test_server;
  embedded_test_server.ServeFilesFromDirectory(
      base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  ASSERT_TRUE(embedded_test_server.InitializeAndWaitUntilReady());

  NetworkQualityEstimator estimator(true);
  TestDelegate test_delegate;
  TestURLRequestContext context(false);

  uint64 min_transfer_size_in_bytes =
      NetworkQualityEstimator::kMinTransferSizeInBytes;
  base::TimeDelta request_duration = base::TimeDelta::FromMicroseconds(
      NetworkQualityEstimator::kMinRequestDurationMicroseconds);

  // Push 10 more observations than the maximum buffer size.
  for (size_t i = 0;
       i < estimator.GetMaximumObservationBufferSizeForTests() + 10U; ++i) {
    scoped_ptr<URLRequest> request(
        context.CreateRequest(embedded_test_server.GetURL("/echo.html"),
                              DEFAULT_PRIORITY, &test_delegate));
    request->Start();
    base::RunLoop().Run();
    base::PlatformThread::Sleep(request_duration);

    estimator.NotifyDataReceived(*request, min_transfer_size_in_bytes,
                                 min_transfer_size_in_bytes);
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
  base::PlatformThread::Sleep(request_duration);
  estimator.NotifyDataReceived(*request, std::numeric_limits<int64_t>::max(),
                               std::numeric_limits<int64_t>::max());
  {
    NetworkQuality network_quality = estimator.GetPeakEstimate();
    EXPECT_EQ(std::numeric_limits<int32_t>::max() - 1,
              network_quality.downstream_throughput_kbps());
  }
}

}  // namespace net
