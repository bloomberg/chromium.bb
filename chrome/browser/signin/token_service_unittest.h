// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a unit test harness for the profile's token service.

#ifndef CHROME_BROWSER_SIGNIN_TOKEN_SERVICE_UNITTEST_H_
#define CHROME_BROWSER_SIGNIN_TOKEN_SERVICE_UNITTEST_H_

#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_notification_tracker.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "testing/gtest/include/gtest/gtest.h"

class FakeSigninManagerBase;

class TokenServiceTestHarness : public testing::Test {
 protected:
  TokenServiceTestHarness();
  virtual ~TokenServiceTestHarness();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  virtual scoped_ptr<TestingProfile> CreateProfile();
  void UpdateCredentialsOnService();
  TestingProfile* profile() const { return profile_.get(); }
  TokenService* service() const { return service_; }
  const GaiaAuthConsumer::ClientLoginResult& credentials() const {
    return credentials_;
  }

  void CreateSigninManager(const std::string& username);

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  FakeSigninManagerBase* signin_manager_;
  TokenService* service_;
  GaiaAuthConsumer::ClientLoginResult credentials_;
  std::string oauth_token_;
  std::string oauth_secret_;
  scoped_ptr<TestingProfile> profile_;
};

#endif  // CHROME_BROWSER_SIGNIN_TOKEN_SERVICE_UNITTEST_H_
