// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SESSION_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_SESSION_CONTROLLER_CLIENT_H_

#include "ash/public/interfaces/session_controller.mojom.h"
#include "base/macros.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {
enum class AddUserSessionPolicy;
}

namespace user_manager {
class User;
}

// Updates session state etc to ash via SessionController interface and handles
// session related calls from ash.
// TODO(xiyuan): Update when UserSessionStateObserver is gone.
class SessionControllerClient
    : public ash::mojom::SessionControllerClient,
      public user_manager::UserManager::UserSessionStateObserver,
      public user_manager::UserManager::Observer,
      public session_manager::SessionManagerObserver {
 public:
  SessionControllerClient();
  ~SessionControllerClient() override;

  // ash::mojom::SessionControllerClient:
  void RequestLockScreen() override;
  void SwitchActiveUser(const AccountId& account_id) override;
  void CycleActiveUser(ash::CycleUserDirection direction) override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(const user_manager::User* active_user) override;
  void UserAddedToSession(const user_manager::User* added_user) override;

  // user_manager::UserManager::Observer
  void OnUserImageChanged(const user_manager::User& user) override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // TODO(xiyuan): Remove after SessionStateDelegateChromeOS is gone.
  static bool CanLockScreen();
  static bool ShouldLockScreenAutomatically();
  static ash::AddUserSessionPolicy GetAddUserSessionPolicy();
  static void DoLockScreen();
  static void DoSwitchActiveUser(const AccountId& account_id);
  static void DoCycleActiveUser(ash::CycleUserDirection direction);

  // Flushes the mojo pipe to ash.
  static void FlushForTesting();

 private:
  // Connects or reconnects to the |session_controller_| interface and set
  // this object as its client.
  void ConnectToSessionControllerAndSetClient();

  // Sends session info to ash.
  void SendSessionInfoIfChanged();

  // Sends the user session info.
  void SendUserSession(const user_manager::User& user);

  // Sends the order of user sessions to ash.
  void SendUserSessionOrder();

  // Binds to the client interface.
  mojo::Binding<ash::mojom::SessionControllerClient> binding_;

  // SessionController interface in ash.
  ash::mojom::SessionControllerPtr session_controller_;

  // Whether the primary user session info is sent to ash.
  bool primary_user_session_sent_ = false;

  ash::mojom::SessionInfoPtr last_sent_session_info_;

  DISALLOW_COPY_AND_ASSIGN(SessionControllerClient);
};

#endif  // CHROME_BROWSER_UI_ASH_SESSION_CONTROLLER_CLIENT_H_
