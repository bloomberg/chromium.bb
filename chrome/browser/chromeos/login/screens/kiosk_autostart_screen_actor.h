// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_KIOSK_AUTOSTART_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_KIOSK_AUTOSTART_SCREEN_ACTOR_H_

#include <string>

namespace chromeos {

// Interface between kiosk autostart screen and its representation.
// Note, do not forget to call OnActorDestroyed in the dtor.
class KioskAutostartScreenActor {
 public:
  // Allows us to get info from reset screen that we need.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when screen is exited.
    virtual void OnExit() = 0;

    // This method is called, when actor is being destroyed. Note, if Delegate
    // is destroyed earlier then it has to call SetDelegate(NULL).
    virtual void OnActorDestroyed(KioskAutostartScreenActor* actor) = 0;
  };

  virtual ~KioskAutostartScreenActor() {}

  virtual void PrepareToShow() = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void SetDelegate(Delegate* delegate) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_KIOSK_AUTOSTART_SCREEN_ACTOR_H_
