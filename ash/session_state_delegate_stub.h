// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_STATE_DELEGATE_STUB_H_
#define ASH_SESSION_STATE_DELEGATE_STUB_H_

#include "ash/session_state_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace ash {

// Stub implementation of SessionStateDelegate for testing.
class SessionStateDelegateStub : public SessionStateDelegate {
 public:
  SessionStateDelegateStub();
  virtual ~SessionStateDelegateStub();

  // SessionStateDelegate:
  virtual bool HasActiveUser() const OVERRIDE;
  virtual bool IsActiveUserSessionStarted() const OVERRIDE;
  virtual bool CanLockScreen() const OVERRIDE;
  virtual bool IsScreenLocked() const OVERRIDE;
  virtual void LockScreen() OVERRIDE;
  virtual void UnlockScreen() OVERRIDE;

 private:
  bool screen_locked_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegateStub);
};

}  // namespace ash

#endif  // ASH_SESSION_STATE_DELEGATE_STUB_H_
