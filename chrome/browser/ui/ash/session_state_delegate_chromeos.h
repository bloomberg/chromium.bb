// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_CHROMEOS_H_

#include "ash/common/session/session_state_delegate.h"
#include "base/macros.h"

class SessionStateDelegateChromeos : public ash::SessionStateDelegate {
 public:
  SessionStateDelegateChromeos();
  ~SessionStateDelegateChromeos() override;

  // ash::SessionStateDelegate:
  bool ShouldShowAvatar(ash::WmWindow* window) const override;
  gfx::ImageSkia GetAvatarImageForWindow(ash::WmWindow* window) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegateChromeos);
};

#endif  // CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_CHROMEOS_H_
