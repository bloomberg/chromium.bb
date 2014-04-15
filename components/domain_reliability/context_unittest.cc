// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/context.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "components/domain_reliability/dispatcher.h"
#include "components/domain_reliability/scheduler.h"
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
  beacon.start_time = time->NowTicks() - beacon.elapsed;
  return beacon;
}

}  // namespace

class DomainReliabilityContextTest : public testing::Test {
 protected:
  DomainReliabilityContextTest()
      : dispatcher_(&time_),
        params_(CreateParams()),
        uploader_(base::Bind(&DomainReliabilityContextTest::OnUploadRequest,
                             base::Unretained(this))),
        context_(&time_,
                 params_,
                 &dispatcher_,
                 &uploader_,
                 CreateConfig().Pass()),
        upload_pending_(false) {}

  TimeDelta min_delay() const { return params_.minimum_upload_delay; }
  TimeDelta max_delay() const { return params_.maximum_upload_delay; }
  TimeDelta retry_interval() const { return params_.upload_retry_interval; }
  TimeDelta zero_delta() const { return TimeDelta::FromMicroseconds(0); }

  bool upload_pending() { return upload_pending_; }

  const std::string& upload_report() {
    DCHECK(upload_pending_);
    return upload_report_;
  }

  const GURL& upload_url() {
    DCHECK(upload_pending_);
    return upload_url_;
  }

  void CallUploadCallback(bool success) {
    DCHECK(upload_pending_);
    upload_callback_.Run(success);
    upload_pending_ = false;
  }

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
  DomainReliabilityDispatcher dispatcher_;
  DomainReliabilityScheduler::Params params_;
  MockUploader uploader_;
  DomainReliabilityContext context_;

 private:
  void OnUploadRequest(
      const std::string& report_json,
      const GURL& upload_url,
      const DomainReliabilityUploader::UploadCallback& callback) {
    DCHECK(!upload_pending_);
    upload_report_ = report_json;
    upload_url_ = upload_url;
    upload_callback_ = callback;
    upload_pending_ = true;
  }

  static DomainReliabilityScheduler::Params CreateParams() {
    DomainReliabilityScheduler::Params params;
    params.minimum_upload_delay = base::TimeDelta::FromSeconds(60);
    params.maximum_upload_delay = base::TimeDelta::FromSeconds(300);
    params.upload_retry_interval = base::TimeDelta::FromSeconds(15);
    return params;
  }

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

  bool upload_pending_;
  std::string upload_report_;
  GURL upload_url_;
  DomainReliabilityUploader::UploadCallback upload_callback_;
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

TEST_F(DomainReliabilityContextTest, ReportUpload) {
  DomainReliabilityBeacon beacon = MakeBeacon(&time_);
  context_.AddBeacon(beacon, GURL("http://example/always_report"));

  const char* kExpectedReport = "{\"reporter\":\"chrome\","
      "\"resource_reports\":[{\"beacons\":[{\"http_response_code\":200,"
      "\"request_age_ms\":300250,\"request_elapsed_ms\":250,\"server_ip\":"
      "\"127.0.0.1\",\"status\":\"ok\"}],\"failed_requests\":0,"
      "\"resource_name\":\"always_report\",\"successful_requests\":1},"
      "{\"beacons\":[],\"failed_requests\":0,\"resource_name\":"
      "\"never_report\",\"successful_requests\":0}]}";

  time_.Advance(max_delay());
  EXPECT_TRUE(upload_pending());
  EXPECT_EQ(kExpectedReport, upload_report());
  EXPECT_EQ(GURL("https://example/upload"), upload_url());
  CallUploadCallback(true);
}

}  // namespace domain_reliability
