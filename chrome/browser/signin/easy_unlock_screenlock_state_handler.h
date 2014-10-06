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
    // An Easy Unlock enabled phone is found, but it is not allowed to unlock
    // the device because it does not support reporting it's lock screen state.
    STATE_PHONE_UNSUPPORTED,
    // The device can be unlocked using Easy Unlock.
    STATE_AUTHENTICATED
  };

  // Hard lock states.
  enum HardlockState {
    NO_HARDLOCK = 0,           // Hard lock is not enforced. This is default.
    USER_HARDLOCK = 1 << 0,    // Hard lock is requested by user.
    PAIRING_CHANGED = 1 << 1,  // Hard lock because pairing data is changed.
    NO_PAIRING = 1 << 2        // Hard lock because there is no pairing data.
  };

  // |user_email|: The email for the user associated with the profile to which
  //     this class is attached.
  // |pref_service|: The profile preferences.
  // |screenlock_bridge|: The screenlock bridge used to update the screen lock
  //     state.
  EasyUnlockScreenlockStateHandler(const std::string& user_email,
                                   HardlockState initial_hardlock_state,
                                   PrefService* pref_service,
                                   ScreenlockBridge* screenlock_bridge);
  virtual ~EasyUnlockScreenlockStateHandler();

  // Changes internal state to |new_state| and updates the user's screenlock
  // accordingly.
  void ChangeState(State new_state);

  // Updates the screenlock state.
  void SetHardlockState(HardlockState new_state);

  // Shows the hardlock UI if the hardlock_state_ is not NO_HARDLOCK.
  void MaybeShowHardlockUI();

 private:
  // ScreenlockBridge::Observer:
  virtual void OnScreenDidLock() OVERRIDE;
  virtual void OnScreenDidUnlock() OVERRIDE;
  virtual void OnFocusedUserChanged(const std::string& user_id) OVERRIDE;

  void ShowHardlockUI();

  // Updates icon's tooltip options.
  // |trial_run|: Whether the trial Easy Unlock run is in progress.
  void UpdateTooltipOptions(
      bool trial_run,
      ScreenlockBridge::UserPodCustomIconOptions* icon_options);

  // Whether this is the first, trial Easy Unlock run. If this is the case, a
  // tutorial message should be shown and hard-locking be disabled in
  // Authenticated state. The trial run will be active if Easy Unlock never
  // entered Authenticated state (across sessions).
  bool IsTrialRun();

  // Sets user preference that marks trial run completed.
  void MarkTrialRunComplete();

  // Gets the name to be used for the device. The name depends on the device
  // type (example values: Chromebook and Chromebox).
  base::string16 GetDeviceName();

  // Updates the screenlock auth type if it has to be changed.
  void UpdateScreenlockAuthType();

  State state_;
  std::string user_email_;
  PrefService* pref_service_;
  ScreenlockBridge* screenlock_bridge_;

  // State of hardlock.
  HardlockState hardlock_state_;
  bool hardlock_ui_shown_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockScreenlockStateHandler);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SCREENLOCK_STATE_HANDLER_H_
