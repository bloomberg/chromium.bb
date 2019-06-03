// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/test_login_screen.h"

#include <utility>

#include "ash/public/interfaces/constants.mojom.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_filter.h"

TestLoginScreen::TestLoginScreen() {
  CHECK(content::ServiceManagerConnection::GetForProcess())
      << "ServiceManager is uninitialized. Did you forget to create a "
         "content::TestServiceManagerContext?";
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->OverrideBinderForTesting(
          service_manager::ServiceFilter::ByName(ash::mojom::kServiceName),
          ash::mojom::LoginScreen::Name_,
          base::BindRepeating(&TestLoginScreen::Bind, base::Unretained(this)));
}

TestLoginScreen::~TestLoginScreen() {
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->ClearBinderOverrideForTesting(
          service_manager::ServiceFilter::ByName(ash::mojom::kServiceName),
          ash::mojom::LoginScreen::Name_);
}

void TestLoginScreen::SetClient(ash::mojom::LoginScreenClientPtr client) {}

void TestLoginScreen::ShowLockScreen(ShowLockScreenCallback callback) {
  std::move(callback).Run(true);
}

void TestLoginScreen::ShowLoginScreen(ShowLoginScreenCallback callback) {
  std::move(callback).Run(true);
}

ash::LoginScreenModel* TestLoginScreen::GetModel() {
  return &test_screen_model_;
}

void TestLoginScreen::ShowKioskAppError(const std::string& message) {}

void TestLoginScreen::FocusLoginShelf(bool reverse) {}

bool TestLoginScreen::IsReadyForPassword() {
  return true;
}

void TestLoginScreen::EnableAddUserButton(bool enable) {}

void TestLoginScreen::EnableShutdownButton(bool enable) {}

void TestLoginScreen::ShowGuestButtonInOobe(bool show) {}

void TestLoginScreen::ShowParentAccessButton(bool show) {}

void TestLoginScreen::ShowParentAccessWidget(
    const AccountId& child_account_id,
    base::RepeatingCallback<void(bool success)> callback) {}

void TestLoginScreen::SetAllowLoginAsGuest(bool allow_guest) {}

void TestLoginScreen::Bind(mojo::ScopedMessagePipeHandle handle) {
  binding_.Bind(ash::mojom::LoginScreenRequest(std::move(handle)));
}
