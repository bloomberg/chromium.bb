// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/net/metrics_network_client.h"

#include "base/mac/scoped_nsobject.h"
#include "base/test/histogram_tester.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

// Dummy client to be registered as underlying client for the
// MetricsNetworkClient.
@interface MetricsMockClient : CRNForwardingNetworkClient
@end

@implementation MetricsMockClient

- (void)didFailWithNSErrorCode:(NSInteger)nsErrorCode
                  netErrorCode:(int)netErrorCode {
}

- (void)didFinishLoading {
}

@end

namespace {

// Name for the histogram the MetricsNetworkClient has to update.
const char kHistogramName[] = "Net.ErrorCodesForMainFrame3";

class MetricsNetworkClientTest : public testing::Test {
 public:
  MetricsNetworkClientTest()
      : histogram_tester_(), client_([[MetricsNetworkClient alloc] init]) {
    // Setup a dummy underlying client to avoid DCHECKs.
    base::scoped_nsobject<MetricsMockClient> underying_client(
        [[MetricsMockClient alloc] init]);
    [client_ setUnderlyingClient:underying_client];
  }

  // Returns true if there are no samples for "Net.ErrorCodesForMainFrame3".
  void VerifyNoSamples() {
    histogram_tester_.ExpectTotalCount(kHistogramName, 0);
  }

 protected:
  base::HistogramTester histogram_tester_;
  base::scoped_nsobject<MetricsNetworkClient> client_;
};

}  // namespace

TEST_F(MetricsNetworkClientTest, HistogramUpdatedOnErrors) {
  int net_error = net::ERR_FAILED;
  VerifyNoSamples();
  // NSURLErrorCancelled errors must not update the histogram.
  [client_ didFailWithNSErrorCode:NSURLErrorCancelled netErrorCode:net_error];
  VerifyNoSamples();
  // Other iOS errors update the histogram.
  [client_ didFailWithNSErrorCode:NSURLErrorCannotConnectToHost
                     netErrorCode:net_error];
  // |net_error| is negative, the histogram reports the opposite value.
  histogram_tester_.ExpectUniqueSample(kHistogramName, -net_error, 1);
}

TEST_F(MetricsNetworkClientTest, HistogramUpdatedOnSuccess) {
  VerifyNoSamples();
  [client_ didFinishLoading];
  histogram_tester_.ExpectUniqueSample(kHistogramName, -net::OK, 1);
}
