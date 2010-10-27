// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_LOGIN_STATUS_CONSUMER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_LOGIN_STATUS_CONSUMER_H_
#pragma once

#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class MockConsumer : public LoginStatusConsumer {
 public:
  MockConsumer() {}
  ~MockConsumer() {}
  MOCK_METHOD1(OnLoginFailure, void(const LoginFailure& error));
  MOCK_METHOD4(OnLoginSuccess, void(
      const std::string& username,
      const std::string& password,
      const GaiaAuthConsumer::ClientLoginResult& result,
      bool pending_requests));
  MOCK_METHOD0(OnOffTheRecordLoginSuccess, void(void));
  MOCK_METHOD1(OnPasswordChangeDetected,
      void(const GaiaAuthConsumer::ClientLoginResult& result));

  // The following functions can be used in gmock Invoke() clauses.

  // Compatible with LoginStatusConsumer::OnOffTheRecordLoginSuccess()
  static void OnGuestSuccessQuit() {
    MessageLoop::current()->Quit();
  }

  static void OnGuestSuccessQuitAndFail() {
    ADD_FAILURE() << "Guest Login should have failed!";
    MessageLoop::current()->Quit();
  }

  // Compatible with LoginStatusConsumer::OnLoginSuccess()
  static void OnSuccessQuit(
      const std::string& username,
      const std::string& password,
      const GaiaAuthConsumer::ClientLoginResult& credentials,
      bool pending_requests) {
    MessageLoop::current()->Quit();
  }

  static void OnSuccessQuitAndFail(
      const std::string& username,
      const std::string& password,
      const GaiaAuthConsumer::ClientLoginResult& credentials,
      bool pending_requests) {
    ADD_FAILURE() << "Login should NOT have succeeded!";
    MessageLoop::current()->Quit();
  }

  // Compatible with LoginStatusConsumer::OnLoginFailure()
  static void OnFailQuit(const LoginFailure& error) {
    MessageLoop::current()->Quit();
  }

  static void OnFailQuitAndFail(const LoginFailure& error) {
    ADD_FAILURE() << "Login should not have failed!";
    MessageLoop::current()->Quit();
  }

  // Compatible with LoginStatusConsumer::OnPasswordChangeDetected()
  static void OnMigrateQuit(
      const GaiaAuthConsumer::ClientLoginResult& credentials) {
    MessageLoop::current()->Quit();
  }

  static void OnMigrateQuitAndFail(
      const GaiaAuthConsumer::ClientLoginResult& credentials) {
    ADD_FAILURE() << "Should not have detected a PW change!";
    MessageLoop::current()->Quit();
  }
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_LOGIN_STATUS_CONSUMER_H_
