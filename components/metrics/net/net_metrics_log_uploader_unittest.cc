// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/net_metrics_log_uploader.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/macros.h"
#include "components/metrics/proto/reporting_info.pb.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

class NetMetricsLogUploaderTest : public testing::Test {
 public:
  NetMetricsLogUploaderTest() : on_upload_complete_count_(0) {
  }

  void CreateAndOnUploadCompleteReuseUploader() {
    ReportingInfo reporting_info;
    reporting_info.set_attempt_count(10);
    uploader_.reset(new NetMetricsLogUploader(
        NULL, "http://dummy_server", "dummy_mime", MetricsLogUploader::UMA,
        base::Bind(&NetMetricsLogUploaderTest::OnUploadCompleteReuseUploader,
                   base::Unretained(this))));
    uploader_->UploadLog("initial_dummy_data", "initial_dummy_hash",
                         reporting_info);
  }

  void OnUploadCompleteReuseUploader(int response_code, int error_code) {
    ++on_upload_complete_count_;
    if (on_upload_complete_count_ == 1) {
      ReportingInfo reporting_info;
      reporting_info.set_attempt_count(20);
      uploader_->UploadLog("dummy_data", "dummy_hash", reporting_info);
    }
  }

  int on_upload_complete_count() const {
    return on_upload_complete_count_;
  }

 private:
  std::unique_ptr<NetMetricsLogUploader> uploader_;
  int on_upload_complete_count_;

  DISALLOW_COPY_AND_ASSIGN(NetMetricsLogUploaderTest);
};

void CheckReportingInfoHeader(net::TestURLFetcher* fetcher,
                              int expected_attempt_count) {
  net::HttpRequestHeaders headers;
  fetcher->GetExtraRequestHeaders(&headers);
  std::string reporting_info_base64;
  EXPECT_TRUE(
      headers.GetHeader("X-Chrome-UMA-ReportingInfo", &reporting_info_base64));
  std::string reporting_info_string;
  EXPECT_TRUE(
      base::Base64Decode(reporting_info_base64, &reporting_info_string));
  ReportingInfo reporting_info;
  EXPECT_TRUE(reporting_info.ParseFromString(reporting_info_string));
  EXPECT_EQ(reporting_info.attempt_count(), expected_attempt_count);
}

TEST_F(NetMetricsLogUploaderTest, OnUploadCompleteReuseUploader) {
  net::TestURLFetcherFactory factory;
  CreateAndOnUploadCompleteReuseUploader();

  // Mimic the initial fetcher callback.
  net::TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  CheckReportingInfoHeader(fetcher, 10);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // Mimic the second fetcher callback.
  fetcher = factory.GetFetcherByID(0);
  CheckReportingInfoHeader(fetcher, 20);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  EXPECT_EQ(on_upload_complete_count(), 2);
}

}  // namespace metrics
