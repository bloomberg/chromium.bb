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
  MOCK_METHOD3(OnLoginSuccess, void(
      const std::string& username,
      const GaiaAuthConsumer::ClientLoginResult& result,
      bool pending_requests));
  MOCK_METHOD0(OnOffTheRecordLoginSuccess, void(void));
  MOCK_METHOD1(OnPasswordChangeDetected,
      void(const GaiaAuthConsumer::ClientLoginResult& result));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_LOGIN_STATUS_CONSUMER_H_
