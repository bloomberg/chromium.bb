// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/session/session_controller.h"

#include <algorithm>

#include "ash/common/session/session_state_observer.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "components/signin/core/account_id/account_id.h"
#include "services/service_manager/public/cpp/connector.h"

namespace ash {

SessionController::SessionController() {}

SessionController::~SessionController() {}

void SessionController::BindRequest(mojom::SessionControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

int SessionController::GetMaximumNumberOfLoggedInUsers() const {
  return session_manager::kMaxmiumNumberOfUserSessions;
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
  return can_lock_;
}

bool SessionController::IsScreenLocked() const {
  return state_ == session_manager::SessionState::LOCKED;
}

bool SessionController::ShouldLockScreenAutomatically() const {
  return should_lock_screen_automatically_;
}

bool SessionController::IsUserSessionBlocked() const {
  return state_ != session_manager::SessionState::ACTIVE;
}

session_manager::SessionState SessionController::GetSessionState() const {
  return state_;
}

const std::vector<mojom::UserSessionPtr>& SessionController::GetUserSessions()
    const {
  return user_sessions_;
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

void SessionController::AddSessionStateObserver(
    SessionStateObserver* observer) {
  observers_.AddObserver(observer);
}

void SessionController::RemoveSessionStateObserver(
    SessionStateObserver* observer) {
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
  // TODO(xiyuan): Notify observers about meta change to replace things such as
  //     NOTIFICATION_LOGIN_USER_IMAGE_CHANGED. http://crbug.com/670422
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
      observer.ActiveUserChanged(user_sessions_[0]->account_id);
  }
}

void SessionController::SetSessionState(session_manager::SessionState state) {
  if (state_ == state)
    return;

  state_ = state;
  for (auto& observer : observers_)
    observer.SessionStateChanged(state_);
}

void SessionController::AddUserSession(mojom::UserSessionPtr user_session) {
  const AccountId account_id(user_session->account_id);

  user_sessions_.push_back(std::move(user_session));

  for (auto& observer : observers_)
    observer.UserAddedToSession(account_id);
}

}  // namespace ash
