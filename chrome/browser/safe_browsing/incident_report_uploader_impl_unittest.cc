// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_report_uploader_impl.h"

#include <string>

#include "base/test/test_simple_task_runner.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

class IncidentReportUploaderImplTest : public testing::Test {
 public:
  // safe_browsing::IncidentReportUploader::OnResultCallback implementation.
  void OnReportUploadResult(
      safe_browsing::IncidentReportUploader::Result result,
      scoped_ptr<safe_browsing::ClientIncidentResponse> response) {
    result_ = result;
    response_ = response.Pass();
  }

 protected:
  IncidentReportUploaderImplTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        result_(safe_browsing::IncidentReportUploader::UPLOAD_REQUEST_FAILED) {}

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  safe_browsing::IncidentReportUploader::Result result_;
  scoped_ptr<safe_browsing::ClientIncidentResponse> response_;
};

TEST_F(IncidentReportUploaderImplTest, Success) {
  safe_browsing::ClientIncidentReport report;
  scoped_ptr<safe_browsing::IncidentReportUploader> instance(
      safe_browsing::IncidentReportUploaderImpl::UploadReport(
          base::Bind(&IncidentReportUploaderImplTest::OnReportUploadResult,
                     base::Unretained(this)),
          NULL,
          report));

  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(
      safe_browsing::IncidentReportUploaderImpl::kTestUrlFetcherId);
  ASSERT_NE(static_cast<net::TestURLFetcher*>(NULL), fetcher);

  safe_browsing::ClientIncidentReport uploaded_report;

  EXPECT_EQ(net::LOAD_DISABLE_CACHE, fetcher->GetLoadFlags());
  EXPECT_TRUE(uploaded_report.ParseFromString(fetcher->upload_data()));

  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::SUCCESS, 0));
  fetcher->set_response_code(net::HTTP_OK);
  std::string response;
  safe_browsing::ClientIncidentResponse().SerializeToString(&response);
  fetcher->SetResponseString(response);

  fetcher->delegate()->OnURLFetchComplete(fetcher);

  EXPECT_EQ(safe_browsing::IncidentReportUploader::UPLOAD_SUCCESS, result_);
  EXPECT_TRUE(response_);
}

// TODO(grt):
// bad status/response code
// confirm data in request is in upload test
// confirm data in response is parsed
