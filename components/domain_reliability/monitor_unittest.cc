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
#include "components/domain_reliability/beacon.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {

typedef std::vector<DomainReliabilityBeacon> BeaconVector;

class DomainReliabilityMonitorTest : public testing::Test {
 protected:
  typedef DomainReliabilityMonitor::RequestInfo RequestInfo;

  DomainReliabilityMonitorTest()
      : bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        time_(new MockTime()),
        monitor_(scoped_ptr<MockableTime>(time_)),
        context_(monitor_.AddContextForTesting(CreateConfig())) {}

  static scoped_ptr<const DomainReliabilityConfig> CreateConfig() {
    DomainReliabilityConfig* config = new DomainReliabilityConfig();
    DomainReliabilityConfig::Resource* resource;

    resource = new DomainReliabilityConfig::Resource();
    resource->name = "always_report";
    resource->url_patterns.push_back(
        new std::string("http://example/always_report"));
    resource->success_sample_rate = 1.0;
    resource->failure_sample_rate = 1.0;
    config->resources.push_back(resource);

    resource = new DomainReliabilityConfig::Resource();
    resource->name = "never_report";
    resource->url_patterns.push_back(
        new std::string("http://example/never_report"));
    resource->success_sample_rate = 0.0;
    resource->failure_sample_rate = 0.0;
    config->resources.push_back(resource);

    DomainReliabilityConfig::Collector* collector;
    collector = new DomainReliabilityConfig::Collector();
    collector->upload_url = GURL("https://example/upload");
    config->domain = "example";
    config->collectors.push_back(collector);

    return scoped_ptr<const DomainReliabilityConfig>(config);
  }

  RequestInfo MakeRequestInfo() {
    RequestInfo request;
    request.status = net::URLRequestStatus();
    request.response_code = 200;
    request.was_cached = false;
    return request;
  }

  bool CheckNoBeacons(int index) {
    BeaconVector beacons;
    int successful, failed;
    context_->GetQueuedDataForTesting(index, &beacons, &successful, &failed);
    return beacons.empty() && successful == 0 && failed == 0;
  }

  void OnRequestLegComplete(const RequestInfo& info) {
    monitor_.OnRequestLegComplete(info);
  }

  content::TestBrowserThreadBundle bundle_;
  MockTime* time_;
  DomainReliabilityMonitor monitor_;
  DomainReliabilityContext* context_;
  DomainReliabilityMonitor::RequestInfo request_;
};

TEST_F(DomainReliabilityMonitorTest, Create) {
  EXPECT_TRUE(CheckNoBeacons(0));
  EXPECT_TRUE(CheckNoBeacons(1));
}

TEST_F(DomainReliabilityMonitorTest, NoContextRequest) {
  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://no-context/");
  OnRequestLegComplete(request);

  EXPECT_TRUE(CheckNoBeacons(0));
  EXPECT_TRUE(CheckNoBeacons(1));
}

TEST_F(DomainReliabilityMonitorTest, ContextRequest) {
  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/always_report");
  OnRequestLegComplete(request);

  BeaconVector beacons;
  context_->GetQueuedDataForTesting(0, &beacons, NULL, NULL);
  EXPECT_EQ(1u, beacons.size());
  EXPECT_TRUE(CheckNoBeacons(1));
}

}  // namespace domain_reliability
