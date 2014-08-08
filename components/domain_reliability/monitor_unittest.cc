// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/monitor.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/test/test_simple_task_runner.h"
#include "components/domain_reliability/baked_in_configs.h"
#include "components/domain_reliability/beacon.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/test_util.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {

namespace {

typedef std::vector<DomainReliabilityBeacon> BeaconVector;

static const size_t kAlwaysReportIndex = 0u;
static const size_t kNeverReportIndex = 1u;

scoped_refptr<net::HttpResponseHeaders> MakeHttpResponseHeaders(
    const std::string& headers) {
  return scoped_refptr<net::HttpResponseHeaders>(
      new net::HttpResponseHeaders(net::HttpUtil::AssembleRawHeaders(
          headers.c_str(), headers.length())));
}

}  // namespace

class DomainReliabilityMonitorTest : public testing::Test {
 protected:
  typedef DomainReliabilityMonitor::RequestInfo RequestInfo;

  DomainReliabilityMonitorTest()
      : network_task_runner_(new base::TestSimpleTaskRunner()),
        url_request_context_getter_(
            new net::TestURLRequestContextGetter(network_task_runner_)),
        time_(new MockTime()),
        monitor_("test-reporter", scoped_ptr<MockableTime>(time_)),
        context_(NULL) {
    monitor_.Init(url_request_context_getter_);
    context_ = monitor_.AddContextForTesting(MakeTestConfig());
  }

  static RequestInfo MakeRequestInfo() {
    RequestInfo request;
    request.status = net::URLRequestStatus();
    request.status.set_status(net::URLRequestStatus::SUCCESS);
    request.status.set_error(net::OK);
    request.response_info.socket_address =
        net::HostPortPair::FromString("12.34.56.78:80");
    request.response_info.headers = MakeHttpResponseHeaders(
        "HTTP/1.1 200 OK\n\n");
    request.response_info.network_accessed = true;
    request.response_info.was_fetched_via_proxy = false;
    request.load_flags = 0;
    request.is_upload = false;
    return request;
  }

  void OnRequestLegComplete(const RequestInfo& info) {
    monitor_.OnRequestLegComplete(info);
  }

  size_t CountPendingBeacons() {
    BeaconVector beacons;
    context_->GetQueuedBeaconsForTesting(&beacons);
    return beacons.size();
  }

  bool CheckRequestCounts(size_t index,
                          uint32 expected_successful,
                          uint32 expected_failed) {
    return CheckRequestCounts(context_,
                              index,
                              expected_successful,
                              expected_failed);
  }

  bool CheckRequestCounts(DomainReliabilityContext* context,
                          size_t index,
                          uint32 expected_successful,
                          uint32 expected_failed) {
    uint32 successful, failed;
    context->GetRequestCountsForTesting(index, &successful, &failed);
    EXPECT_EQ(expected_successful, successful);
    EXPECT_EQ(expected_failed, failed);
    return expected_successful == successful && expected_failed == failed;
  }

  DomainReliabilityContext* CreateAndAddContext(const std::string& domain) {
    return monitor_.AddContextForTesting(MakeTestConfigWithDomain(domain));
  }

  scoped_refptr<base::TestSimpleTaskRunner> network_task_runner_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  MockTime* time_;
  DomainReliabilityMonitor monitor_;
  DomainReliabilityContext* context_;
  DomainReliabilityMonitor::RequestInfo request_;
};

namespace {

TEST_F(DomainReliabilityMonitorTest, Create) {
  EXPECT_EQ(0u, CountPendingBeacons());
  EXPECT_TRUE(CheckRequestCounts(kAlwaysReportIndex, 0u, 0u));
  EXPECT_TRUE(CheckRequestCounts(kNeverReportIndex, 0u, 0u));
}

TEST_F(DomainReliabilityMonitorTest, NoContext) {
  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://no-context/");
  OnRequestLegComplete(request);

  EXPECT_EQ(0u, CountPendingBeacons());
  EXPECT_TRUE(CheckRequestCounts(kAlwaysReportIndex, 0u, 0u));
  EXPECT_TRUE(CheckRequestCounts(kNeverReportIndex, 0u, 0u));
}

TEST_F(DomainReliabilityMonitorTest, NotReported) {
  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/never_report");
  OnRequestLegComplete(request);

  EXPECT_EQ(0u, CountPendingBeacons());
  EXPECT_TRUE(CheckRequestCounts(kNeverReportIndex, 1u, 0u));
}

TEST_F(DomainReliabilityMonitorTest, NetworkFailure) {
  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/always_report");
  request.status.set_status(net::URLRequestStatus::FAILED);
  request.status.set_error(net::ERR_CONNECTION_RESET);
  request.response_info.headers = NULL;
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountPendingBeacons());
  EXPECT_TRUE(CheckRequestCounts(kAlwaysReportIndex, 0u, 1u));
}

