// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/context.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "components/domain_reliability/beacon.h"
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
  beacon.domain = "localhost";
  beacon.status = "ok";
  beacon.chrome_error = net::OK;
  beacon.server_ip = "127.0.0.1";
  beacon.protocol = "HTTP";
  beacon.http_response_code = 200;
  beacon.elapsed = base::TimeDelta::FromMilliseconds(250);
  beacon.start_time = time->NowTicks() - beacon.elapsed;
  return beacon;
}

class DomainReliabilityContextTest : public testing::Test {
 protected:
  DomainReliabilityContextTest()
      : dispatcher_(&time_),
        params_(MakeTestSchedulerParams()),
        uploader_(base::Bind(&DomainReliabilityContextTest::OnUploadRequest,
                             base::Unretained(this))),
        upload_reporter_string_("test-reporter"),
        context_(&time_,
                 params_,
                 upload_reporter_string_,
                 &dispatcher_,
                 &uploader_,
                 MakeTestConfig().Pass()),
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

  bool CheckNoBeacons() {
    BeaconVector beacons;
    context_.GetQueuedBeaconsForTesting(&beacons);
    return beacons.empty();
  }

  bool CheckCounts(size_t index,
                   unsigned expected_successful,
                   unsigned expected_failed) {
    unsigned successful, failed;
    context_.GetRequestCountsForTesting(index, &successful, &failed);
    return successful == expected_successful && failed == expected_failed;
  }

  MockTime time_;
  DomainReliabilityDispatcher dispatcher_;
  DomainReliabilityScheduler::Params params_;
  MockUploader uploader_;
  std::string upload_reporter_string_;
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

  bool upload_pending_;
  std::string upload_report_;
  GURL upload_url_;
  DomainReliabilityUploader::UploadCallback upload_callback_;
};

TEST_F(DomainReliabilityContextTest, Create) {
  EXPECT_TRUE(CheckNoBeacons());
  EXPECT_TRUE(CheckCounts(0, 0, 0));
  EXPECT_TRUE(CheckCounts(1, 0, 0));
}

TEST_F(DomainReliabilityContextTest, NoResource) {
  GURL url("http://example/no_resource");
  DomainReliabilityBeacon beacon = MakeBeacon(&time_);
  context_.OnBeacon(url, beacon);

  EXPECT_TRUE(CheckNoBeacons());
  EXPECT_TRUE(CheckCounts(0, 0, 0));
  EXPECT_TRUE(CheckCounts(1, 0, 0));
}

TEST_F(DomainReliabilityContextTest, NeverReport) {
  GURL url("http://example/never_report");
  DomainReliabilityBeacon beacon = MakeBeacon(&time_);
  context_.OnBeacon(url, beacon);

  EXPECT_TRUE(CheckNoBeacons());
  EXPECT_TRUE(CheckCounts(0, 0, 0));
  EXPECT_TRUE(CheckCounts(1, 1, 0));
}

TEST_F(DomainReliabilityContextTest, AlwaysReport) {
  GURL url("http://example/always_report");
  DomainReliabilityBeacon beacon = MakeBeacon(&time_);
  context_.OnBeacon(url, beacon);

  BeaconVector beacons;
  context_.GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());
  EXPECT_TRUE(CheckCounts(0, 1, 0));
  EXPECT_TRUE(CheckCounts(1, 0, 0));
}

TEST_F(DomainReliabilityContextTest, ReportUpload) {
  GURL url("http://example/always_report");
  DomainReliabilityBeacon beacon = MakeBeacon(&time_);
  context_.OnBeacon(url, beacon);

  BeaconVector beacons;
  context_.GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());
  EXPECT_TRUE(CheckCounts(0, 1, 0));
  EXPECT_TRUE(CheckCounts(1, 0, 0));

  // N.B.: Assumes max_delay is 5 minutes.
  const char* kExpectedReport = "{"
      "\"config_version\":\"1\","
      "\"entries\":[{\"domain\":\"localhost\","
          "\"http_response_code\":200,\"protocol\":\"HTTP\","
          "\"request_age_ms\":300250,\"request_elapsed_ms\":250,"
          "\"resource\":\"always_report\",\"server_ip\":\"127.0.0.1\","
          "\"status\":\"ok\"}],"
      "\"reporter\":\"test-reporter\","
      "\"resources\":[{\"failed_requests\":0,\"name\":\"always_report\","
          "\"successful_requests\":1}]}";

  time_.Advance(max_delay());
  EXPECT_TRUE(upload_pending());
  EXPECT_EQ(kExpectedReport, upload_report());
  EXPECT_EQ(GURL("https://exampleuploader/upload"), upload_url());
  CallUploadCallback(true);

  EXPECT_TRUE(CheckNoBeacons());
  EXPECT_TRUE(CheckCounts(0, 0, 0));
  EXPECT_TRUE(CheckCounts(1, 0, 0));
}

}  // namespace
}  // namespace domain_reliability
