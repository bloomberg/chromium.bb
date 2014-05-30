// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CHECK_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CHECK_SCREEN_ACTOR_H_

namespace chromeos {

// Interface between auto-enrollment check screen and its representation.
// Note, do not forget to call OnActorDestroyed in the dtor.
class AutoEnrollmentCheckScreenActor {
 public:
  // Allows us to get info from auto-enrollment check screen that we need.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when screen is exited.
    virtual void OnExit() = 0;

    // This method is called, when actor is being destroyed. Note, if Delegate
    // is destroyed earlier then it has to call SetDelegate(NULL).
    virtual void OnActorDestroyed(AutoEnrollmentCheckScreenActor* actor) = 0;
  };

  virtual ~AutoEnrollmentCheckScreenActor() {}

  virtual void Show() = 0;
  virtual void SetDelegate(Delegate* delegate) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CHECK_SCREEN_ACTOR_H_