TEST_F(DomainReliabilityMonitorTest, ServerFailure) {
  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/always_report");
  request.response_info.headers =
      MakeHttpResponseHeaders("HTTP/1.1 500 :(\n\n");
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountPendingBeacons());
  EXPECT_TRUE(CheckRequestCounts(kAlwaysReportIndex, 0u, 1u));
}

TEST_F(DomainReliabilityMonitorTest, NotReportedFailure) {
  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/never_report");
  request.status.set_status(net::URLRequestStatus::FAILED);
  request.status.set_error(net::ERR_CONNECTION_RESET);
  OnRequestLegComplete(request);

  EXPECT_EQ(0u, CountPendingBeacons());
  EXPECT_TRUE(CheckRequestCounts(kNeverReportIndex, 0u, 1u));
}

TEST_F(DomainReliabilityMonitorTest, Request) {
  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/always_report");
  OnRequestLegComplete(request);

  EXPECT_EQ(1u, CountPendingBeacons());
  EXPECT_TRUE(CheckRequestCounts(kAlwaysReportIndex, 1u, 0u));
}

// Make sure the monitor does not log requests that did not access the network.
TEST_F(DomainReliabilityMonitorTest, DidNotAccessNetwork) {
  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/always_report");
  request.response_info.network_accessed = false;
  OnRequestLegComplete(request);

  EXPECT_EQ(0u, CountPendingBeacons());
  EXPECT_TRUE(CheckRequestCounts(kAlwaysReportIndex, 0u, 0u));
}

// Make sure the monitor does not log requests that don't send cookies.
TEST_F(DomainReliabilityMonitorTest, DoNotSendCookies) {
  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/always_report");
  request.load_flags = net::LOAD_DO_NOT_SEND_COOKIES;
  OnRequestLegComplete(request);

  EXPECT_EQ(0u, CountPendingBeacons());
  EXPECT_TRUE(CheckRequestCounts(kAlwaysReportIndex, 0u, 0u));
}

// Make sure the monitor does not log upload requests.
TEST_F(DomainReliabilityMonitorTest, IsUpload) {
  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/always_report");
  request.is_upload = true;
  OnRequestLegComplete(request);

  EXPECT_EQ(0u, CountPendingBeacons());
  EXPECT_TRUE(CheckRequestCounts(kAlwaysReportIndex, 0u, 0u));
}

// Make sure the monitor does not log a network-local error.
TEST_F(DomainReliabilityMonitorTest, LocalError) {
  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/always_report");
  request.status.set_status(net::URLRequestStatus::FAILED);
  request.status.set_error(net::ERR_PROXY_CONNECTION_FAILED);
  OnRequestLegComplete(request);

  EXPECT_EQ(0u, CountPendingBeacons());
  EXPECT_TRUE(CheckRequestCounts(kAlwaysReportIndex, 0u, 0u));
}

// Make sure the monitor does not log the proxy's IP if one was used.
TEST_F(DomainReliabilityMonitorTest, WasFetchedViaProxy) {
  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/always_report");
  request.response_info.socket_address =
      net::HostPortPair::FromString("127.0.0.1:3128");
  request.response_info.was_fetched_via_proxy = true;
  OnRequestLegComplete(request);

  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());
  EXPECT_TRUE(beacons[0].server_ip.empty());

  EXPECT_TRUE(CheckRequestCounts(kAlwaysReportIndex, 1u, 0u));
}

// Will fail when baked-in configs expire, as a reminder to update them.
// (Contact ttuttle@chromium.org if this starts failing.)
TEST_F(DomainReliabilityMonitorTest, AddBakedInConfigs) {
  // AddBakedInConfigs DCHECKs that the baked-in configs parse correctly, so
  // this unittest will fail if someone tries to add an invalid config to the
  // source tree.
  monitor_.AddBakedInConfigs();

  // Count the number of baked-in configs.
  size_t num_baked_in_configs = 0;
  for (const char* const* p = kBakedInJsonConfigs; *p; ++p)
    ++num_baked_in_configs;

  // The monitor should have contexts for all of the baked-in configs, plus the
  // test one added in the test constructor.
  EXPECT_EQ(num_baked_in_configs + 1, monitor_.contexts_size_for_testing());
}

