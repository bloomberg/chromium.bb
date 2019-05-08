// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/test_session_controller.h"

#include <utility>

#include "ash/public/interfaces/constants.mojom.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_filter.h"

TestSessionController::TestSessionController() {
  CHECK(content::ServiceManagerConnection::GetForProcess())
      << "ServiceManager is uninitialized. Did you forget to create a "
         "content::TestServiceManagerContext?";
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->OverrideBinderForTesting(
          service_manager::ServiceFilter::ByName(ash::mojom::kServiceName),
          ash::mojom::SessionController::Name_,
          base::BindRepeating(&TestSessionController::Bind,
                              base::Unretained(this)));
}

TestSessionController::~TestSessionController() {
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->ClearBinderOverrideForTesting(
          service_manager::ServiceFilter::ByName(ash::mojom::kServiceName),
          ash::mojom::SessionController::Name_);
}

void TestSessionController::SetClient(ash::SessionControllerClient* client) {}

void TestSessionController::SetSessionInfo(const ash::SessionInfo& info) {
  last_session_info_ = info;
}

void TestSessionController::UpdateUserSession(
    const ash::UserSession& user_session) {
  last_user_session_ = user_session;
  update_user_session_count_++;
}

void TestSessionController::SetUserSessionOrder(
    const std::vector<uint32_t>& user_session_order) {}

void TestSessionController::PrepareForLock(PrepareForLockCallback callback) {
  std::move(callback).Run();
}

void TestSessionController::StartLock(StartLockCallback callback) {
  std::move(callback).Run(true);
}

void TestSessionController::NotifyChromeLockAnimationsComplete() {
  lock_animation_complete_call_count_++;
}

void TestSessionController::RunUnlockAnimation(
    RunUnlockAnimationCallback callback) {
  std::move(callback).Run();
}

void TestSessionController::NotifyChromeTerminating() {}

void TestSessionController::SetSessionLengthLimit(base::TimeDelta length_limit,
                                                  base::TimeTicks start_time) {
  last_session_length_limit_ = length_limit;
  last_session_start_time_ = start_time;
}

void TestSessionController::CanSwitchActiveUser(
    CanSwitchActiveUserCallback callback) {
  std::move(callback).Run(true);
}

void TestSessionController::ShowMultiprofilesIntroDialog(
    ShowMultiprofilesIntroDialogCallback callback) {
  std::move(callback).Run(true, false);
}

void TestSessionController::ShowTeleportWarningDialog(
    ShowTeleportWarningDialogCallback callback) {
  std::move(callback).Run(true, false);
}

void TestSessionController::ShowMultiprofilesSessionAbortedDialog(
    const std::string& user_email) {}

void TestSessionController::AddSessionActivationObserverForAccountId(
    const AccountId& account_id,
    ash::mojom::SessionActivationObserverPtr observer) {}

void TestSessionController::Bind(mojo::ScopedMessagePipeHandle handle) {
  binding_.Bind(ash::mojom::SessionControllerRequest(std::move(handle)));
}
