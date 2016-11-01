// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/session_manager/core/session_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "build/build_config.h"
#include "components/user_manager/user_manager.h"

namespace session_manager {

// static
SessionManager* SessionManager::instance = nullptr;

SessionManager::SessionManager() {
  DCHECK(!SessionManager::Get());
  SessionManager::SetInstance(this);
}

SessionManager::~SessionManager() {
  DCHECK_EQ(instance, this);
  SessionManager::SetInstance(nullptr);
}

// static
SessionManager* SessionManager::Get() {
  return SessionManager::instance;
}

void SessionManager::SetSessionState(SessionState state) {
  VLOG(1) << "Changing session state to: " << static_cast<int>(state);

  if (session_state_ != state) {
    // TODO(nkostylev): Notify observers about the state change.
    // TODO(nkostylev): Add code to process session state change and probably
    // replace delegate_ if needed.
    session_state_ = state;
  }
}

void SessionManager::CreateSession(const AccountId& user_account_id,
                                   const std::string& user_id_hash) {
  CreateSessionInternal(user_account_id, user_id_hash,
                        false /* browser_restart */);
}

void SessionManager::CreateSessionForRestart(const AccountId& user_account_id,
                                             const std::string& user_id_hash) {
  CreateSessionInternal(user_account_id, user_id_hash,
                        true /* browser_restart */);
}

bool SessionManager::IsSessionStarted() const {
  return session_started_;
}

void SessionManager::SessionStarted() {
  session_started_ = true;
}

void SessionManager::NotifyUserLoggedIn(const AccountId& user_account_id,
                                        const std::string& user_id_hash,
                                        bool browser_restart) {
  auto* user_manager = user_manager::UserManager::Get();
  if (!user_manager)
    return;
  user_manager->UserLoggedIn(user_account_id, user_id_hash, browser_restart);
}

// static
void SessionManager::SetInstance(SessionManager* session_manager) {
  SessionManager::instance = session_manager;
}

void SessionManager::CreateSessionInternal(const AccountId& user_account_id,
                                           const std::string& user_id_hash,
                                           bool browser_restart) {
  DCHECK(std::find_if(sessions_.begin(), sessions_.end(),
                      [user_account_id](const Session& session) {
                        return session.user_account_id == user_account_id;
                      }) == sessions_.end());

  sessions_.push_back({next_id_++, user_account_id});
  NotifyUserLoggedIn(user_account_id, user_id_hash, browser_restart);
}

}  // namespace session_manager
