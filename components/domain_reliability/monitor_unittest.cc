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
#include "components/domain_reliability/baked_in_configs.h"
#include "components/domain_reliability/beacon.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {

typedef std::vector<DomainReliabilityBeacon> BeaconVector;

class DomainReliabilityMonitorTest : public testing::Test {
 protected:
  typedef DomainReliabilityMonitor::RequestInfo RequestInfo;

  DomainReliabilityMonitorTest()
      : bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        url_request_context_getter_(new net::TestURLRequestContextGetter(
            base::MessageLoopProxy::current())),
        time_(new MockTime()),
        monitor_(url_request_context_getter_->GetURLRequestContext(),
                 scoped_ptr<MockableTime>(time_)),
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
    EXPECT_TRUE(resource->IsValid());
    config->resources.push_back(resource);

    resource = new DomainReliabilityConfig::Resource();
    resource->name = "never_report";
    resource->url_patterns.push_back(
        new std::string("http://example/never_report"));
    resource->success_sample_rate = 0.0;
    resource->failure_sample_rate = 0.0;
    EXPECT_TRUE(resource->IsValid());
    config->resources.push_back(resource);

    DomainReliabilityConfig::Collector* collector;
    collector = new DomainReliabilityConfig::Collector();
    collector->upload_url = GURL("https://example/upload");
    EXPECT_TRUE(collector->IsValid());
    config->collectors.push_back(collector);

    config->version = "1";
    config->valid_until = 1234567890.0;
    config->domain = "example";
    EXPECT_TRUE(config->IsValid());

    return scoped_ptr<const DomainReliabilityConfig>(config);
  }

  RequestInfo MakeRequestInfo() {
    RequestInfo request;
    request.status = net::URLRequestStatus();
    request.response_code = 200;
    request.was_cached = false;
    request.load_flags = 0;
    request.is_upload = false;
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
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
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

TEST_F(DomainReliabilityMonitorTest, ContextRequestWithDoNotSendCookies) {
  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/always_report");
  request.load_flags = net::LOAD_DO_NOT_SEND_COOKIES;
  OnRequestLegComplete(request);

  EXPECT_TRUE(CheckNoBeacons(0));
  EXPECT_TRUE(CheckNoBeacons(1));
}

TEST_F(DomainReliabilityMonitorTest, ContextRequestThatIsUpload) {
  RequestInfo request = MakeRequestInfo();
  request.url = GURL("http://example/always_report");
  request.is_upload = true;
  OnRequestLegComplete(request);

  EXPECT_TRUE(CheckNoBeacons(0));
  EXPECT_TRUE(CheckNoBeacons(1));
}

TEST_F(DomainReliabilityMonitorTest, AddBakedInConfigs) {
  // AddBakedInConfigs DCHECKs that the baked-in configs parse correctly, so
  // this unittest will fail if someone tries to add an invalid config to the
  // source tree.
  monitor_.AddBakedInConfigs();

  size_t num_baked_in_configs = 0;
  for (const char* const* p = kBakedInJsonConfigs; *p; ++p)
    ++num_baked_in_configs;

  // The monitor should have contexts for all of the baked-in configs, plus the
  // test one added in the test constructor.
  EXPECT_EQ(num_baked_in_configs + 1, monitor_.contexts_size_for_testing());
}

}  // namespace domain_reliability
