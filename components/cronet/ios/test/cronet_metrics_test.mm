// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cronet/Cronet.h>

#include "components/cronet/ios/cronet_metrics.h"
#include "components/cronet/ios/test/cronet_test_base.h"

#include "testing/gtest_mac.h"

@interface TestMetricsDelegate : NSObject<CronetMetricsDelegate>
@end

@implementation TestMetricsDelegate
- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task
                    didFinishCollectingMetrics:(NSURLSessionTaskMetrics*)metrics
    NS_AVAILABLE_IOS(10.0) {
  // This is never actually called, so its definition currently doesn't matter.
}
@end

namespace cronet {

class CronetMetricsTest : public CronetTestBase {
 protected:
  CronetMetricsTest() {}
  ~CronetMetricsTest() override {}
};

TEST_F(CronetMetricsTest, Setters) {
  if (@available(iOS 10, *)) {
    CronetMetrics* cronet_metrics = [[CronetMetrics alloc] init];

    NSDate* test_date = [NSDate date];
    NSURLRequest* test_req = [NSURLRequest
        requestWithURL:[NSURL URLWithString:@"test.example.com"]];
    NSURLResponse* test_resp = [[NSURLResponse alloc]
                  initWithURL:[NSURL URLWithString:@"test.example.com"]
                     MIMEType:@"text/plain"
        expectedContentLength:128
             textEncodingName:@"ascii"];

    [cronet_metrics setRequest:test_req];
    [cronet_metrics setResponse:test_resp];

    [cronet_metrics setFetchStartDate:test_date];
    [cronet_metrics setDomainLookupStartDate:test_date];
    [cronet_metrics setDomainLookupEndDate:test_date];
    [cronet_metrics setConnectStartDate:test_date];
    [cronet_metrics setSecureConnectionStartDate:test_date];
    [cronet_metrics setSecureConnectionEndDate:test_date];
    [cronet_metrics setConnectEndDate:test_date];
    [cronet_metrics setRequestStartDate:test_date];
    [cronet_metrics setRequestEndDate:test_date];
    [cronet_metrics setResponseStartDate:test_date];
    [cronet_metrics setResponseEndDate:test_date];

    [cronet_metrics setNetworkProtocolName:@"http/2"];
    [cronet_metrics setProxyConnection:YES];
    [cronet_metrics setReusedConnection:YES];
    [cronet_metrics setResourceFetchType:
                        NSURLSessionTaskMetricsResourceFetchTypeNetworkLoad];

    NSURLSessionTaskTransactionMetrics* metrics = cronet_metrics;

    EXPECT_EQ([metrics request], test_req);
    EXPECT_EQ([metrics response], test_resp);

    EXPECT_EQ([metrics fetchStartDate], test_date);
    EXPECT_EQ([metrics domainLookupStartDate], test_date);
    EXPECT_EQ([metrics domainLookupEndDate], test_date);
    EXPECT_EQ([metrics connectStartDate], test_date);
    EXPECT_EQ([metrics secureConnectionStartDate], test_date);
    EXPECT_EQ([metrics secureConnectionEndDate], test_date);
    EXPECT_EQ([metrics connectEndDate], test_date);
    EXPECT_EQ([metrics requestStartDate], test_date);
    EXPECT_EQ([metrics requestEndDate], test_date);
    EXPECT_EQ([metrics responseStartDate], test_date);
    EXPECT_EQ([metrics responseEndDate], test_date);

    EXPECT_EQ([metrics networkProtocolName], @"http/2");
    EXPECT_EQ([metrics isProxyConnection], YES);
    EXPECT_EQ([metrics isReusedConnection], YES);
    EXPECT_EQ([metrics resourceFetchType],
              NSURLSessionTaskMetricsResourceFetchTypeNetworkLoad);
  }
}

TEST_F(CronetMetricsTest, Delegate) {
  if (@available(iOS 10, *)) {
    TestMetricsDelegate* metrics_delegate =
        [[TestMetricsDelegate alloc] init];
    EXPECT_TRUE([Cronet addMetricsDelegate:metrics_delegate]);
    EXPECT_FALSE([Cronet addMetricsDelegate:metrics_delegate]);
    EXPECT_TRUE([Cronet removeMetricsDelegate:metrics_delegate]);
    EXPECT_FALSE([Cronet removeMetricsDelegate:metrics_delegate]);
  }
}

}  // namespace cronet
