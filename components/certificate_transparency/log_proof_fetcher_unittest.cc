// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/log_proof_fetcher.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "components/safe_json/testing_json_parser.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"
#include "net/cert/signed_tree_head.h"
#include "net/http/http_status_code.h"
#include "net/test/ct_test_util.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace certificate_transparency {

namespace {

const char kGetSTHHeaders[] =
    "HTTP/1.1 200 OK\n"
    "Content-Type: application/json; charset=ISO-8859-1\n";

const char kGetSTHNotFoundHeaders[] =
    "HTTP/1.1 404 Not Found\n"
    "Content-Type: text/html; charset=iso-8859-1\n";

const char kLogSchema[] = "https";
const char kLogHost[] = "ct.log.example.com";
const char kLogPathPrefix[] = "somelog";
const char kLogID[] = "some_id";

class FetchSTHTestJob : public net::URLRequestTestJob {
 public:
  FetchSTHTestJob(const std::string& get_sth_data,
                  const std::string& get_sth_headers,
                  net::URLRequest* request,
                  net::NetworkDelegate* network_delegate)
      : URLRequestTestJob(request,
                          network_delegate,
                          get_sth_headers,
                          get_sth_data,
                          true),
        async_io_(false) {}

  void set_async_io(bool async_io) { async_io_ = async_io; }

 private:
  ~FetchSTHTestJob() override {}

  bool NextReadAsync() override {
    // Response with indication of async IO only once, otherwise the final
    // Read would (incorrectly) be classified as async, causing the
    // URLRequestJob to try reading another time and failing on a CHECK
    // that the raw_read_buffer_ is not null.
    // According to mmenke@, this is a bug in the URLRequestTestJob code.
    // TODO(eranm): Once said bug is fixed, switch most tests to using async
    // IO.
    if (async_io_) {
      async_io_ = false;
      return true;
    }
    return false;
  }

  bool async_io_;

  DISALLOW_COPY_AND_ASSIGN(FetchSTHTestJob);
};

class GetSTHResponseHandler : public net::URLRequestInterceptor {
 public:
  GetSTHResponseHandler()
      : async_io_(false),
        response_body_(""),
        response_headers_(
            std::string(kGetSTHHeaders, arraysize(kGetSTHHeaders))) {}
  ~GetSTHResponseHandler() override {}

  // URLRequestInterceptor implementation:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    std::string expected_url = base::StringPrintf(
        "%s://%s/%s/ct/v1/get-sth", kLogSchema, kLogHost, kLogPathPrefix);
    EXPECT_EQ(GURL(expected_url), request->url());
    FetchSTHTestJob* job = new FetchSTHTestJob(
        response_body_, response_headers_, request, network_delegate);
    job->set_async_io(async_io_);
    return job;
  }

  void set_response_body(const std::string& response_body) {
    response_body_ = response_body;
  }

  void set_response_headers(const std::string& response_headers) {
    response_headers_ = response_headers;
  }

  void set_async_io(bool async_io) { async_io_ = async_io; }

 private:
  bool async_io_;
  std::string response_body_;
  std::string response_headers_;

  DISALLOW_COPY_AND_ASSIGN(GetSTHResponseHandler);
};

class RecordFetchCallbackInvocations {
 public:
  RecordFetchCallbackInvocations(bool expect_success)
      : expect_success_(expect_success),
        invoked_(false),
        net_error_(net::OK),
        http_response_code_(-1) {}

  void STHFetched(const std::string& log_id,
                  const net::ct::SignedTreeHead& sth) {
    ASSERT_TRUE(expect_success_);
    ASSERT_FALSE(invoked_);
    invoked_ = true;
    // If expected to succeed, expecting the known_good STH.
    net::ct::SignedTreeHead expected_sth;
    net::ct::GetSampleSignedTreeHead(&expected_sth);

    EXPECT_EQ(kLogID, log_id);
    EXPECT_EQ(expected_sth.version, sth.version);
    EXPECT_EQ(expected_sth.timestamp, sth.timestamp);
    EXPECT_EQ(expected_sth.tree_size, sth.tree_size);
    EXPECT_STREQ(expected_sth.sha256_root_hash, sth.sha256_root_hash);
    EXPECT_EQ(expected_sth.signature.hash_algorithm,
              sth.signature.hash_algorithm);
    EXPECT_EQ(expected_sth.signature.signature_algorithm,
              sth.signature.signature_algorithm);
    EXPECT_EQ(expected_sth.signature.signature_data,
              sth.signature.signature_data);
  }

  void FetchingFailed(const std::string& log_id,
                      int net_error,
                      int http_response_code) {
    ASSERT_FALSE(expect_success_);
    ASSERT_FALSE(invoked_);
    invoked_ = true;
    net_error_ = net_error;
    http_response_code_ = http_response_code;
    if (net_error_ == net::OK) {
      EXPECT_NE(net::HTTP_OK, http_response_code_);
    }
  }

  bool invoked() const { return invoked_; }

  int net_error() const { return net_error_; }

  int http_response_code() const { return http_response_code_; }

 private:
  const bool expect_success_;
  bool invoked_;
  int net_error_;
  int http_response_code_;
};

