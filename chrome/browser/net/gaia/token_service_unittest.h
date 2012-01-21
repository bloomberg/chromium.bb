// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a unit test harness for the profile's token service.

#ifndef CHROME_BROWSER_NET_GAIA_TOKEN_SERVICE_UNITTEST_H_
#define CHROME_BROWSER_NET_GAIA_TOKEN_SERVICE_UNITTEST_H_
#pragma once

#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/test/test_browser_thread.h"
#include "content/test/test_notification_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

// TestNotificationTracker doesn't do a deep copy on the notification details.
// We have to in order to read it out, or we have a bad ptr, since the details
// are a reference on the stack.
class TokenAvailableTracker : public TestNotificationTracker {
 public:
  TokenAvailableTracker();
  virtual ~TokenAvailableTracker();

  const TokenService::TokenAvailableDetails& details() {
    return details_;
  }

 private:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  TokenService::TokenAvailableDetails details_;
};

class TokenFailedTracker : public TestNotificationTracker {
 public:
  TokenFailedTracker();
  virtual ~TokenFailedTracker();

  const TokenService::TokenRequestFailedDetails& details() {
    return details_;
  }

 private:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  TokenService::TokenRequestFailedDetails details_;
};

class TokenServiceTestHarness : public testing::Test {
 public:
  TokenServiceTestHarness();
  virtual ~TokenServiceTestHarness();

  virtual void SetUp() OVERRIDE;

  virtual void TearDown() OVERRIDE;

  void WaitForDBLoadCompletion();

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;  // Mostly so DCHECKS pass.
  content::TestBrowserThread db_thread_;  // WDS on here

  TokenService service_;
  TokenAvailableTracker success_tracker_;
  TokenFailedTracker failure_tracker_;
  GaiaAuthConsumer::ClientLoginResult credentials_;
  std::string oauth_token_;
  std::string oauth_secret_;
  scoped_ptr<TestingProfile> profile_;
};

#endif  // CHROME_BROWSER_NET_GAIA_TOKEN_SERVICE_UNITTEST_H_
