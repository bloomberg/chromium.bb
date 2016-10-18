// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SESSION_SESSION_STATE_OBSERVER_H_
#define ASH_COMMON_SESSION_SESSION_STATE_OBSERVER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "components/session_manager/session_manager_types.h"

class AccountId;

namespace ash {

class ASH_EXPORT SessionStateObserver {
 public:
  // Called when active user has changed.
  virtual void ActiveUserChanged(const AccountId& account_id) {}

  // Called when another user gets added to the existing session.
  virtual void UserAddedToSession(const AccountId& account_id) {}

  // Called when session state is changed.
  virtual void SessionStateChanged(session_manager::SessionState state) {}

 protected:
  virtual ~SessionStateObserver() {}
};

// A class to attach / detach an object as a session state observer.
class ASH_EXPORT ScopedSessionStateObserver {
 public:
  explicit ScopedSessionStateObserver(SessionStateObserver* observer);
  virtual ~ScopedSessionStateObserver();

 private:
  ash::SessionStateObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSessionStateObserver);
};

}  // namespace ash

#endif  // ASH_COMMON_SESSION_SESSION_STATE_OBSERVER_H_
