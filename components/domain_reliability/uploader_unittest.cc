// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/uploader.h"

#include <algorithm>
#include <cstring>
#include <map>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/test/test_simple_task_runner.h"
#include "components/domain_reliability/test_util.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace domain_reliability {
namespace {

const char *kReportJSON = "{}";
const char *kUploadURL = "https://example/upload";

class DomainReliabilityUploaderTest : public testing::Test {
 protected:
  DomainReliabilityUploaderTest()
      : network_task_runner_(new base::TestSimpleTaskRunner()),
        url_request_context_getter_(new net::TestURLRequestContextGetter(
            network_task_runner_)),
        uploader_(DomainReliabilityUploader::Create(
            &time_, url_request_context_getter_)) {
    uploader_->set_discard_uploads(false);
  }

  DomainReliabilityUploader::UploadCallback MakeUploadCallback(size_t index) {
    return base::Bind(&DomainReliabilityUploaderTest::OnUploadComplete,
                      base::Unretained(this),
                      index);
  }

  void OnUploadComplete(size_t index,
                        const DomainReliabilityUploader::UploadResult& result) {
    EXPECT_FALSE(upload_complete_[index]);
    upload_complete_[index] = true;
    upload_result_[index] = result;
  }

  void SimulateUpload() {
    uploader_->UploadReport(kReportJSON,
                            GURL(kUploadURL),
                            MakeUploadCallback(0));
  }

  void SimulateUploadRequest() {
    net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
    EXPECT_TRUE(fetcher);

    EXPECT_EQ(kReportJSON, fetcher->upload_data());
    EXPECT_EQ(GURL(kUploadURL), fetcher->GetOriginalURL());
    EXPECT_TRUE(fetcher->GetLoadFlags() & net::LOAD_DO_NOT_SAVE_COOKIES);
    EXPECT_TRUE(fetcher->GetLoadFlags() & net::LOAD_DO_NOT_SEND_COOKIES);

    fetcher->set_url(GURL(kUploadURL));
  }

  void SimulateNoUploadRequest() {
    net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
    EXPECT_FALSE(fetcher);
  }

  void SimulateUploadResponse(int response_code, const char* headers) {
    net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
    EXPECT_TRUE(fetcher);

    fetcher->set_status(net::URLRequestStatus());
    fetcher->set_response_code(response_code);
    fetcher->set_response_headers(new net::HttpResponseHeaders(
        net::HttpUtil::AssembleRawHeaders(headers, strlen(headers))));
    fetcher->SetResponseString("");
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  void SimulateUploadError(int error) {
    net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
    EXPECT_TRUE(fetcher);

    net::URLRequestStatus status;
    status.set_status(net::URLRequestStatus::FAILED);
    status.set_error(error);
    fetcher->set_status(status);
    fetcher->set_response_code(-1);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  scoped_refptr<base::TestSimpleTaskRunner> network_task_runner_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  scoped_refptr<net::TestURLRequestContextGetter> url_request_context_getter_;
  MockTime time_;
  scoped_ptr<DomainReliabilityUploader> uploader_;

  // Whether the upload callback was called for a particular collector index.
  std::map<size_t, bool> upload_complete_;
  // The result of the last completed upload for a particular collector index.
  std::map<size_t, DomainReliabilityUploader::UploadResult> upload_result_;
};

TEST_F(DomainReliabilityUploaderTest, Create) {
  SimulateNoUploadRequest();
}

TEST_F(DomainReliabilityUploaderTest, SuccessfulUpload) {
  SimulateUpload();
  SimulateUploadRequest();
  EXPECT_FALSE(upload_complete_[0]);
  SimulateUploadResponse(200, "HTTP/1.1 200 OK\n\n");
  EXPECT_TRUE(upload_complete_[0]);
  EXPECT_TRUE(upload_result_[0].is_success());
}

TEST_F(DomainReliabilityUploaderTest, NetworkFailedUpload) {
  SimulateUpload();
  SimulateUploadRequest();
  EXPECT_FALSE(upload_complete_[0]);
  SimulateUploadError(net::ERR_CONNECTION_REFUSED);
  EXPECT_TRUE(upload_complete_[0]);
  EXPECT_TRUE(upload_result_[0].is_failure());
}

TEST_F(DomainReliabilityUploaderTest, ServerFailedUpload) {
  SimulateUpload();
  SimulateUploadRequest();
  EXPECT_FALSE(upload_complete_[0]);
  SimulateUploadResponse(500, "HTTP/1.1 500 Blargh\n\n");
  EXPECT_TRUE(upload_complete_[0]);
  EXPECT_TRUE(upload_result_[0].is_failure());
}

TEST_F(DomainReliabilityUploaderTest, RetryAfterUpload) {
  SimulateUpload();
  SimulateUploadRequest();
  EXPECT_FALSE(upload_complete_[0]);
  SimulateUploadResponse(503, "HTTP/1.1 503 Ugh\nRetry-After: 3600\n\n");
  EXPECT_TRUE(upload_complete_[0]);
  EXPECT_TRUE(upload_result_[0].is_retry_after());
  EXPECT_EQ(base::TimeDelta::FromSeconds(3600), upload_result_[0].retry_after);
}

TEST_F(DomainReliabilityUploaderTest, DiscardedUpload) {
  uploader_->set_discard_uploads(true);
  SimulateUpload();
  SimulateNoUploadRequest();
}

}  // namespace
}  // namespace domain_reliability
