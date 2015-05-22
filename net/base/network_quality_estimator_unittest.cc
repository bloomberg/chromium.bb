// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_quality_estimator.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_quality.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

// SpawnedTestServer not supported on iOS (see http://crbug.com/148666).
#if !defined(OS_IOS)
TEST(NetworkQualityEstimatorTest, TestPeakKbpsFastestRTTUpdates) {
  SpawnedTestServer test_server_(
      SpawnedTestServer::TYPE_HTTP, SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  ASSERT_TRUE(test_server_.Start());

  // Enable requests to local host to be used for network quality estimation.
  NetworkQualityEstimator estimator(true);
  {
    NetworkQuality network_quality = estimator.GetEstimate();
    EXPECT_EQ(network_quality.fastest_rtt_confidence, 0);
    EXPECT_EQ(network_quality.peak_throughput_kbps_confidence, 0);
  }

  TestDelegate d;
  TestURLRequestContext context(false);

  uint64 min_transfer_size_in_bytes =
      NetworkQualityEstimator::kMinTransferSizeInBytes;
  base::TimeDelta request_duration = base::TimeDelta::FromMicroseconds(
      NetworkQualityEstimator::kMinRequestDurationMicroseconds);

  scoped_ptr<URLRequest> request(context.CreateRequest(
      test_server_.GetURL("echo.html"), DEFAULT_PRIORITY, &d));
  request->Start();

  base::RunLoop().Run();

  base::PlatformThread::Sleep(request_duration);

  // With smaller transfer, |fastest_rtt| will be updated but not
  // |peak_throughput_kbps|.
  estimator.NotifyDataReceived(*(request.get()),
                               min_transfer_size_in_bytes - 1);
  {
    NetworkQuality network_quality = estimator.GetEstimate();
    EXPECT_GT(network_quality.fastest_rtt_confidence, 0);
    EXPECT_EQ(network_quality.peak_throughput_kbps_confidence, 0);
  }

  // With large transfer, both |fastest_rtt| and |peak_throughput_kbps| will be
  // updated.
  estimator.NotifyDataReceived(*(request.get()), min_transfer_size_in_bytes);
  {
    NetworkQuality network_quality = estimator.GetEstimate();
    EXPECT_GT(network_quality.fastest_rtt_confidence, 0);
    EXPECT_GT(network_quality.peak_throughput_kbps_confidence, 0);
    EXPECT_GE(network_quality.fastest_rtt, request_duration);
    EXPECT_GT(network_quality.peak_throughput_kbps, uint32(0));
    EXPECT_LE(
        network_quality.peak_throughput_kbps,
        min_transfer_size_in_bytes * 8.0 / request_duration.InMilliseconds());
  }
  EXPECT_EQ(estimator.bytes_read_since_last_connection_change_, true);

  // Check UMA histograms.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("NQE.PeakKbps.Unknown", 0);
  histogram_tester.ExpectTotalCount("NQE.FastestRTT.Unknown", 0);

  estimator.OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
  histogram_tester.ExpectTotalCount("NQE.PeakKbps.Unknown", 1);
  histogram_tester.ExpectTotalCount("NQE.FastestRTT.Unknown", 1);
  {
    NetworkQuality network_quality = estimator.GetEstimate();
    EXPECT_EQ(estimator.current_connection_type_,
              NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
    EXPECT_EQ(network_quality.fastest_rtt_confidence, 0);
    EXPECT_EQ(network_quality.peak_throughput_kbps_confidence, 0);
  }
}
#endif  // !defined(OS_IOS)

}  // namespace net
