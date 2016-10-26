// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_H_
#define COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "components/session_manager/session_manager_export.h"
#include "components/session_manager/session_manager_types.h"

namespace session_manager {

class SessionManagerDelegate;

class SESSION_EXPORT SessionManager {
 public:
  SessionManager();
  virtual ~SessionManager();

  // Returns current SessionManager instance and NULL if it hasn't been
  // initialized yet.
  static SessionManager* Get();

  SessionState session_state() const { return session_state_; }
  virtual void SetSessionState(SessionState state);

  // Returns true if we're logged in and browser has been started i.e.
  // browser_creator.LaunchBrowser(...) was called after sign in
  // or restart after crash.
  virtual bool IsSessionStarted() const;

  // Called when browser session is started i.e. after
  // browser_creator.LaunchBrowser(...) was called after user sign in.
  // When user is at the image screen IsUserLoggedIn() will return true
  // but IsSessionStarted() will return false. During the kiosk splash screen,
  // we perform additional initialization after the user is logged in but
  // before the session has been started.
  virtual void SessionStarted();

  // Let session delegate executed on its plan of actions depending on the
  // current session type / state.
  void Start();

 protected:
  // Initializes SessionManager with delegate.
  void Initialize(SessionManagerDelegate* delegate);

  // Sets SessionManager instance.
  static void SetInstance(SessionManager* session_manager);

 private:
  // Pointer to the existing SessionManager instance (if any).
  // Set in ctor, reset in dtor. Not owned since specific implementation of
  // SessionManager should decide on its own appropriate owner of SessionManager
  // instance. For src/chrome implementation such place is
  // g_browser_process->platform_part().
  static SessionManager* instance;

  SessionState session_state_ = SessionState::UNKNOWN;
  std::unique_ptr<SessionManagerDelegate> delegate_;

  // True if SessionStarted() has been called.
  bool session_started_ = false;

  DISALLOW_COPY_AND_ASSIGN(SessionManager);
};

class SESSION_EXPORT SessionManagerDelegate {
 public:
  SessionManagerDelegate();
  virtual ~SessionManagerDelegate();

  virtual void SetSessionManager(
      session_manager::SessionManager* session_manager);

  // Executes specific actions defined by this delegate.
  virtual void Start() = 0;

 protected:
  session_manager::SessionManager* session_manager_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionManagerDelegate);
};

}  // namespace session_manager

#endif  // COMPONENTS_SESSION_MANAGER_CORE_SESSION_MANAGER_H_
