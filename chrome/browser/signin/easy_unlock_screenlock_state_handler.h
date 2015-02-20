// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SCREENLOCK_STATE_HANDLER_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SCREENLOCK_STATE_HANDLER_H_

#include <string>

#include "base/strings/string16.h"
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
    // An Easy Unlock enabled phone is found, but it is not allowed to unlock
    // the device because it does not support reporting it's lock screen state.
    STATE_PHONE_UNSUPPORTED,
    // A phone eligible to unlock the device is found, but its received signal
    // strength is too low, i.e. the phone is roughly more than 30 feet away,
    // and therefore is not allowed to unlock the device.
    STATE_RSSI_TOO_LOW,
    // A phone eligible to unlock the device is found, but the local device's
    // transmission power is too high, indicating that the phone is (probably)
    // more than 1 foot away, and therefore is not allowed to unlock the device.
    STATE_TX_POWER_TOO_HIGH,
    // A phone eligible to unlock the device is found; but (a) the phone is
    // locked, and (b) the local device's transmission power is too high,
    // indicating that the phone is (probably) more than 1 foot away, and
    // therefore is not allowed to unlock the device.
    STATE_PHONE_LOCKED_AND_TX_POWER_TOO_HIGH,
    // The device can be unlocked using Easy Unlock.
    STATE_AUTHENTICATED
  };

  // Hard lock states.
  enum HardlockState {
    NO_HARDLOCK = 0,           // Hard lock is not enforced. This is default.
    USER_HARDLOCK = 1 << 0,    // Hard lock is requested by user.
    PAIRING_CHANGED = 1 << 1,  // Hard lock because pairing data is changed.
    NO_PAIRING = 1 << 2,       // Hard lock because there is no pairing data.
    LOGIN_FAILED = 1 << 3,     // Transient hard lock caused by login attempt
                               // failure. Reset when screen is unlocked.
    PAIRING_ADDED = 1 << 4,    // Similar to PAIRING_CHANGED when it happens
                               // on a new Chromebook.
  };

  // |user_email|: The email for the user associated with the profile to which
  //     this class is attached.
  // |initial_hardlock_state|: The initial hardlock state.
  // |screenlock_bridge|: The screenlock bridge used to update the screen lock
  //     state.
  EasyUnlockScreenlockStateHandler(const std::string& user_email,
                                   HardlockState initial_hardlock_state,
                                   ScreenlockBridge* screenlock_bridge);
  ~EasyUnlockScreenlockStateHandler() override;

  // Returns true if handler is not in INACTIVE state.
  bool IsActive() const;

  // Whether the handler is in state that is allowed just after auth failure
  // (i.e. the state that would cause auth failure rather than one caused by an
  // auth failure).
  bool InStateValidOnRemoteAuthFailure() const;

  // Changes internal state to |new_state| and updates the user's screenlock
  // accordingly.
  void ChangeState(State new_state);

  // Updates the screenlock state.
  void SetHardlockState(HardlockState new_state);

  // Shows the hardlock UI if the hardlock_state_ is not NO_HARDLOCK.
  void MaybeShowHardlockUI();

  // Marks the current screenlock state as the one for trial Easy Unlock run.
  void SetTrialRun();

  // Records that the user clicked on the lock icon during the trial run
  // initiated by the Easy Unlock app.
  void RecordClickOnLockIcon();

  State state() const { return state_; }

 private:
  // ScreenlockBridge::Observer:
  void OnScreenDidLock(
      ScreenlockBridge::LockHandler::ScreenType screen_type) override;
  void OnScreenDidUnlock(
      ScreenlockBridge::LockHandler::ScreenType screen_type) override;
  void OnFocusedUserChanged(const std::string& user_id) override;

  // Forces refresh of the Easy Unlock screenlock UI.
  void RefreshScreenlockState();

  void ShowHardlockUI();

  // Updates icon's tooltip options.
  void UpdateTooltipOptions(
      ScreenlockBridge::UserPodCustomIconOptions* icon_options);

  // Gets the name to be used for the device. The name depends on the device
  // type (example values: Chromebook and Chromebox).
  base::string16 GetDeviceName();

  // Updates the screenlock auth type if it has to be changed.
  void UpdateScreenlockAuthType();

  State state_;
  std::string user_email_;
  ScreenlockBridge* screenlock_bridge_;

  // State of hardlock.
  HardlockState hardlock_state_;
  bool hardlock_ui_shown_;

  // Whether this is the trial Easy Unlock run. If this is the case, a
  // tutorial message should be shown and hard-locking be disabled. The trial
  // run should be set if the screen was locked by the Easy Unlock setup app.
  bool is_trial_run_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockScreenlockStateHandler);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SCREENLOCK_STATE_HANDLER_H_
