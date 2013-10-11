// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/request_sender.h"

#include "chrome/browser/google_apis/base_requests.h"
#include "chrome/browser/google_apis/dummy_auth_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

class RequestSenderTest : public testing::Test {
 protected:
  RequestSenderTest()
     : request_sender_(new DummyAuthService, NULL, NULL, "dummy-user-agent") {
  }

  RequestSender request_sender_;
};

// Minimal implementation for AuthenticatedRequestInterface that can interact
// with RequestSender correctly.
class TestRequest : public AuthenticatedRequestInterface {
 public:
  TestRequest(RequestSender* sender, bool* start_called, bool* cancel_called)
      : sender_(sender),
        start_called_(start_called),
        cancel_called_(cancel_called),
        weak_ptr_factory_(this) {
  }

  // Test the situation that the request has finished.
  void FinishRequest() {
    sender_->RequestFinished(this);
  }

  virtual void Start(const std::string& access_token,
                     const std::string& custom_user_agent,
                     const ReAuthenticateCallback& callback) OVERRIDE {
    EXPECT_FALSE(*start_called_);

    *start_called_ = true;
  }

  virtual void Cancel() OVERRIDE {
    EXPECT_TRUE(*start_called_);

    *cancel_called_ = true;
    sender_->RequestFinished(this);
  }

  virtual void OnAuthFailed(GDataErrorCode code) OVERRIDE {}

  virtual base::WeakPtr<AuthenticatedRequestInterface> GetWeakPtr() OVERRIDE {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  RequestSender* sender_;
  bool* start_called_;
  bool* cancel_called_;
  base::WeakPtrFactory<TestRequest> weak_ptr_factory_;
};

}  // namespace

TEST_F(RequestSenderTest, StartAndFinishRequest) {
  bool start_called  = false;
  bool cancel_called = false;
  TestRequest* request = new TestRequest(&request_sender_,
                                         &start_called,
                                         &cancel_called);
  base::WeakPtr<AuthenticatedRequestInterface> weak_ptr = request->GetWeakPtr();

  base::Closure cancel_closure = request_sender_.StartRequestWithRetry(request);
  EXPECT_TRUE(!cancel_closure.is_null());
  EXPECT_TRUE(start_called);

  request->FinishRequest();
  EXPECT_FALSE(weak_ptr);  // The request object is deleted.

  // It is safe to run the cancel closure even after the request is finished.
  // It is just no-op. The TestRequest::Cancel method is not called.
  cancel_closure.Run();
  EXPECT_FALSE(cancel_called);
}

TEST_F(RequestSenderTest, StartAndCancelRequest) {
  bool start_called  = false;
  bool cancel_called = false;
  TestRequest* request = new TestRequest(&request_sender_,
                                         &start_called,
                                         &cancel_called);
  base::WeakPtr<AuthenticatedRequestInterface> weak_ptr = request->GetWeakPtr();

  base::Closure cancel_closure = request_sender_.StartRequestWithRetry(request);
  EXPECT_TRUE(!cancel_closure.is_null());
  EXPECT_TRUE(start_called);

  cancel_closure.Run();
  EXPECT_TRUE(cancel_called);  // Cancel is called.
  EXPECT_FALSE(weak_ptr);  // The request object is deleted.
}

}  // namespace google_apis
