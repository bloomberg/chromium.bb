// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/session_controller.h"

#include <algorithm>
#include <utility>

#include "ash/session/session_observer.h"
#include "ash/shell.h"
#include "ash/system/power/power_event_observer.h"
#include "ash/wm/lock_state_controller.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "chromeos/chromeos_switches.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_type.h"
#include "services/service_manager/public/cpp/connector.h"

using session_manager::SessionState;

namespace ash {

namespace {

// Get the default session state. Default session state is ACTIVE when the
// process starts with a user session, i.e. the process has kLoginUser command
// line switch. This is needed because ash focus rules depends on whether
// session is blocked to pick an activatable window and chrome needs to create a
// focused browser window when starting with a user session (both in production
// and in tests). Using ACTIVE as default in this situation allows chrome to run
// without having to wait for session state to reach to ash. For other cases
// (oobe/login), there is only one login window. The login window always gets
// focus so default session state does not matter. Use UNKNOWN and wait for
// chrome to update ash for such cases.
SessionState GetDefaultSessionState() {
  const bool start_with_user =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kLoginUser);
  return start_with_user ? SessionState::ACTIVE : SessionState::UNKNOWN;
}

}  // namespace

SessionController::SessionController()
    : state_(GetDefaultSessionState()), weak_ptr_factory_(this) {}

SessionController::~SessionController() {
  // Abort pending start lock request.
  if (!start_lock_callback_.is_null())
    std::move(start_lock_callback_).Run(false /* locked */);
}

