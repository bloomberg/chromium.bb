// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SESSION_SESSION_CONTROLLER_H_
#define ASH_COMMON_SESSION_SESSION_CONTROLLER_H_

#include <stdint.h>

#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/session_types.h"
#include "ash/public/interfaces/session_controller.mojom.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/binding_set.h"

class AccountId;

namespace ash {

class SessionStateObserver;

// Implements mojom::SessionController to cache session related info such as
// session state, meta data about user sessions to support synchronous
// queries for ash. It is targeted as a replacement for SessionStateDelegate.
class ASH_EXPORT SessionController
    : NON_EXPORTED_BASE(public mojom::SessionController) {
 public:
  SessionController();
  ~SessionController() override;

  // Binds the mojom::SessionControllerRequest to this object.
  void BindRequest(mojom::SessionControllerRequest request);

  // Returns the maximum possible number of logged in users.
  int GetMaximumNumberOfLoggedInUsers() const;

  // Returns the number of signed in users. If 0 is returned, there is either
  // no session in progress or no active user.
  int NumberOfLoggedInUsers() const;

  // Gets the policy of adding a user session to ash.
  AddUserSessionPolicy GetAddUserPolicy() const;

  // Returns |true| if the session has been fully started for the active user.
  // When a user becomes active, the profile and browser UI are not immediately
  // available. Only once this method starts returning |true| is the browser
  // startup complete and both profile and UI are fully available.
  bool IsActiveUserSessionStarted() const;

  // Returns true if the screen can be locked.
  bool CanLockScreen() const;

  // Returns true if the screen is currently locked.
  bool IsScreenLocked() const;

  // Returns true if the screen should be locked automatically when the screen
  // is turned off or the system is suspended.
  bool ShouldLockScreenAutomatically() const;

  // Returns |true| if user session blocked by some overlying UI. It can be
  // login screen, lock screen or screen for adding users into multi-profile
  // session.
  bool IsUserSessionBlocked() const;

  // Gets the ash session state.
  session_manager::SessionState GetSessionState() const;

  // Gets the user sessions in LRU order with the active session being first.
  const std::vector<mojom::UserSessionPtr>& GetUserSessions() const;

  // Locks the screen. The locking happens asynchronously.
  void LockScreen();

  // Switches to another active user with |account_id| (if that user has
  // already signed in).
  void SwitchActiveUser(const AccountId& account_id);

  // Switches the active user to the next or previous user, with the same
  // ordering as user sessions are created.
  void CycleActiveUser(CycleUserDirection direction);

  void AddSessionStateObserver(SessionStateObserver* observer);
  void RemoveSessionStateObserver(SessionStateObserver* observer);

  // mojom::SessionController
  void SetClient(mojom::SessionControllerClientPtr client) override;
  void SetSessionInfo(mojom::SessionInfoPtr info) override;
  void UpdateUserSession(mojom::UserSessionPtr user_session) override;
  void SetUserSessionOrder(
      const std::vector<uint32_t>& user_session_order) override;

 private:
  void SetSessionState(session_manager::SessionState state);
  void AddUserSession(mojom::UserSessionPtr user_session);

  // Bindings for mojom::SessionController interface.
  mojo::BindingSet<mojom::SessionController> bindings_;

  // Client interface to session manager code (chrome).
  mojom::SessionControllerClientPtr client_;

  // Cached session info.
  bool can_lock_ = false;
  bool should_lock_screen_automatically_ = false;
  AddUserSessionPolicy add_user_session_policy_ = AddUserSessionPolicy::ALLOWED;
  session_manager::SessionState state_ = session_manager::SessionState::UNKNOWN;

  // Cached user session info sorted by the order from SetUserSessionOrder.
  // Currently the session manager code (chrome) sets a LRU order with the
  // active session being the first.
  std::vector<mojom::UserSessionPtr> user_sessions_;

  // The user session id of the current active user session. User session id
  // is managed by session manager code, starting at 1. 0u is an invalid id
  // to detect first active user session.
  uint32_t active_session_id_ = 0u;

  base::ObserverList<ash::SessionStateObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(SessionController);
};

}  // namespace ash

#endif  // ASH_COMMON_SESSION_SESSION_CONTROLLER_H_
