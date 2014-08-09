// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/rpc/http_post.h"

#include "base/test/test_simple_task_runner.h"
#include "components/copresence/proto/data.pb.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kFakeServerHost[] = "test.server.google.com";
const char kRPCName[] = "testRpc";
const char kTracingToken[] = "trace me!";

}  // namespace

using google::protobuf::MessageLite;

namespace copresence {

class HttpPostTest : public testing::Test {
 public:
  HttpPostTest()
      : received_response_code_(0) {
    context_getter_ = new net::TestURLRequestContextGetter(
        make_scoped_refptr(new base::TestSimpleTaskRunner));
    proto_.set_client("test_client");
    proto_.set_version_code(123);
  }
  virtual ~HttpPostTest() {}

  // Record the response sent back to the client for verification.
  void TestResponseCallback(int response_code,
                            const std::string& response) {
    received_response_code_ = response_code;
    received_response_ = response;
  }

 protected:
  bool ResponsePassedThrough(int response_code, const std::string& response) {
    pending_post_ = new HttpPost(context_getter_.get(),
                                 std::string("http://") + kFakeServerHost,
                                 kRPCName,
                                 "",
                                 proto_);
    pending_post_->Start(base::Bind(&HttpPostTest::TestResponseCallback,
                                    base::Unretained(this)));
    net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(
        HttpPost::kUrlFetcherId);
    fetcher->set_response_code(response_code);
    fetcher->SetResponseString(response);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
    delete pending_post_;
    return received_response_code_ == response_code &&
           received_response_ == response;
  }

  net::TestURLFetcherFactory fetcher_factory_;
  scoped_refptr<net::TestURLRequestContextGetter> context_getter_;

  ClientVersion proto_;

  int received_response_code_;
  std::string received_response_;

 private:
  HttpPost* pending_post_;
};

TEST_F(HttpPostTest, OKResponse) {
  // "Send" the proto to the "server".
  HttpPost* post = new HttpPost(context_getter_.get(),
                                std::string("http://") + kFakeServerHost,
                                kRPCName,
                                kTracingToken,
                                proto_);
  post->Start(base::Bind(&HttpPostTest::TestResponseCallback,
                         base::Unretained(this)));

  // Verify that the right data got sent to the right place.
  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(
      HttpPost::kUrlFetcherId);
  EXPECT_EQ(kFakeServerHost, fetcher->GetOriginalURL().host());
  EXPECT_EQ(std::string("/") + kRPCName, fetcher->GetOriginalURL().path());
  std::string tracing_token_sent;
  EXPECT_TRUE(net::GetValueForKeyInQuery(fetcher->GetOriginalURL(),
                                         HttpPost::kTracingTokenField,
                                         &tracing_token_sent));
  EXPECT_EQ(std::string("token:") + kTracingToken, tracing_token_sent);
  std::string upload_data;
  ASSERT_TRUE(proto_.SerializeToString(&upload_data));
  EXPECT_EQ(upload_data, fetcher->upload_data());

  // Send a response and check that it's passed along correctly.
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString("Hello World!");
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(net::HTTP_OK, received_response_code_);
  EXPECT_EQ("Hello World!", received_response_);
  delete post;
}

TEST_F(HttpPostTest, ErrorResponse) {
  EXPECT_TRUE(ResponsePassedThrough(
      net::HTTP_BAD_REQUEST, "Bad client. Shame on you."));
  EXPECT_TRUE(ResponsePassedThrough(
      net::HTTP_INTERNAL_SERVER_ERROR, "I'm dying. Forgive me."));
  EXPECT_TRUE(ResponsePassedThrough(-1, ""));
}

}  // namespace copresence
