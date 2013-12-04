// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_SCREEN_ACTOR_H_

#include "base/strings/string16.h"

namespace chromeos {

// Interface for dependency injection between NetworkScreen and its actual
// representation, either views based or WebUI. Owned by NetworkScreen.
class NetworkScreenActor {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnActorDestroyed(NetworkScreenActor* actor) = 0;
    virtual void OnContinuePressed() = 0;
  };

  virtual ~NetworkScreenActor() {}

  // Sets screen this actor belongs to.
  virtual void SetDelegate(Delegate* screen) = 0;

  // Prepare the contents to showing.
  virtual void PrepareToShow() = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Shows error message in a bubble.
  virtual void ShowError(const base::string16& message) = 0;

  // Hides error messages showing no error state.
  virtual void ClearErrors() = 0;

  // Shows network connecting status or network selection otherwise.
  virtual void ShowConnectingStatus(
      bool connecting,
      const base::string16& network_id) = 0;

  // Sets whether continue control is enabled.
  virtual void EnableContinue(bool enabled) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_SCREEN_ACTOR_H_
