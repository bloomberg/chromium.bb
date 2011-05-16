// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mock_login_status_consumer.h"

#include "base/message_loop.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

MockConsumer::MockConsumer() {}

MockConsumer::~MockConsumer() {}

// static
void MockConsumer::OnGuestSuccessQuit() {
  MessageLoop::current()->Quit();
}

// static
void MockConsumer::OnGuestSuccessQuitAndFail() {
  ADD_FAILURE() << "Guest Login should have failed!";
  MessageLoop::current()->Quit();
}

// static
void MockConsumer::OnSuccessQuit(
    const std::string& username,
    const std::string& password,
    const GaiaAuthConsumer::ClientLoginResult& credentials,
    bool pending_requests) {
  MessageLoop::current()->Quit();
}

// static
void MockConsumer::OnSuccessQuitAndFail(
    const std::string& username,
    const std::string& password,
    const GaiaAuthConsumer::ClientLoginResult& credentials,
    bool pending_requests) {
  ADD_FAILURE() << "Login should NOT have succeeded!";
  MessageLoop::current()->Quit();
}

// static
void MockConsumer::OnFailQuit(const LoginFailure& error) {
  MessageLoop::current()->Quit();
}

// static
void MockConsumer::OnFailQuitAndFail(const LoginFailure& error) {
  ADD_FAILURE() << "Login should not have failed!";
  MessageLoop::current()->Quit();
}

// static
void MockConsumer::OnMigrateQuit(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  MessageLoop::current()->Quit();
}

// static
void MockConsumer::OnMigrateQuitAndFail(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  ADD_FAILURE() << "Should not have detected a PW change!";
  MessageLoop::current()->Quit();
}

}  // namespace chromeos
