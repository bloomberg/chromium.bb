// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SCREENLOCK_STATE_HANDLER_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SCREENLOCK_STATE_HANDLER_H_

#include <string>

#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "chrome/browser/signin/screenlock_bridge.h"

class PrefService;

// Profile specific class responsible for updating screenlock UI for the user
// associated with the profile when their Easy Unlock state changes.
class EasyUnlockScreenlockStateHandler : public ScreenlockBridge::Observer {
 public:
  // Available Easy Unlock states.
  enum State {
    // Easy Unlock is not enabled, or the screen is not locked.
    STATE_INACTIVE,
    // Bluetooth is not on.
    STATE_NO_BLUETOOTH,
    // Easy Unlock is in process of turning on Bluetooth.
    STATE_BLUETOOTH_CONNECTING,
    // No phones eligible to unlock the device can be found.
    STATE_NO_PHONE,
    // A phone eligible to unlock the device is found, but cannot be
    // authenticated.
    STATE_PHONE_NOT_AUTHENTICATED,
    // A phone eligible to unlock the device is found, but it's locked.
    STATE_PHONE_LOCKED,
    // A phone eligible to unlock the device is found, but does not have lock
    // screen enabled.
    STATE_PHONE_UNLOCKABLE,
    // A phone eligible to unlock the device is found, but it's not close enough
    // to be allowed to unlock the device.
    STATE_PHONE_NOT_NEARBY,
    // The device can be unlocked using Easy Unlock.
    STATE_AUTHENTICATED
  };

  // |user_email|: The email for the user associated with the profile to which
  //     this class is attached.
  // |pref_service|: The profile preferences.
  // |screenlock_bridge|: The screenlock bridge used to update the screen lock
  //     state.
  EasyUnlockScreenlockStateHandler(const std::string& user_email,
                                   PrefService* pref_service,
                                   ScreenlockBridge* screenlock_bridge);
  virtual ~EasyUnlockScreenlockStateHandler();

  // Changes internal state to |new_state| and updates the user's screenlock
  // accordingly.
  void ChangeState(State new_state);

 private:
  // ScreenlockBridge::Observer:
  virtual void OnScreenDidLock() OVERRIDE;
  virtual void OnScreenDidUnlock() OVERRIDE;

  void UpdateTooltipOptions(
      ScreenlockBridge::UserPodCustomIconOptions* icon_options);

  // Whether the tutorial message should be shown to the user. The message is
  // shown only once, when the user encounters STATE_AUTHENTICATED for the first
  // time (across sessions). After the tutorial message is shown,
  // |MarkTutorialShown| should be called to prevent further tutorial message.
  bool ShouldShowTutorial();

  // Sets user preference that prevents showing of tutorial messages.
  void MarkTutorialShown();

  // Gets the name to be used for the device. The name depends on the device
  // type (example values: Chromebook and Chromebox).
  base::string16 GetDeviceName();

  // Updates the screenlock auth type if it has to be changed.
  void UpdateScreenlockAuthType();

  State state_;
  std::string user_email_;
  PrefService* pref_service_;
  ScreenlockBridge* screenlock_bridge_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockScreenlockStateHandler);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SCREENLOCK_STATE_HANDLER_H_
