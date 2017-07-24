// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/create_presentation_connection_request.h"

#include "base/bind.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/media_source_helper.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

namespace {

constexpr char kPresentationUrl1[] = "http://www.example.com/presentation.html";
constexpr char kPresentationUrl2[] = "http://www.example.net/alternate.html";
constexpr char kFrameUrl[] = "http://google.com";
constexpr char kPresentationId[] = "presentationId";
constexpr char kRouteId[] =
    "urn:x-org.chromium:media:route:presentationId/cast-sink1/"
    "http://www.example.com/presentation.html";

}  // namespace

class CreatePresentationConnectionRequestTest : public ::testing::Test {
 public:
  CreatePresentationConnectionRequestTest()
      : cb_invoked_(false),
        presentation_url_(kPresentationUrl1),
        presentation_request_({1, 2},
                              {presentation_url_, GURL(kPresentationUrl2)},
                              url::Origin(GURL(kFrameUrl))) {}

  ~CreatePresentationConnectionRequestTest() override {}

  void OnSuccess(const content::PresentationInfo& expected_info,
                 const content::PresentationInfo& actual_info,
                 const MediaRoute& route) {
    cb_invoked_ = true;
    EXPECT_EQ(expected_info.presentation_url, actual_info.presentation_url);
    EXPECT_EQ(expected_info.presentation_id, actual_info.presentation_id);
    EXPECT_EQ(route.media_route_id(), kRouteId);
  }

  void OnError(const content::PresentationError& expected_error,
               const content::PresentationError& actual_error) {
    cb_invoked_ = true;
    EXPECT_EQ(expected_error.error_type, actual_error.error_type);
    EXPECT_EQ(expected_error.message, actual_error.message);
  }

  void FailOnSuccess(const content::PresentationInfo& info,
                     const MediaRoute& route) {
    FAIL() << "Success callback should not have been called.";
  }

  void FailOnError(const content::PresentationError& error) {
    FAIL() << "Error should not have been called.";
  }

  bool cb_invoked_;
  GURL presentation_url_;
  content::PresentationRequest presentation_request_;
};

TEST_F(CreatePresentationConnectionRequestTest, ErrorCallbackInvokedByDefault) {
  content::PresentationError error(content::PRESENTATION_ERROR_UNKNOWN,
                                   "Unknown error.");
  CreatePresentationConnectionRequest request(
      presentation_request_,
      base::BindOnce(&CreatePresentationConnectionRequestTest::FailOnSuccess,
                     base::Unretained(this)),
      base::BindOnce(&CreatePresentationConnectionRequestTest::OnError,
                     base::Unretained(this), error));

  // Since we didn't explicitly call Invoke*, the error callback will be
  // invoked when |request| is destroyed.
}

TEST_F(CreatePresentationConnectionRequestTest, SuccessCallback) {
  content::PresentationInfo presentation_info(presentation_url_,
                                              kPresentationId);
  CreatePresentationConnectionRequest request(
      presentation_request_,
      base::BindOnce(&CreatePresentationConnectionRequestTest::OnSuccess,
                     base::Unretained(this), presentation_info),
      base::BindOnce(&CreatePresentationConnectionRequestTest::FailOnError,
                     base::Unretained(this)));
  MediaRoute route(kRouteId, MediaSourceForTab(1), "sinkId", "Description",
                   false, "", false);
  request.InvokeSuccessCallback(kPresentationId, presentation_url_, route);
  EXPECT_TRUE(cb_invoked_);
}

TEST_F(CreatePresentationConnectionRequestTest, ErrorCallback) {
  content::PresentationError error(
      content::PRESENTATION_ERROR_PRESENTATION_REQUEST_CANCELLED,
      "This is an error message");
  CreatePresentationConnectionRequest request(
      presentation_request_,
      base::BindOnce(&CreatePresentationConnectionRequestTest::FailOnSuccess,
                     base::Unretained(this)),
      base::BindOnce(&CreatePresentationConnectionRequestTest::OnError,
                     base::Unretained(this), error));
  request.InvokeErrorCallback(error);
  EXPECT_TRUE(cb_invoked_);
}

}  // namespace media_router
