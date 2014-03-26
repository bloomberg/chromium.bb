// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ERROR_SCREEN_ACTOR_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ERROR_SCREEN_ACTOR_DELEGATE_H_

namespace chromeos {

class ErrorScreenActorDelegate {
 public:
  virtual ~ErrorScreenActorDelegate() {}
  virtual void OnErrorShow() = 0;
  virtual void OnErrorHide() = 0;
  virtual void OnLaunchOobeGuestSession() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ERROR_SCREEN_ACTOR_DELEGATE_H_
