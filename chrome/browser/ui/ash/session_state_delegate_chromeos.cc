// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session_state_delegate_chromeos.h"

#include "ash/common/session/session_state_observer.h"
#include "ash/common/wm_window.h"
#include "ash/content/shell_content_state.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "chrome/browser/ui/ash/session_util.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/login_state.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_info.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/gfx/image/image_skia.h"

SessionStateDelegateChromeos::SessionStateDelegateChromeos()
    : session_state_(session_manager::SessionState::LOGIN_PRIMARY) {
  user_manager::UserManager::Get()->AddSessionStateObserver(this);
  chromeos::UserAddingScreen::Get()->AddObserver(this);

  // LoginState is not initialized in unit_tests.
  if (chromeos::LoginState::IsInitialized()) {
    chromeos::LoginState::Get()->AddObserver(this);
    // Note that the session state is only set to ACTIVE or LOGIN_PRIMARY
    // instead of using SessionManager::Get()->session_state(). This is
    // an intermediate state of replacing SessionStateDelegate with
    // mojo interfaces. The replacement mojo interface would reflect
    // real session state in SessionManager and have getters to translate
    // them in a sensible way to ash code.
    SetSessionState(chromeos::LoginState::Get()->IsUserLoggedIn()
                        ? session_manager::SessionState::ACTIVE
                        : session_manager::SessionState::LOGIN_PRIMARY,
                    true);
  }
}

SessionStateDelegateChromeos::~SessionStateDelegateChromeos() {
  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
  chromeos::UserAddingScreen::Get()->RemoveObserver(this);

  // LoginState is not initialized in unit_tests.
  if (chromeos::LoginState::IsInitialized())
    chromeos::LoginState::Get()->RemoveObserver(this);
}

int SessionStateDelegateChromeos::GetMaximumNumberOfLoggedInUsers() const {
  // We limit list of logged in users to 10 due to memory constraints.
  // Note that 10 seems excessive, but we want to test how many users are
  // actually added to a session.
  // TODO(nkostylev): Adjust this limitation based on device capabilites.
  // http://crbug.com/230865
  return session_manager::kMaxmiumNumberOfUserSessions;
}

int SessionStateDelegateChromeos::NumberOfLoggedInUsers() const {
  return user_manager::UserManager::Get()->GetLoggedInUsers().size();
}

ash::AddUserSessionPolicy
SessionStateDelegateChromeos::GetAddUserSessionPolicy() const {
  return SessionControllerClient::GetAddUserSessionPolicy();
}

bool SessionStateDelegateChromeos::IsActiveUserSessionStarted() const {
  return session_manager::SessionManager::Get() &&
         session_manager::SessionManager::Get()->IsSessionStarted();
}

bool SessionStateDelegateChromeos::CanLockScreen() const {
  return SessionControllerClient::CanLockScreen();
}

bool SessionStateDelegateChromeos::IsScreenLocked() const {
  return chromeos::ScreenLocker::default_screen_locker() &&
         chromeos::ScreenLocker::default_screen_locker()->locked();
}

bool SessionStateDelegateChromeos::ShouldLockScreenAutomatically() const {
  return SessionControllerClient::ShouldLockScreenAutomatically();
}

void SessionStateDelegateChromeos::LockScreen() {
  return SessionControllerClient::DoLockScreen();
}

void SessionStateDelegateChromeos::UnlockScreen() {
  // This is used only for testing thus far.
  NOTIMPLEMENTED();
}

bool SessionStateDelegateChromeos::IsUserSessionBlocked() const {
  bool has_login_manager = base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kLoginManager);
  return (has_login_manager && !IsActiveUserSessionStarted()) ||
         IsScreenLocked() ||
         chromeos::UserAddingScreen::Get()->IsRunning();
}

session_manager::SessionState SessionStateDelegateChromeos::GetSessionState()
    const {
  return session_state_;
}

const user_manager::UserInfo* SessionStateDelegateChromeos::GetUserInfo(
    ash::UserIndex index) const {
  DCHECK_LT(index, NumberOfLoggedInUsers());
  return user_manager::UserManager::Get()->GetLRULoggedInUsers()[index];
}

bool SessionStateDelegateChromeos::ShouldShowAvatar(
    ash::WmWindow* window) const {
  return chrome::MultiUserWindowManager::GetInstance()->ShouldShowAvatar(
      ash::WmWindow::GetAuraWindow(window));
}

gfx::ImageSkia SessionStateDelegateChromeos::GetAvatarImageForWindow(
    ash::WmWindow* window) const {
  content::BrowserContext* context =
      ash::ShellContentState::GetInstance()->GetBrowserContextForWindow(
          ash::WmWindow::GetAuraWindow(window));
  return GetAvatarImageForContext(context);
}

void SessionStateDelegateChromeos::SwitchActiveUser(
    const AccountId& account_id) {
  SessionControllerClient::DoSwitchActiveUser(account_id);
}

void SessionStateDelegateChromeos::CycleActiveUser(
    ash::CycleUserDirection direction) {
  SessionControllerClient::DoCycleActiveUser(direction);
}

bool SessionStateDelegateChromeos::IsMultiProfileAllowedByPrimaryUserPolicy()
    const {
  return chromeos::MultiProfileUserController::GetPrimaryUserPolicy() ==
         chromeos::MultiProfileUserController::ALLOWED;
}

void SessionStateDelegateChromeos::AddSessionStateObserver(
    ash::SessionStateObserver* observer) {
  session_state_observer_list_.AddObserver(observer);
}

void SessionStateDelegateChromeos::RemoveSessionStateObserver(
    ash::SessionStateObserver* observer) {
  session_state_observer_list_.RemoveObserver(observer);
}

void SessionStateDelegateChromeos::LoggedInStateChanged() {
  SetSessionState(chromeos::LoginState::Get()->IsUserLoggedIn()
                      ? session_manager::SessionState::ACTIVE
                      : session_manager::SessionState::LOGIN_PRIMARY,
                  false);
}

void SessionStateDelegateChromeos::ActiveUserChanged(
    const user_manager::User* active_user) {
  for (ash::SessionStateObserver& observer : session_state_observer_list_)
    observer.ActiveUserChanged(active_user->GetAccountId());
}

void SessionStateDelegateChromeos::UserAddedToSession(
    const user_manager::User* added_user) {
  for (ash::SessionStateObserver& observer : session_state_observer_list_)
    observer.UserAddedToSession(added_user->GetAccountId());
}

void SessionStateDelegateChromeos::OnUserAddingStarted() {
  SetSessionState(session_manager::SessionState::LOGIN_SECONDARY, false);
}

void SessionStateDelegateChromeos::OnUserAddingFinished() {
  SetSessionState(session_manager::SessionState::ACTIVE, false);
}

void SessionStateDelegateChromeos::SetSessionState(
    session_manager::SessionState new_state,
    bool force) {
  if (session_state_ == new_state && !force)
    return;

  session_state_ = new_state;
  NotifySessionStateChanged();
}

void SessionStateDelegateChromeos::NotifySessionStateChanged() {
  for (ash::SessionStateObserver& observer : session_state_observer_list_)
    observer.SessionStateChanged(session_state_);
}
