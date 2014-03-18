// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/context.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "components/domain_reliability/test_util.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {

namespace {

typedef std::vector<DomainReliabilityBeacon> BeaconVector;

DomainReliabilityBeacon MakeBeacon(MockableTime* time) {
  DomainReliabilityBeacon beacon;
  beacon.status = "ok";
  beacon.chrome_error = net::OK;
  beacon.server_ip = "127.0.0.1";
  beacon.http_response_code = 200;
  beacon.elapsed = base::TimeDelta::FromMilliseconds(250);
  beacon.start_time = time->Now() - beacon.elapsed;
  return beacon;
}

}  // namespace

class DomainReliabilityContextTest : public testing::Test {
 protected:
  DomainReliabilityContextTest()
      : context_(&time_,
                 CreateConfig().Pass()) {}

  bool CheckNoBeacons(int index) {
    BeaconVector beacons;
    context_.GetQueuedDataForTesting(index, &beacons, NULL, NULL);
    return beacons.empty();
  }

  bool CheckCounts(int index, int expected_successful, int expected_failed) {
    int successful, failed;
    context_.GetQueuedDataForTesting(index, NULL, &successful, &failed);
    return successful == expected_successful && failed == expected_failed;
  }

  MockTime time_;
  DomainReliabilityContext context_;

 private:
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
    config->collectors.push_back(collector);

    return scoped_ptr<const DomainReliabilityConfig>(config);
  }
};

TEST_F(DomainReliabilityContextTest, Create) {
  EXPECT_TRUE(CheckNoBeacons(0));
  EXPECT_TRUE(CheckCounts(0, 0, 0));
  EXPECT_TRUE(CheckNoBeacons(1));
  EXPECT_TRUE(CheckCounts(1, 0, 0));
}

TEST_F(DomainReliabilityContextTest, NoResource) {
  DomainReliabilityBeacon beacon = MakeBeacon(&time_);
  context_.AddBeacon(beacon, GURL("http://example/no_resource"));

  EXPECT_TRUE(CheckNoBeacons(0));
  EXPECT_TRUE(CheckCounts(0, 0, 0));
  EXPECT_TRUE(CheckNoBeacons(1));
  EXPECT_TRUE(CheckCounts(1, 0, 0));
}

TEST_F(DomainReliabilityContextTest, NeverReport) {
  DomainReliabilityBeacon beacon = MakeBeacon(&time_);
  context_.AddBeacon(beacon, GURL("http://example/never_report"));

  EXPECT_TRUE(CheckNoBeacons(0));
  EXPECT_TRUE(CheckCounts(0, 0, 0));
  EXPECT_TRUE(CheckNoBeacons(1));
  EXPECT_TRUE(CheckCounts(1, 1, 0));
}

TEST_F(DomainReliabilityContextTest, AlwaysReport) {
  DomainReliabilityBeacon beacon = MakeBeacon(&time_);
  context_.AddBeacon(beacon, GURL("http://example/always_report"));

  BeaconVector beacons;
  context_.GetQueuedDataForTesting(0, &beacons, NULL, NULL);
  EXPECT_EQ(1u, beacons.size());
  EXPECT_TRUE(CheckCounts(0, 1, 0));
  EXPECT_TRUE(CheckNoBeacons(1));
  EXPECT_TRUE(CheckCounts(1, 0, 0));
}

}  // namespace domain_reliability
