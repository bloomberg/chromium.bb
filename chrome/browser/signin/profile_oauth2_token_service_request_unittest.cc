// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/signin/profile_oauth2_token_service_request.h"

#include <set>
#include <string>
#include <vector>
#include "base/threading/thread.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kAccessToken[] = "access_token";
const char kAccountId[] = "test_user@gmail.com";
const char kRefreshToken[] = "refresh_token";

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
  int number_of_successful_tokens_;
  GoogleServiceAuthError last_error_;
  int number_of_errors_;
};

TestingOAuth2TokenServiceConsumer::TestingOAuth2TokenServiceConsumer()
    : number_of_successful_tokens_(0),
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
  ++number_of_successful_tokens_;
}

void TestingOAuth2TokenServiceConsumer::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  last_error_ = error;
  ++number_of_errors_;
}

class ProfileOAuth2TokenServiceRequestTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE;

 protected:
  base::MessageLoop ui_loop_;
  scoped_ptr<content::TestBrowserThread> ui_thread_;

  scoped_ptr<Profile> profile_;
  TestingOAuth2TokenServiceConsumer consumer_;
  FakeProfileOAuth2TokenService* oauth2_service_;

  scoped_ptr<ProfileOAuth2TokenServiceRequest> request_;
};

void ProfileOAuth2TokenServiceRequestTest::SetUp() {
  ui_thread_.reset(new content::TestBrowserThread(content::BrowserThread::UI,
                                                  &ui_loop_));
  TestingProfile::Builder builder;
  builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                            &FakeProfileOAuth2TokenService::Build);
  profile_ = builder.Build();

  oauth2_service_ = (FakeProfileOAuth2TokenService*)
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_.get());
}

TEST_F(ProfileOAuth2TokenServiceRequestTest,
       Failure) {
  scoped_ptr<ProfileOAuth2TokenServiceRequest> request(
      ProfileOAuth2TokenServiceRequest::CreateAndStart(
          profile_.get(),
          std::string(),
          OAuth2TokenService::ScopeSet(),
          &consumer_));
  oauth2_service_->IssueErrorForAllPendingRequests(
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
  ui_loop_.RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
}

TEST_F(ProfileOAuth2TokenServiceRequestTest,
       Success) {
  oauth2_service_->IssueRefreshToken(kRefreshToken);
  scoped_ptr<ProfileOAuth2TokenServiceRequest> request(
      ProfileOAuth2TokenServiceRequest::CreateAndStart(
          profile_.get(),
          kAccountId,
          OAuth2TokenService::ScopeSet(),
          &consumer_));
  oauth2_service_->IssueTokenForAllPendingRequests(kAccessToken,
                                                   base::Time::Max());
  ui_loop_.RunUntilIdle();
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(kAccessToken, consumer_.last_token_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
}

TEST_F(ProfileOAuth2TokenServiceRequestTest,
       RequestDeletionBeforeServiceComplete) {
  oauth2_service_->IssueRefreshToken(kRefreshToken);
  scoped_ptr<ProfileOAuth2TokenServiceRequest> request(
      ProfileOAuth2TokenServiceRequest::CreateAndStart(
          profile_.get(),
          kAccountId,
          OAuth2TokenService::ScopeSet(),
          &consumer_));
  request.reset();
  oauth2_service_->IssueTokenForAllPendingRequests(kAccessToken,
                                                   base::Time::Max());
  ui_loop_.RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
}

TEST_F(ProfileOAuth2TokenServiceRequestTest,
       RequestDeletionAfterServiceComplete) {
  oauth2_service_->IssueRefreshToken(kRefreshToken);
  scoped_ptr<ProfileOAuth2TokenServiceRequest> request(
      ProfileOAuth2TokenServiceRequest::CreateAndStart(
          profile_.get(),
          kAccountId,
          OAuth2TokenService::ScopeSet(),
          &consumer_));
  oauth2_service_->IssueTokenForAllPendingRequests(kAccessToken,
                                                   base::Time::Max());
  ui_loop_.RunUntilIdle();
  request.reset();
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
}

}  // namespace
