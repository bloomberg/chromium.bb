// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_SESSION_STATE_OBSERVER_H_
#define ASH_SESSION_SESSION_STATE_OBSERVER_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/session/session_state_delegate.h"
#include "base/basictypes.h"

namespace ash {

class ASH_EXPORT SessionStateObserver {
 public:
  // Called when active user has changed.
  virtual void ActiveUserChanged(const std::string& user_id) {}

  // Called when another user gets added to the existing session.
  virtual void UserAddedToSession(const std::string& user_id) {}

  // Called when session state is changed.
  virtual void SessionStateChanged(SessionStateDelegate::SessionState state) {}

 protected:
  virtual ~SessionStateObserver() {}
};

// A class to attach / detach an object as a session state observer with a
// scoped pointer.
class ASH_EXPORT ScopedSessionStateObserver {
 public:
  explicit ScopedSessionStateObserver(ash::SessionStateObserver* observer);
  virtual ~ScopedSessionStateObserver();

 private:
  ash::SessionStateObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSessionStateObserver);
};

}  // namespace ash

#endif  // ASH_SESSION_SESSION_STATE_OBSERVER_H_