TEST_F(DomainReliabilityMonitorTest, ClearBeacons) {
  // Initially the monitor should have just the test context, with no beacons.
  EXPECT_EQ(1u, monitor_.contexts_size_for_testing());
  EXPECT_EQ(0u, CountPendingBeacons());
  EXPECT_TRUE(CheckRequestCounts(kAlwaysReportIndex, 0u, 0u));
  EXPECT_TRUE(CheckRequestCounts(kNeverReportIndex, 0u, 0u));

  // Add a beacon.
  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/always_report");
  OnRequestLegComplete(request);

  // Make sure it was added.
  EXPECT_EQ(1u, CountPendingBeacons());
  EXPECT_TRUE(CheckRequestCounts(kAlwaysReportIndex, 1u, 0u));

  monitor_.ClearBrowsingData(CLEAR_BEACONS);

  // Make sure the beacon was cleared, but not the contexts.
  EXPECT_EQ(1u, monitor_.contexts_size_for_testing());
  EXPECT_EQ(0u, CountPendingBeacons());
  EXPECT_TRUE(CheckRequestCounts(kAlwaysReportIndex, 0u, 0u));
  EXPECT_TRUE(CheckRequestCounts(kNeverReportIndex, 0u, 0u));
}

TEST_F(DomainReliabilityMonitorTest, ClearContexts) {
  // Initially the monitor should have just the test context.
  EXPECT_EQ(1u, monitor_.contexts_size_for_testing());

  monitor_.ClearBrowsingData(CLEAR_CONTEXTS);

  // Clearing contexts should leave the monitor with none.
  EXPECT_EQ(0u, monitor_.contexts_size_for_testing());
}

TEST_F(DomainReliabilityMonitorTest, IgnoreSuccessError) {
  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/always_report");
  request.status.set_error(net::ERR_QUIC_PROTOCOL_ERROR);
  OnRequestLegComplete(request);

  BeaconVector beacons;
  context_->GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());
  EXPECT_EQ(net::OK, beacons[0].chrome_error);

  EXPECT_TRUE(CheckRequestCounts(kAlwaysReportIndex, 1u, 0u));
}

TEST_F(DomainReliabilityMonitorTest, WildcardMatchesSelf) {
  DomainReliabilityContext* context = CreateAndAddContext("*.wildcard");

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://wildcard/always_report");
  OnRequestLegComplete(request);
  EXPECT_TRUE(CheckRequestCounts(context, kAlwaysReportIndex, 1u, 0u));
}

TEST_F(DomainReliabilityMonitorTest, WildcardMatchesSubdomain) {
  DomainReliabilityContext* context = CreateAndAddContext("*.wildcard");

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://test.wildcard/always_report");
  OnRequestLegComplete(request);
  EXPECT_TRUE(CheckRequestCounts(context, kAlwaysReportIndex, 1u, 0u));
}

TEST_F(DomainReliabilityMonitorTest, WildcardDoesntMatchSubsubdomain) {
  DomainReliabilityContext* context = CreateAndAddContext("*.wildcard");

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://test.test.wildcard/always_report");
  OnRequestLegComplete(request);
  EXPECT_TRUE(CheckRequestCounts(context, kAlwaysReportIndex, 0u, 0u));
}

TEST_F(DomainReliabilityMonitorTest, WildcardPrefersSelfToSelfWildcard) {
  DomainReliabilityContext* context1 = CreateAndAddContext("wildcard");
  DomainReliabilityContext* context2 = CreateAndAddContext("*.wildcard");

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://wildcard/always_report");
  OnRequestLegComplete(request);

  EXPECT_TRUE(CheckRequestCounts(context1, kAlwaysReportIndex, 1u, 0u));
  EXPECT_TRUE(CheckRequestCounts(context2, kAlwaysReportIndex, 0u, 0u));
}

TEST_F(DomainReliabilityMonitorTest, WildcardPrefersSelfToParentWildcard) {
  DomainReliabilityContext* context1 = CreateAndAddContext("test.wildcard");
  DomainReliabilityContext* context2 = CreateAndAddContext("*.wildcard");

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://test.wildcard/always_report");
  OnRequestLegComplete(request);

  EXPECT_TRUE(CheckRequestCounts(context1, kAlwaysReportIndex, 1u, 0u));
  EXPECT_TRUE(CheckRequestCounts(context2, kAlwaysReportIndex, 0u, 0u));
}

TEST_F(DomainReliabilityMonitorTest,
    WildcardPrefersSelfWildcardToParentWildcard) {
  DomainReliabilityContext* context1 = CreateAndAddContext("*.test.wildcard");
  DomainReliabilityContext* context2 = CreateAndAddContext("*.wildcard");

  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://test.wildcard/always_report");
  OnRequestLegComplete(request);

  EXPECT_TRUE(CheckRequestCounts(context1, kAlwaysReportIndex, 1u, 0u));
  EXPECT_TRUE(CheckRequestCounts(context2, kAlwaysReportIndex, 0u, 0u));
}

}  // namespace

}  // namespace domain_reliability
