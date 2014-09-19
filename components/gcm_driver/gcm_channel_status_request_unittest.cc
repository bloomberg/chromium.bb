// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "components/gcm_driver/gcm_channel_status_request.h"
#include "components/gcm_driver/proto/gcm_channel_status.pb.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

class GCMChannelStatusRequestTest : public testing::Test {
 public:
  GCMChannelStatusRequestTest();
  virtual ~GCMChannelStatusRequestTest();

 protected:
  enum GCMStatus {
    NOT_SPECIFIED,
    GCM_ENABLED,
    GCM_DISABLED,
  };

  void StartRequest();
  void SetResponseStatusAndString(net::HttpStatusCode status_code,
                                  const std::string& response_body);
  void SetResponseProtoData(GCMStatus status, int poll_interval_seconds);
  void CompleteFetch();
  void OnRequestCompleted(bool enabled, int poll_interval_seconds);

  scoped_ptr<GCMChannelStatusRequest> request_;
  base::MessageLoop message_loop_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  scoped_refptr<net::TestURLRequestContextGetter> url_request_context_getter_;
  bool request_callback_invoked_;
  bool enabled_;
  int poll_interval_seconds_;
};

GCMChannelStatusRequestTest::GCMChannelStatusRequestTest()
    : url_request_context_getter_(new net::TestURLRequestContextGetter(
           message_loop_.message_loop_proxy())),
      request_callback_invoked_(false),
      enabled_(true),
      poll_interval_seconds_(0) {
}

GCMChannelStatusRequestTest::~GCMChannelStatusRequestTest() {
}

void GCMChannelStatusRequestTest::StartRequest() {
  request_.reset(new GCMChannelStatusRequest(
      url_request_context_getter_.get(),
      base::Bind(&GCMChannelStatusRequestTest::OnRequestCompleted,
                 base::Unretained(this))));
  request_->Start();
}

void GCMChannelStatusRequestTest::SetResponseStatusAndString(
    net::HttpStatusCode status_code,
    const std::string& response_body) {
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(status_code);
  fetcher->SetResponseString(response_body);
}

void GCMChannelStatusRequestTest::SetResponseProtoData(
    GCMStatus status, int poll_interval_seconds) {
  gcm_proto::ExperimentStatusResponse response_proto;
  if (status != NOT_SPECIFIED)
    response_proto.mutable_gcm_channel()->set_enabled(status == GCM_ENABLED);

  // Zero |poll_interval_seconds| means the optional field is not set.
  if (poll_interval_seconds)
    response_proto.set_poll_interval_seconds(poll_interval_seconds);

  std::string response_string;
  response_proto.SerializeToString(&response_string);
  SetResponseStatusAndString(net::HTTP_OK, response_string);
}

void GCMChannelStatusRequestTest::CompleteFetch() {
  request_callback_invoked_ = false;
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

void GCMChannelStatusRequestTest::OnRequestCompleted(
    bool enabled, int poll_interval_seconds) {
  request_callback_invoked_ = true;
  enabled_ = enabled;
  poll_interval_seconds_ = poll_interval_seconds;
}

TEST_F(GCMChannelStatusRequestTest, ResponseHttpStatusNotOK) {
  StartRequest();
  SetResponseStatusAndString(net::HTTP_UNAUTHORIZED, "");
  CompleteFetch();

  EXPECT_FALSE(request_callback_invoked_);
}

TEST_F(GCMChannelStatusRequestTest, ResponseEmpty) {
  StartRequest();
  SetResponseStatusAndString(net::HTTP_OK, "");
  CompleteFetch();

  EXPECT_FALSE(request_callback_invoked_);
}

TEST_F(GCMChannelStatusRequestTest, ResponseNotInProtoFormat) {
  StartRequest();
  SetResponseStatusAndString(net::HTTP_OK, "foo");
  CompleteFetch();

  EXPECT_FALSE(request_callback_invoked_);
}

TEST_F(GCMChannelStatusRequestTest, ResponseEmptyProtoData) {
  StartRequest();
  SetResponseProtoData(NOT_SPECIFIED, 0);
  CompleteFetch();

  EXPECT_FALSE(request_callback_invoked_);
}

TEST_F(GCMChannelStatusRequestTest, ResponseWithDisabledStatus) {
  StartRequest();
  SetResponseProtoData(GCM_DISABLED, 0);
  CompleteFetch();

  EXPECT_TRUE(request_callback_invoked_);
  EXPECT_FALSE(enabled_);
  EXPECT_EQ(
      GCMChannelStatusRequest::default_poll_interval_seconds(),
      poll_interval_seconds_);
}

TEST_F(GCMChannelStatusRequestTest, ResponseWithEnabledStatus) {
  StartRequest();
  SetResponseProtoData(GCM_ENABLED, 0);
  CompleteFetch();

  EXPECT_TRUE(request_callback_invoked_);
  EXPECT_TRUE(enabled_);
  EXPECT_EQ(
      GCMChannelStatusRequest::default_poll_interval_seconds(),
      poll_interval_seconds_);
}

TEST_F(GCMChannelStatusRequestTest, ResponseWithPollInterval) {
  // Setting a poll interval 15 minutes longer than the minimum interval we
  // enforce.
  int poll_interval_seconds =
      GCMChannelStatusRequest::min_poll_interval_seconds() + 15 * 60;
  StartRequest();
  SetResponseProtoData(NOT_SPECIFIED, poll_interval_seconds);
  CompleteFetch();

  EXPECT_TRUE(request_callback_invoked_);
  EXPECT_TRUE(enabled_);
  EXPECT_EQ(poll_interval_seconds, poll_interval_seconds_);
}

TEST_F(GCMChannelStatusRequestTest, ResponseWithShortPollInterval) {
  // Setting a poll interval 15 minutes shorter than the minimum interval we
  // enforce.
  int poll_interval_seconds =
      GCMChannelStatusRequest::min_poll_interval_seconds() - 15 * 60;
  StartRequest();
  SetResponseProtoData(NOT_SPECIFIED, poll_interval_seconds);
  CompleteFetch();

  EXPECT_TRUE(request_callback_invoked_);
  EXPECT_TRUE(enabled_);
  EXPECT_EQ(GCMChannelStatusRequest::min_poll_interval_seconds(),
            poll_interval_seconds_);
}

TEST_F(GCMChannelStatusRequestTest, ResponseWithDisabledStatusAndPollInterval) {
  int poll_interval_seconds =
      GCMChannelStatusRequest::min_poll_interval_seconds() + 15 * 60;
  StartRequest();
  SetResponseProtoData(GCM_DISABLED, poll_interval_seconds);
  CompleteFetch();

  EXPECT_TRUE(request_callback_invoked_);
  EXPECT_FALSE(enabled_);
  EXPECT_EQ(poll_interval_seconds, poll_interval_seconds_);
}

}  // namespace gcm
