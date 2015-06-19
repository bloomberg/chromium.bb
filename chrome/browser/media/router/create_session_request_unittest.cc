// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/media/router/create_session_request.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

namespace {

const char kPresentationUrl[] = "http://fooUrl";
const char kPresentationId[] = "presentationId";

}  // namespace

class CreateSessionRequestTest : public ::testing::Test {
 public:
  CreateSessionRequestTest() : cb_invoked_(false) {}
  ~CreateSessionRequestTest() override {}

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
TEST_F(CreateSessionRequestTest, Getters) {
  GURL frame_url("http://frameUrl");

  content::PresentationSessionInfo session_info(kPresentationUrl,
                                                kPresentationId);

  CreateSessionRequest context(
      kPresentationUrl, kPresentationId, frame_url,
      CreateSessionRequest::PresentationSessionSuccessCallback(),
      CreateSessionRequest::PresentationSessionErrorCallback());
  EXPECT_TRUE(MediaSourceForPresentationUrl(kPresentationUrl)
                  .Equals(context.GetMediaSource()));
  EXPECT_EQ(frame_url, context.frame_url());
  content::PresentationSessionInfo actual_session_info(
      context.presentation_info());
  EXPECT_EQ(kPresentationUrl, actual_session_info.presentation_url);
  EXPECT_EQ(kPresentationId, actual_session_info.presentation_id);
}

TEST_F(CreateSessionRequestTest, SuccessCallback) {
  GURL frame_url("http://frameUrl");
  content::PresentationSessionInfo session_info(kPresentationUrl,
                                                kPresentationId);
  CreateSessionRequest context(
      kPresentationUrl, kPresentationId, frame_url,
      base::Bind(&CreateSessionRequestTest::OnSuccess, base::Unretained(this),
                 session_info),
      base::Bind(&CreateSessionRequestTest::FailOnError,
                 base::Unretained(this)));
  context.MaybeInvokeSuccessCallback("routeid");
  // No-op since success callback is already invoked.
  context.MaybeInvokeErrorCallback(content::PresentationError(
      content::PRESENTATION_ERROR_NO_AVAILABLE_SCREENS, "Error message"));
  EXPECT_TRUE(cb_invoked_);
}

TEST_F(CreateSessionRequestTest, ErrorCallback) {
  GURL frame_url("http://frameUrl");
  content::PresentationSessionInfo session_info(kPresentationUrl,
                                                kPresentationId);
  content::PresentationError error(
      content::PRESENTATION_ERROR_SESSION_REQUEST_CANCELLED,
      "This is an error message");
  CreateSessionRequest context(
      kPresentationUrl, kPresentationId, frame_url,
      base::Bind(&CreateSessionRequestTest::FailOnSuccess,
                 base::Unretained(this)),
      base::Bind(&CreateSessionRequestTest::OnError, base::Unretained(this),
                 error));
  context.MaybeInvokeErrorCallback(error);
  // No-op since error callback is already invoked.
  context.MaybeInvokeSuccessCallback("routeid");
  EXPECT_TRUE(cb_invoked_);
}

}  // namespace media_router