class LogProofFetcherTest : public ::testing::Test {
 public:
  LogProofFetcherTest()
      : log_url_(base::StringPrintf("%s://%s/%s/",
                                    kLogSchema,
                                    kLogHost,
                                    kLogPathPrefix)) {
    scoped_ptr<GetSTHResponseHandler> handler(new GetSTHResponseHandler());
    handler_ = handler.get();

    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        kLogSchema, kLogHost, handler.Pass());

    fetcher_.reset(new LogProofFetcher(&context_));
  }

  ~LogProofFetcherTest() override {
    net::URLRequestFilter::GetInstance()->RemoveHostnameHandler(kLogSchema,
                                                                kLogHost);
  }

 protected:
  void SetValidSTHJSONResponse() {
    std::string sth_json_reply_data = net::ct::GetSampleSTHAsJson();
    handler_->set_response_body(sth_json_reply_data);
  }

  void RunFetcherWithCallback(RecordFetchCallbackInvocations* callback) {
    fetcher_->FetchSignedTreeHead(
        log_url_, kLogID,
        base::Bind(&RecordFetchCallbackInvocations::STHFetched,
                   base::Unretained(callback)),
        base::Bind(&RecordFetchCallbackInvocations::FetchingFailed,
                   base::Unretained(callback)));
    message_loop_.RunUntilIdle();
  }

  base::MessageLoopForIO message_loop_;
  net::TestURLRequestContext context_;
  safe_json::TestingJsonParser::ScopedFactoryOverride factory_override_;
  scoped_ptr<LogProofFetcher> fetcher_;
  const GURL log_url_;
  GetSTHResponseHandler* handler_;
};

TEST_F(LogProofFetcherTest, TestValidGetReply) {
  SetValidSTHJSONResponse();

  RecordFetchCallbackInvocations callback(true);

  RunFetcherWithCallback(&callback);

  ASSERT_TRUE(callback.invoked());
}

TEST_F(LogProofFetcherTest, TestValidGetReplyAsyncIO) {
  SetValidSTHJSONResponse();
  handler_->set_async_io(true);

  RecordFetchCallbackInvocations callback(true);
  RunFetcherWithCallback(&callback);

  ASSERT_TRUE(callback.invoked());
}

TEST_F(LogProofFetcherTest, TestInvalidGetReplyIncompleteJSON) {
  std::string sth_json_reply_data = net::ct::CreateSignedTreeHeadJsonString(
      21 /* tree_size */, 123456u /* timestamp */, std::string(),
      std::string());
  handler_->set_response_body(sth_json_reply_data);

  RecordFetchCallbackInvocations callback(false);
  RunFetcherWithCallback(&callback);

  ASSERT_TRUE(callback.invoked());
  EXPECT_EQ(net::ERR_CT_STH_INCOMPLETE, callback.net_error());
}

TEST_F(LogProofFetcherTest, TestInvalidGetReplyInvalidJSON) {
  std::string sth_json_reply_data = "{\"tree_size\":21,\"timestamp\":}";
  handler_->set_response_body(sth_json_reply_data);

  RecordFetchCallbackInvocations callback(false);
  RunFetcherWithCallback(&callback);

  ASSERT_TRUE(callback.invoked());
  EXPECT_EQ(net::ERR_CT_STH_PARSING_FAILED, callback.net_error());
}

TEST_F(LogProofFetcherTest, TestLogReplyIsTooLong) {
  std::string sth_json_reply_data = net::ct::GetSampleSTHAsJson();
  // Add kMaxLogResponseSizeInBytes to make sure the response is too big.
  sth_json_reply_data.append(
      std::string(LogProofFetcher::kMaxLogResponseSizeInBytes, ' '));
  handler_->set_response_body(sth_json_reply_data);

  RecordFetchCallbackInvocations callback(false);
  RunFetcherWithCallback(&callback);

  ASSERT_TRUE(callback.invoked());
  EXPECT_EQ(net::ERR_FILE_TOO_BIG, callback.net_error());
  EXPECT_EQ(net::HTTP_OK, callback.http_response_code());
}

TEST_F(LogProofFetcherTest, TestLogReplyIsExactlyMaxSize) {
  std::string sth_json_reply_data = net::ct::GetSampleSTHAsJson();
  // Extend the reply to be exactly kMaxLogResponseSizeInBytes.
  sth_json_reply_data.append(std::string(
      LogProofFetcher::kMaxLogResponseSizeInBytes - sth_json_reply_data.size(),
      ' '));
  handler_->set_response_body(sth_json_reply_data);

  RecordFetchCallbackInvocations callback(true);
  RunFetcherWithCallback(&callback);

  ASSERT_TRUE(callback.invoked());
}

TEST_F(LogProofFetcherTest, TestLogRepliesWithHttpError) {
  handler_->set_response_headers(
      std::string(kGetSTHNotFoundHeaders, arraysize(kGetSTHNotFoundHeaders)));

  RecordFetchCallbackInvocations callback(false);
  RunFetcherWithCallback(&callback);

  ASSERT_TRUE(callback.invoked());
  EXPECT_EQ(net::OK, callback.net_error());
  EXPECT_EQ(net::HTTP_NOT_FOUND, callback.http_response_code());
}

}  // namespace

}  // namespace certificate_transparency
