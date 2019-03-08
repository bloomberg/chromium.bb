// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/login_screen_tester.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/login_screen_test_api.test-mojom-test-utils.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {
namespace test {

LoginScreenTester::LoginScreenTester() {
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &test_api_);
}

LoginScreenTester::~LoginScreenTester() = default;

int64_t LoginScreenTester::GetUiUpdateCount() {
  ash::mojom::LoginScreenTestApiAsyncWaiter login_screen(test_api_.get());
  int64_t ui_update_count = 0;
  login_screen.GetUiUpdateCount(&ui_update_count);
  return ui_update_count;
}

bool LoginScreenTester::IsRestartButtonShown() {
  ash::mojom::LoginScreenTestApiAsyncWaiter login_screen(test_api_.get());
  bool is_restart_button_shown;
  login_screen.IsRestartButtonShown(&is_restart_button_shown);
  return is_restart_button_shown;
}

bool LoginScreenTester::IsShutdownButtonShown() {
  ash::mojom::LoginScreenTestApiAsyncWaiter login_screen(test_api_.get());
  bool is_shutdown_button_shown;
  login_screen.IsShutdownButtonShown(&is_shutdown_button_shown);
  return is_shutdown_button_shown;
}

bool LoginScreenTester::ClickAddUserButton() {
  ash::mojom::LoginScreenTestApiAsyncWaiter login_screen(test_api_.get());
  bool success;
  login_screen.ClickAddUserButton(&success);
  return success;
}

bool LoginScreenTester::WaitForUiUpdate(int64_t previous_update_count) {
  ash::mojom::LoginScreenTestApiAsyncWaiter login_screen(test_api_.get());
  bool success;
  login_screen.WaitForUiUpdate(previous_update_count, &success);
  return success;
}

}  // namespace test
}  // namespace chromeos
