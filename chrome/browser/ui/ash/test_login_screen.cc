// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/test_login_screen.h"

TestLoginScreen::TestLoginScreen() = default;

TestLoginScreen::~TestLoginScreen() = default;

void TestLoginScreen::SetClient(ash::LoginScreenClient* client) {}

ash::LoginScreenModel* TestLoginScreen::GetModel() {
  return &test_screen_model_;
}

void TestLoginScreen::ShowLockScreen() {}

void TestLoginScreen::ShowLoginScreen() {}

void TestLoginScreen::ShowKioskAppError(const std::string& message) {}

void TestLoginScreen::FocusLoginShelf(bool reverse) {}

bool TestLoginScreen::IsReadyForPassword() {
  return true;
}

void TestLoginScreen::EnableAddUserButton(bool enable) {}

void TestLoginScreen::EnableShutdownButton(bool enable) {}

void TestLoginScreen::ShowGuestButtonInOobe(bool show) {}

void TestLoginScreen::ShowParentAccessButton(bool show) {}

void TestLoginScreen::SetAllowLoginAsGuest(bool allow_guest) {}
