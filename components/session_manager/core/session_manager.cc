// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/session_manager/core/session_manager.h"

#include "base/logging.h"
#include "build/build_config.h"

namespace session_manager {

// static
SessionManager* SessionManager::instance = NULL;

SessionManager::SessionManager() {
  DCHECK(!SessionManager::Get());
  SessionManager::SetInstance(this);
}

SessionManager::~SessionManager() {
  DCHECK(instance == this);
  SessionManager::SetInstance(NULL);
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

bool SessionManager::IsSessionStarted() const {
  return session_started_;
}

void SessionManager::SessionStarted() {
  session_started_ = true;
}

void SessionManager::Initialize(SessionManagerDelegate* delegate) {
  DCHECK(delegate);
  delegate_.reset(delegate);
  delegate_->SetSessionManager(this);
}

// static
void SessionManager::SetInstance(SessionManager* session_manager) {
  SessionManager::instance = session_manager;
}

void SessionManager::Start() {
  delegate_->Start();
}

SessionManagerDelegate::SessionManagerDelegate() {
}

SessionManagerDelegate::~SessionManagerDelegate() {
}

void SessionManagerDelegate::SetSessionManager(
    session_manager::SessionManager* session_manager) {
  session_manager_ = session_manager;
}

}  // namespace session_manager
