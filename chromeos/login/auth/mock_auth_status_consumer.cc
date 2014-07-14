// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/mock_auth_status_consumer.h"

#include "base/message_loop/message_loop.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

MockAuthStatusConsumer::MockAuthStatusConsumer() {
}

MockAuthStatusConsumer::~MockAuthStatusConsumer() {
}

// static
void MockAuthStatusConsumer::OnRetailModeSuccessQuit(
    const UserContext& user_context) {
  base::MessageLoop::current()->Quit();
}

// static
void MockAuthStatusConsumer::OnRetailModeSuccessQuitAndFail(
    const UserContext& user_context) {
  ADD_FAILURE() << "Retail mode login should have failed!";
  base::MessageLoop::current()->Quit();
}

// static
void MockAuthStatusConsumer::OnGuestSuccessQuit() {
  base::MessageLoop::current()->Quit();
}

// static
void MockAuthStatusConsumer::OnGuestSuccessQuitAndFail() {
  ADD_FAILURE() << "Guest login should have failed!";
  base::MessageLoop::current()->Quit();
}

// static
void MockAuthStatusConsumer::OnSuccessQuit(const UserContext& user_context) {
  base::MessageLoop::current()->Quit();
}

// static
void MockAuthStatusConsumer::OnSuccessQuitAndFail(
    const UserContext& user_context) {
  ADD_FAILURE() << "Login should NOT have succeeded!";
  base::MessageLoop::current()->Quit();
}

// static
void MockAuthStatusConsumer::OnFailQuit(const AuthFailure& error) {
  base::MessageLoop::current()->Quit();
}

// static
void MockAuthStatusConsumer::OnFailQuitAndFail(const AuthFailure& error) {
  ADD_FAILURE() << "Login should not have failed!";
  base::MessageLoop::current()->Quit();
}

// static
void MockAuthStatusConsumer::OnMigrateQuit() {
  base::MessageLoop::current()->Quit();
}

// static
void MockAuthStatusConsumer::OnMigrateQuitAndFail() {
  ADD_FAILURE() << "Should not have detected a PW change!";
  base::MessageLoop::current()->Quit();
}

}  // namespace chromeos