void SessionController::BindRequest(mojom::SessionControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

int SessionController::NumberOfLoggedInUsers() const {
  return static_cast<int>(user_sessions_.size());
}

AddUserSessionPolicy SessionController::GetAddUserPolicy() const {
  return add_user_session_policy_;
}

bool SessionController::IsActiveUserSessionStarted() const {
  return !user_sessions_.empty();
}

bool SessionController::CanLockScreen() const {
  return IsActiveUserSessionStarted() && can_lock_;
}

bool SessionController::IsScreenLocked() const {
  return state_ == SessionState::LOCKED;
}

bool SessionController::ShouldLockScreenAutomatically() const {
  return should_lock_screen_automatically_;
}

bool SessionController::IsUserSessionBlocked() const {
  // User sessions are blocked when session state is not ACTIVE, except that
  // LOCKED state with a running unlocking animation. This is made an exception
  // because the unlocking animation hides lock container at the end. During the
  // unlock animation, IsUserSessionBlocked needs to return unblocked so that
  // user windows are deemed activatable and ash correctly restore the active
  // window before locking.
  return state_ != SessionState::ACTIVE &&
         !(state_ == SessionState::LOCKED && is_unlocking_);
}

bool SessionController::IsInSecondaryLoginScreen() const {
  return state_ == SessionState::LOGIN_SECONDARY;
}

SessionState SessionController::GetSessionState() const {
  return state_;
}

bool SessionController::ShouldEnableSettings() const {
  // Settings opens a web UI window, so it is not available at the lock screen.
  if (!IsActiveUserSessionStarted() || IsScreenLocked() ||
      IsInSecondaryLoginScreen()) {
    return false;
  }

  return user_sessions_[0]->should_enable_settings;
}

bool SessionController::ShouldShowNotificationTray() const {
  if (!IsActiveUserSessionStarted() || IsInSecondaryLoginScreen())
    return false;

  return user_sessions_[0]->should_show_notification_tray;
}

const std::vector<mojom::UserSessionPtr>& SessionController::GetUserSessions()
    const {
  return user_sessions_;
}

const mojom::UserSession* SessionController::GetUserSession(
    UserIndex index) const {
  if (index < 0 || index >= static_cast<UserIndex>(user_sessions_.size()))
    return nullptr;

  return user_sessions_[index].get();
}

bool SessionController::IsUserSupervised() const {
  if (!IsActiveUserSessionStarted())
    return false;

  user_manager::UserType active_user_type = GetUserSession(0)->type;
  return active_user_type == user_manager::USER_TYPE_SUPERVISED ||
         active_user_type == user_manager::USER_TYPE_CHILD;
}

bool SessionController::IsUserChild() const {
  if (!IsActiveUserSessionStarted())
    return false;

  user_manager::UserType active_user_type = GetUserSession(0)->type;
  return active_user_type == user_manager::USER_TYPE_CHILD;
}

bool SessionController::IsKioskSession() const {
  if (!IsActiveUserSessionStarted())
    return false;

  user_manager::UserType active_user_type = GetUserSession(0)->type;
  return active_user_type == user_manager::USER_TYPE_KIOSK_APP ||
         active_user_type == user_manager::USER_TYPE_ARC_KIOSK_APP;
}

void SessionController::LockScreen() {
  if (client_)
    client_->RequestLockScreen();
}

void SessionController::SwitchActiveUser(const AccountId& account_id) {
  if (client_)
    client_->SwitchActiveUser(account_id);
}

void SessionController::CycleActiveUser(CycleUserDirection direction) {
  if (client_)
    client_->CycleActiveUser(direction);
}

void SessionController::AddObserver(SessionObserver* observer) {
  observers_.AddObserver(observer);
}

void SessionController::RemoveObserver(SessionObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SessionController::SetClient(mojom::SessionControllerClientPtr client) {
  client_ = std::move(client);
}

void SessionController::SetSessionInfo(mojom::SessionInfoPtr info) {
  can_lock_ = info->can_lock_screen;
  should_lock_screen_automatically_ = info->should_lock_screen_automatically;
  add_user_session_policy_ = info->add_user_session_policy;
  SetSessionState(info->state);
}

void SessionController::UpdateUserSession(mojom::UserSessionPtr user_session) {
  auto it =
      std::find_if(user_sessions_.begin(), user_sessions_.end(),
                   [&user_session](const mojom::UserSessionPtr& session) {
                     return session->session_id == user_session->session_id;
                   });
  if (it == user_sessions_.end()) {
    AddUserSession(std::move(user_session));
    return;
  }

  *it = std::move(user_session);
  for (auto& observer : observers_)
    observer.OnUserSessionUpdated((*it)->account_id);

  UpdateLoginStatus();
}

void SessionController::SetUserSessionOrder(
    const std::vector<uint32_t>& user_session_order) {
  DCHECK_EQ(user_sessions_.size(), user_session_order.size());

  // Adjusts |user_sessions_| to match the given order.
  std::vector<mojom::UserSessionPtr> sessions;
  for (const auto& session_id : user_session_order) {
    auto it =
        std::find_if(user_sessions_.begin(), user_sessions_.end(),
                     [session_id](const mojom::UserSessionPtr& session) {
                       return session && session->session_id == session_id;
                     });
    if (it == user_sessions_.end()) {
      LOG(ERROR) << "Unknown session id =" << session_id;
      continue;
    }

    sessions.push_back(std::move(*it));
  }
  user_sessions_.swap(sessions);

  // Check active user change and notifies observers.
  if (user_sessions_[0]->session_id != active_session_id_) {
    active_session_id_ = user_sessions_[0]->session_id;

    for (auto& observer : observers_)
      observer.OnActiveUserSessionChanged(user_sessions_[0]->account_id);

    UpdateLoginStatus();
  }
}

void SessionController::StartLock(StartLockCallback callback) {
  DCHECK(start_lock_callback_.is_null());
  start_lock_callback_ = std::move(callback);

  LockStateController* const lock_state_controller =
      Shell::Get()->lock_state_controller();

  lock_state_controller->SetLockScreenDisplayedCallback(
      base::Bind(&SessionController::OnLockAnimationFinished,
                 weak_ptr_factory_.GetWeakPtr()));
  lock_state_controller->OnStartingLock();
}

void SessionController::NotifyChromeLockAnimationsComplete() {
  Shell::Get()->power_event_observer()->OnLockAnimationsComplete();
}

void SessionController::RunUnlockAnimation(
    RunUnlockAnimationCallback callback) {
  is_unlocking_ = true;

  // Shell could have no instance in tests.
  if (Shell::HasInstance())
    Shell::Get()->lock_state_controller()->OnLockScreenHide(
        std::move(callback));
}

void SessionController::NotifyChromeTerminating() {
  for (auto& observer : observers_)
    observer.OnChromeTerminating();
}

void SessionController::ClearUserSessionsForTest() {
  user_sessions_.clear();
}

void SessionController::FlushMojoForTest() {
  client_.FlushForTesting();
}

void SessionController::LockScreenAndFlushForTest() {
  LockScreen();
  FlushMojoForTest();
}

void SessionController::SetSessionState(SessionState state) {
  if (state_ == state)
    return;

  const bool was_locked = state_ == SessionState::LOCKED;
  state_ = state;
  for (auto& observer : observers_)
    observer.OnSessionStateChanged(state_);

  UpdateLoginStatus();

  const bool locked = state_ == SessionState::LOCKED;
  if (was_locked != locked) {
    if (!locked)
      is_unlocking_ = false;

    for (auto& observer : observers_)
      observer.OnLockStateChanged(locked);
  }
}

void SessionController::AddUserSession(mojom::UserSessionPtr user_session) {
  const AccountId account_id(user_session->account_id);

  user_sessions_.push_back(std::move(user_session));

  for (auto& observer : observers_)
    observer.OnUserSessionAdded(account_id);
}

LoginStatus SessionController::CalculateLoginStatus() const {
  // TODO(jamescook|xiyuan): There is not a 1:1 mapping of SessionState to
  // LoginStatus. Fix the cases that don't match. http://crbug.com/701193
  switch (state_) {
    case SessionState::UNKNOWN:
    case SessionState::OOBE:
    case SessionState::LOGIN_PRIMARY:
    case SessionState::LOGGED_IN_NOT_ACTIVE:
      return LoginStatus::NOT_LOGGED_IN;

    case SessionState::ACTIVE:
      return CalculateLoginStatusForActiveSession();

    case SessionState::LOCKED:
      return LoginStatus::LOCKED;

    case SessionState::LOGIN_SECONDARY:
      // TODO: There is no LoginStatus for this.
      return LoginStatus::USER;
  }
  NOTREACHED();
  return LoginStatus::NOT_LOGGED_IN;
}

LoginStatus SessionController::CalculateLoginStatusForActiveSession() const {
  DCHECK(state_ == SessionState::ACTIVE);

  if (user_sessions_.empty())  // Can be empty in tests.
    return LoginStatus::USER;

  switch (user_sessions_[0]->type) {
    case user_manager::USER_TYPE_REGULAR:
      // TODO: This needs to distinguish between owner and non-owner.
      return LoginStatus::USER;
    case user_manager::USER_TYPE_GUEST:
      return LoginStatus::GUEST;
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
      return LoginStatus::PUBLIC;
    case user_manager::USER_TYPE_SUPERVISED:
      return LoginStatus::SUPERVISED;
    case user_manager::USER_TYPE_KIOSK_APP:
      return LoginStatus::KIOSK_APP;
    case user_manager::USER_TYPE_CHILD:
      return LoginStatus::SUPERVISED;
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
      return LoginStatus::ARC_KIOSK_APP;
    case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
      // TODO: There is no LoginStatus for this.
      return LoginStatus::USER;
    case user_manager::NUM_USER_TYPES:
      // Avoid having a "default" case so the compiler catches new enum values.
      NOTREACHED();
      return LoginStatus::USER;
  }
  NOTREACHED();
  return LoginStatus::USER;
}

void SessionController::UpdateLoginStatus() {
  const LoginStatus new_login_status = CalculateLoginStatus();
  if (new_login_status == login_status_)
    return;

  login_status_ = new_login_status;
  for (auto& observer : observers_)
    observer.OnLoginStatusChanged(login_status_);
}

void SessionController::OnLockAnimationFinished() {
  if (!start_lock_callback_.is_null())
    std::move(start_lock_callback_).Run(true /* locked */);
}

}  // namespace ash
