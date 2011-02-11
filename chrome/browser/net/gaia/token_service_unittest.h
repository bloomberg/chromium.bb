// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a unit test harness for the profile's token service.

#ifndef CHROME_BROWSER_NET_GAIA_TOKEN_SERVICE_UNITTEST_H_
#define CHROME_BROWSER_NET_GAIA_TOKEN_SERVICE_UNITTEST_H_
#pragma once

#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/test/signaling_task.h"
#include "chrome/test/test_notification_tracker.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

// TestNotificationTracker doesn't do a deep copy on the notification details.
// We have to in order to read it out, or we have a bad ptr, since the details
// are a reference on the stack.
class TokenAvailableTracker : public TestNotificationTracker {
 public:
  const TokenService::TokenAvailableDetails& details() {
    return details_;
  }

 private:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    TestNotificationTracker::Observe(type, source, details);
    if (type == NotificationType::TOKEN_AVAILABLE) {
      Details<const TokenService::TokenAvailableDetails> full = details;
      details_ = *full.ptr();
    }
  }

  TokenService::TokenAvailableDetails details_;
};

class TokenFailedTracker : public TestNotificationTracker {
 public:
  const TokenService::TokenRequestFailedDetails& details() {
    return details_;
  }

 private:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    TestNotificationTracker::Observe(type, source, details);
    if (type == NotificationType::TOKEN_REQUEST_FAILED) {
      Details<const TokenService::TokenRequestFailedDetails> full = details;
      details_ = *full.ptr();
    }
  }

  TokenService::TokenRequestFailedDetails details_;
};

class TokenServiceTestHarness : public testing::Test {
 public:
  TokenServiceTestHarness();
  virtual ~TokenServiceTestHarness();

  virtual void SetUp();

  virtual void TearDown();

  void WaitForDBLoadCompletion();

  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;  // Mostly so DCHECKS pass.
  BrowserThread db_thread_;  // WDS on here

  TokenService service_;
  TokenAvailableTracker success_tracker_;
  TokenFailedTracker failure_tracker_;
  GaiaAuthConsumer::ClientLoginResult credentials_;
  scoped_ptr<TestingProfile> profile_;
};

#endif  // CHROME_BROWSER_NET_GAIA_TOKEN_SERVICE_UNITTEST_H_
