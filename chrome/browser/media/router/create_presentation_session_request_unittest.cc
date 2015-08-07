// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/media/router/create_presentation_session_request.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

namespace {

const char kPresentationUrl[] = "http://foo.com";
const char kPresentationId[] = "presentationId";
const char kRouteId[] =
    "urn:x-org.chromium:media:route:presentationId/cast-sink1/http://foo.com";

}  // namespace

class CreatePresentationSessionRequestTest : public ::testing::Test {
 public:
  CreatePresentationSessionRequestTest() : cb_invoked_(false) {}
  ~CreatePresentationSessionRequestTest() override {}

  void OnSuccess(const content::PresentationSessionInfo& expected_info,
                 const content::PresentationSessionInfo& actual_info,
                 const MediaRoute::Id& route_id) {
    cb_invoked_ = true;
    EXPECT_EQ(expected_info.presentation_url, actual_info.presentation_url);
    EXPECT_EQ(expected_info.presentation_id, actual_info.presentation_id);
  }

  void OnError(const content::PresentationError& expected_error,
               const content::PresentationError& actual_error) {
    cb_invoked_ = true;
    EXPECT_EQ(expected_error.error_type, actual_error.error_type);
    EXPECT_EQ(expected_error.message, actual_error.message);
  }

  void FailOnSuccess(const content::PresentationSessionInfo& info,
                     const MediaRoute::Id& route_id) {
    FAIL() << "Success callback should not have been called.";
  }

  void FailOnError(const content::PresentationError& error) {
    FAIL() << "Error should not have been called.";
  }

  bool cb_invoked_;
};

// Test that the object's getters match the constructor parameters.
TEST_F(CreatePresentationSessionRequestTest, Getters) {
  GURL frame_url("http://frameUrl");

  content::PresentationSessionInfo session_info(kPresentationUrl,
                                                kPresentationId);
  content::PresentationError error(content::PRESENTATION_ERROR_UNKNOWN,
                                   "Unknown error.");
  CreatePresentationSessionRequest context(
      kPresentationUrl, kPresentationId, frame_url,
      base::Bind(&CreatePresentationSessionRequestTest::FailOnSuccess,
                 base::Unretained(this)),
      base::Bind(&CreatePresentationSessionRequestTest::OnError,
                 base::Unretained(this), error));
  EXPECT_TRUE(MediaSourceForPresentationUrl(kPresentationUrl)
                  .Equals(context.GetMediaSource()));
  EXPECT_EQ(frame_url, context.frame_url());
  content::PresentationSessionInfo actual_session_info(
      context.presentation_info());
  EXPECT_EQ(kPresentationUrl, actual_session_info.presentation_url);
  EXPECT_EQ(kPresentationId, actual_session_info.presentation_id);
  // Since we didn't explicitly call MaybeInvoke*, the error callback will be
  // invoked when |context| is destroyed.
}

TEST_F(CreatePresentationSessionRequestTest, SuccessCallback) {
  GURL frame_url("http://frameUrl");
  content::PresentationSessionInfo session_info(kPresentationUrl,
                                                kPresentationId);
  CreatePresentationSessionRequest context(
      kPresentationUrl, kPresentationId, frame_url,
      base::Bind(&CreatePresentationSessionRequestTest::OnSuccess,
                 base::Unretained(this), session_info),
      base::Bind(&CreatePresentationSessionRequestTest::FailOnError,
                 base::Unretained(this)));
  context.MaybeInvokeSuccessCallback(kPresentationId, kRouteId);
  // No-op since success callback is already invoked.
  context.MaybeInvokeErrorCallback(content::PresentationError(
      content::PRESENTATION_ERROR_NO_AVAILABLE_SCREENS, "Error message"));
  EXPECT_TRUE(cb_invoked_);
}

TEST_F(CreatePresentationSessionRequestTest, ErrorCallback) {
  GURL frame_url("http://frameUrl");
  content::PresentationSessionInfo session_info(kPresentationUrl,
                                                kPresentationId);
  content::PresentationError error(
      content::PRESENTATION_ERROR_SESSION_REQUEST_CANCELLED,
      "This is an error message");
  CreatePresentationSessionRequest context(
      kPresentationUrl, kPresentationId, frame_url,
      base::Bind(&CreatePresentationSessionRequestTest::FailOnSuccess,
                 base::Unretained(this)),
      base::Bind(&CreatePresentationSessionRequestTest::OnError,
                 base::Unretained(this), error));
  context.MaybeInvokeErrorCallback(error);
  // No-op since error callback is already invoked.
  context.MaybeInvokeSuccessCallback(kPresentationId, kRouteId);
  EXPECT_TRUE(cb_invoked_);
}

}  // namespace media_router
