// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/signin/oauth2_token_service_request.h"

#include <set>
#include <string>
#include <vector>
#include "base/threading/thread.h"
#include "chrome/browser/signin/oauth2_token_service.h"
#include "chrome/browser/signin/oauth2_token_service_factory.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestingOAuth2TokenServiceConsumer : public OAuth2TokenService::Consumer {
 public:
  TestingOAuth2TokenServiceConsumer();
  virtual ~TestingOAuth2TokenServiceConsumer();

  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

  std::string last_token_;
  int number_of_correct_tokens_;
  GoogleServiceAuthError last_error_;
  int number_of_errors_;
};

TestingOAuth2TokenServiceConsumer::TestingOAuth2TokenServiceConsumer()
    : number_of_correct_tokens_(0),
      last_error_(GoogleServiceAuthError::AuthErrorNone()),
      number_of_errors_(0) {
}

TestingOAuth2TokenServiceConsumer::~TestingOAuth2TokenServiceConsumer() {
}

void TestingOAuth2TokenServiceConsumer::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& token,
    const base::Time& expiration_date) {
  last_token_ = token;
  ++number_of_correct_tokens_;
}

void TestingOAuth2TokenServiceConsumer::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  last_error_ = error;
  ++number_of_errors_;
}

class MockOAuth2TokenService : public OAuth2TokenService {
 public:
  class Request : public OAuth2TokenService::Request,
                  public base::SupportsWeakPtr<Request> {
   public:
    Request(OAuth2TokenService::Consumer* consumer,
            GoogleServiceAuthError error,
            std::string access_token);
    virtual ~Request();

    void InformConsumer() const;

   private:
    OAuth2TokenService::Consumer* consumer_;
    GoogleServiceAuthError error_;
    std::string access_token_;
    base::Time expiration_date_;
  };

  MockOAuth2TokenService();
  virtual ~MockOAuth2TokenService();

  virtual scoped_ptr<OAuth2TokenService::Request> StartRequest(
      const std::set<std::string>& scopes,
      OAuth2TokenService::Consumer* consumer) OVERRIDE;

  void SetExpectation(bool success, std::string oauth2_access_token);

 private:
  static void InformConsumer(
      base::WeakPtr<MockOAuth2TokenService::Request> request);

  bool success_;
  std::string oauth2_access_token_;
};

MockOAuth2TokenService::Request::Request(
    OAuth2TokenService::Consumer* consumer,
    GoogleServiceAuthError error,
    std::string access_token)
    : consumer_(consumer),
      error_(error),
      access_token_(access_token) {
}

MockOAuth2TokenService::Request::~Request() {
}

void MockOAuth2TokenService::Request::InformConsumer() const {
  if (error_.state() == GoogleServiceAuthError::NONE)
    consumer_->OnGetTokenSuccess(this, access_token_, expiration_date_);
  else
    consumer_->OnGetTokenFailure(this, error_);
}

MockOAuth2TokenService::MockOAuth2TokenService()
    : success_(true),
      oauth2_access_token_(std::string("success token")) {
}

MockOAuth2TokenService::~MockOAuth2TokenService() {
}

void MockOAuth2TokenService::SetExpectation(bool success,
                                            std::string oauth2_access_token) {
  success_ = success;
  oauth2_access_token_ = oauth2_access_token;
}

// static
void MockOAuth2TokenService::InformConsumer(
    base::WeakPtr<MockOAuth2TokenService::Request> request) {
  if (request)
    request->InformConsumer();
}

scoped_ptr<OAuth2TokenService::Request> MockOAuth2TokenService::StartRequest(
    const std::set<std::string>& scopes,
    OAuth2TokenService::Consumer* consumer) {
  scoped_ptr<Request> request;
  if (success_) {
    request.reset(new MockOAuth2TokenService::Request(
        consumer,
        GoogleServiceAuthError(GoogleServiceAuthError::NONE),
        oauth2_access_token_));
  } else {
    request.reset(new MockOAuth2TokenService::Request(
        consumer,
        GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE),
        std::string()));
  }
  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
    &MockOAuth2TokenService::InformConsumer, request->AsWeakPtr()));
  return request.PassAs<OAuth2TokenService::Request>();
}

static ProfileKeyedService* CreateOAuth2TokenService(Profile* profile) {
  return new MockOAuth2TokenService();
}

class OAuth2TokenServiceRequestTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE;

 protected:
  MessageLoop ui_loop_;
  scoped_ptr<content::TestBrowserThread> ui_thread_;

  scoped_ptr<Profile> profile_;
  TestingOAuth2TokenServiceConsumer consumer_;
  MockOAuth2TokenService* oauth2_service_;

  scoped_ptr<OAuth2TokenServiceRequest> request_;
};

void OAuth2TokenServiceRequestTest::SetUp() {
  ui_thread_.reset(new content::TestBrowserThread(content::BrowserThread::UI,
                                                  &ui_loop_));
  profile_.reset(new TestingProfile());
  OAuth2TokenServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), &CreateOAuth2TokenService);
  oauth2_service_ = (MockOAuth2TokenService*)
      OAuth2TokenServiceFactory::GetForProfile(profile_.get());
}

TEST_F(OAuth2TokenServiceRequestTest,
       Failure) {
  oauth2_service_->SetExpectation(false, std::string());
  scoped_ptr<OAuth2TokenServiceRequest> request(
      OAuth2TokenServiceRequest::CreateAndStart(
          profile_.get(),
          std::set<std::string>(),
          &consumer_));
  ui_loop_.RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceRequestTest,
       Success) {
  scoped_ptr<OAuth2TokenServiceRequest> request(
      OAuth2TokenServiceRequest::CreateAndStart(
          profile_.get(),
          std::set<std::string>(),
          &consumer_));
  ui_loop_.RunUntilIdle();
  EXPECT_EQ(1, consumer_.number_of_correct_tokens_);
  EXPECT_EQ("success token", consumer_.last_token_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceRequestTest,
       RequestDeletionBeforeServiceComplete) {
  scoped_ptr<OAuth2TokenServiceRequest> request(
      OAuth2TokenServiceRequest::CreateAndStart(
          profile_.get(),
          std::set<std::string>(),
          &consumer_));
  request.reset();
  ui_loop_.RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceRequestTest,
       RequestDeletionAfterServiceComplete) {
  scoped_ptr<OAuth2TokenServiceRequest> request(
      OAuth2TokenServiceRequest::CreateAndStart(
          profile_.get(),
          std::set<std::string>(),
          &consumer_));
  ui_loop_.RunUntilIdle();
  request.reset();
  EXPECT_EQ(1, consumer_.number_of_correct_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
}

}  // namespace
