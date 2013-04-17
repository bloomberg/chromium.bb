// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_H_

#include "ash/session_state_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

class SessionStateDelegate : public ash::SessionStateDelegate {
 public:
  SessionStateDelegate();
  virtual ~SessionStateDelegate();

  // ash::SessionStateDelegate:
  virtual bool HasActiveUser() const OVERRIDE;
  virtual bool IsActiveUserSessionStarted() const OVERRIDE;
  virtual bool CanLockScreen() const OVERRIDE;
  virtual bool IsScreenLocked() const OVERRIDE;
  virtual void LockScreen() OVERRIDE;
  virtual void UnlockScreen() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegate);
};

#endif  // CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_H_
