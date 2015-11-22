// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/context.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "components/domain_reliability/beacon.h"
#include "components/domain_reliability/dispatcher.h"
#include "components/domain_reliability/scheduler.h"
#include "components/domain_reliability/test_util.h"
#include "components/domain_reliability/uploader.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {
namespace {

typedef std::vector<const DomainReliabilityBeacon*> BeaconVector;

scoped_ptr<DomainReliabilityBeacon> MakeBeacon(MockableTime* time) {
  scoped_ptr<DomainReliabilityBeacon> beacon(new DomainReliabilityBeacon());
  beacon->url = GURL("https://localhost/");
  beacon->status = "tcp.connection_reset";
  beacon->chrome_error = net::ERR_CONNECTION_RESET;
  beacon->server_ip = "127.0.0.1";
  beacon->was_proxied = false;
  beacon->protocol = "HTTP";
  beacon->http_response_code = -1;
  beacon->elapsed = base::TimeDelta::FromMilliseconds(250);
  beacon->start_time = time->NowTicks() - beacon->elapsed;
  return beacon.Pass();
}

class DomainReliabilityContextTest : public testing::Test {
 protected:
  DomainReliabilityContextTest()
      : last_network_change_time_(time_.NowTicks()),
        dispatcher_(&time_),
        params_(MakeTestSchedulerParams()),
        uploader_(base::Bind(&DomainReliabilityContextTest::OnUploadRequest,
                             base::Unretained(this))),
        upload_reporter_string_("test-reporter"),
        context_(&time_,
                 params_,
                 upload_reporter_string_,
                 &last_network_change_time_,
                 &dispatcher_,
                 &uploader_,
                 MakeTestConfig().Pass()),
        upload_pending_(false) {
    // Make sure that the last network change does not overlap requests
    // made in test cases, which start 250ms in the past (see |MakeBeacon|).
    last_network_change_time_ = time_.NowTicks();
    time_.Advance(base::TimeDelta::FromSeconds(1));
  }

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

  void CallUploadCallback(DomainReliabilityUploader::UploadResult result) {
    DCHECK(upload_pending_);
    upload_callback_.Run(result);
    upload_pending_ = false;
  }

  bool CheckNoBeacons() {
    BeaconVector beacons;
    context_.GetQueuedBeaconsForTesting(&beacons);
    return beacons.empty();
  }

  MockTime time_;
  base::TimeTicks last_network_change_time_;
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
}

TEST_F(DomainReliabilityContextTest, Report) {
  context_.OnBeacon(MakeBeacon(&time_));

  BeaconVector beacons;
  context_.GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());
}

TEST_F(DomainReliabilityContextTest, Upload) {
  context_.OnBeacon(MakeBeacon(&time_));

  BeaconVector beacons;
  context_.GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());

  // N.B.: Assumes max_delay is 5 minutes.
  const char* kExpectedReport = "{"
    "\"entries\":["
      "{\"failure_data\":{\"custom_error\":\"net::ERR_CONNECTION_RESET\"},"
      "\"network_changed\":false,\"protocol\":\"HTTP\","
      "\"request_age_ms\":300250,\"request_elapsed_ms\":250,"
      "\"server_ip\":\"127.0.0.1\",\"status\":\"tcp.connection_reset\","
      "\"url\":\"https://localhost/\",\"was_proxied\":false}],"
      "\"reporter\":\"test-reporter\"}";

  time_.Advance(max_delay());
  EXPECT_TRUE(upload_pending());
  EXPECT_EQ(kExpectedReport, upload_report());
  EXPECT_EQ(GURL("https://exampleuploader/upload"), upload_url());

  DomainReliabilityUploader::UploadResult result;
  result.status = DomainReliabilityUploader::UploadResult::SUCCESS;
  CallUploadCallback(result);

  EXPECT_TRUE(CheckNoBeacons());
}

TEST_F(DomainReliabilityContextTest, Upload_NetworkChanged) {
  context_.OnBeacon(MakeBeacon(&time_));

  BeaconVector beacons;
  context_.GetQueuedBeaconsForTesting(&beacons);
  EXPECT_EQ(1u, beacons.size());

  // N.B.: Assumes max_delay is 5 minutes.
  const char* kExpectedReport = "{"
    "\"entries\":["
      "{\"failure_data\":{\"custom_error\":\"net::ERR_CONNECTION_RESET\"},"
      "\"network_changed\":true,\"protocol\":\"HTTP\","
      "\"request_age_ms\":300250,\"request_elapsed_ms\":250,"
      "\"server_ip\":\"127.0.0.1\",\"status\":\"tcp.connection_reset\","
      "\"url\":\"https://localhost/\",\"was_proxied\":false}],"
      "\"reporter\":\"test-reporter\"}";

  // Simulate a network change after the request but before the upload.
  last_network_change_time_ = time_.NowTicks();
  time_.Advance(max_delay());
  EXPECT_TRUE(upload_pending());
  EXPECT_EQ(kExpectedReport, upload_report());
  EXPECT_EQ(GURL("https://exampleuploader/upload"), upload_url());

  DomainReliabilityUploader::UploadResult result;
  result.status = DomainReliabilityUploader::UploadResult::SUCCESS;
  CallUploadCallback(result);

  EXPECT_TRUE(CheckNoBeacons());
}

// TODO(ttuttle): Add beacon_unittest.cc to test serialization.

}  // namespace
}  // namespace domain_reliability
