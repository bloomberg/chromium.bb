// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mock_login_status_consumer.h"

#include "base/message_loop.h"
#include "chrome/browser/chromeos/login/user.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

MockConsumer::MockConsumer() {}

MockConsumer::~MockConsumer() {}

// static
void MockConsumer::OnRetailModeSuccessQuit(const UserContext& user_context) {
  MessageLoop::current()->Quit();
}

// static
void MockConsumer::OnRetailModeSuccessQuitAndFail(
    const UserContext& user_context) {
  ADD_FAILURE() << "Retail mode login should have failed!";
  MessageLoop::current()->Quit();
}

// static
void MockConsumer::OnGuestSuccessQuit() {
  MessageLoop::current()->Quit();
}

// static
void MockConsumer::OnGuestSuccessQuitAndFail() {
  ADD_FAILURE() << "Guest login should have failed!";
  MessageLoop::current()->Quit();
}

// static
void MockConsumer::OnSuccessQuit(
    const UserContext& user_context,
    bool pending_requests,
    bool using_oauth) {
  MessageLoop::current()->Quit();
}

// static
void MockConsumer::OnSuccessQuitAndFail(
    const UserContext& user_context,
    bool pending_requests,
    bool using_oauth) {
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
void MockConsumer::OnMigrateQuit() {
  MessageLoop::current()->Quit();
}

// static
void MockConsumer::OnMigrateQuitAndFail() {
  ADD_FAILURE() << "Should not have detected a PW change!";
  MessageLoop::current()->Quit();
}

}  // namespace chromeos
