// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_TEST_SESSION_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_TEST_SESSION_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/interfaces/session_controller.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"

// Test implementation of ash's mojo SessionController interface.
//
// Registers itself to ServiceManager on construction and deregisters
// on destruction.
//
// Note: A ServiceManagerConnection must be initialized before constructing this
// object. Consider using content::TestServiceManagerContext on your tests.
class TestSessionController : public ash::SessionController,
                              public ash::mojom::SessionController {
 public:
  TestSessionController();
  ~TestSessionController() override;

  ash::mojom::SessionInfo* last_session_info() {
    return last_session_info_.get();
  }

  base::TimeDelta last_session_length_limit() {
    return last_session_length_limit_;
  }

  base::TimeTicks last_session_start_time() { return last_session_start_time_; }

  ash::mojom::UserSession* last_user_session() {
    return last_user_session_.get();
  }

  int update_user_session_count() { return update_user_session_count_; }

  int lock_animation_complete_call_count() {
    return lock_animation_complete_call_count_;
  }

  // ash::SessionController:
  void SetClient(ash::SessionControllerClient* client) override;

  // ash::mojom::SessionController:
  void SetSessionInfo(ash::mojom::SessionInfoPtr info) override;
  void UpdateUserSession(ash::mojom::UserSessionPtr user_session) override;
  void SetUserSessionOrder(
      const std::vector<uint32_t>& user_session_order) override;
  void PrepareForLock(PrepareForLockCallback callback) override;
  void StartLock(StartLockCallback callback) override;
  void NotifyChromeLockAnimationsComplete() override;
  void RunUnlockAnimation(RunUnlockAnimationCallback callback) override;
  void NotifyChromeTerminating() override;
  void SetSessionLengthLimit(base::TimeDelta length_limit,
                             base::TimeTicks start_time) override;
  void CanSwitchActiveUser(CanSwitchActiveUserCallback callback) override;
  void ShowMultiprofilesIntroDialog(
      ShowMultiprofilesIntroDialogCallback callback) override;
  void ShowTeleportWarningDialog(
      ShowTeleportWarningDialogCallback callback) override;
  void ShowMultiprofilesSessionAbortedDialog(
      const std::string& user_email) override;
  void AddSessionActivationObserverForAccountId(
      const AccountId& account_id,
      ash::mojom::SessionActivationObserverPtr observer) override;

 private:
  void Bind(mojo::ScopedMessagePipeHandle handle);

  ash::mojom::SessionInfoPtr last_session_info_;
  ash::mojom::UserSessionPtr last_user_session_;
  base::TimeDelta last_session_length_limit_;
  base::TimeTicks last_session_start_time_;
  int update_user_session_count_ = 0;
  int lock_animation_complete_call_count_ = 0;
  mojo::Binding<ash::mojom::SessionController> binding_{this};

  DISALLOW_COPY_AND_ASSIGN(TestSessionController);
};

#endif  // CHROME_BROWSER_UI_ASH_TEST_SESSION_CONTROLLER_H_
