// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_METRICS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_METRICS_H_

namespace chromeos {

// Tracking login events for Easy unlock metrics.
// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
enum EasyUnlockLoginEvent {
  // User is successfully authenticated using Easy Sign-in.
  EASY_SIGN_IN_SUCCESS = 0,
  // Easy sign-in failed to authenticate the user.
  EASY_SIGN_IN_FAILURE = 1,

  // Password is used for sign-in because there is no pairing data.
  PASSWORD_SIGN_IN_NO_PAIRING = 2,
  // Password is used for sign-in because pairing data is changed.
  PASSWORD_SIGN_IN_PAIRING_CHANGED = 3,
  // Password is used for sign-in because of user hardlock.
  PASSWORD_SIGN_IN_USER_HARDLOCK = 4,
  // Password is used for sign-in because Easy unlock service is not active.
  PASSWORD_SIGN_IN_SERVICE_NOT_ACTIVE = 5,
  // Password is used for sign-in because Bluetooth is not on.
  PASSWORD_SIGN_IN_NO_BLUETOOTH = 6,
  // Password is used for sign-in because Easy unlock is connecting.
  PASSWORD_SIGN_IN_BLUETOOTH_CONNECTING = 7,
  // Password is used for sign-in because no eligible phones found.
  PASSWORD_SIGN_IN_NO_PHONE = 8,
  // Password is used for sign-in because phone could not be authenticated.
  PASSWORD_SIGN_IN_PHONE_NOT_AUTHENTICATED = 9,
  // Password is used for sign-in because phone is locked.
  PASSWORD_SIGN_IN_PHONE_LOCKED = 10,
  // Password is used for sign-in because phone does not have lock screen.
  PASSWORD_SIGN_IN_PHONE_NOT_LOCKABLE = 11,
  // Password is used for sign-in because phone is not close enough.
  PASSWORD_SIGN_IN_PHONE_NOT_NEARBY = 12,
  // Password is used for sign-in because phone is not supported.
  PASSWORD_SIGN_IN_PHONE_UNSUPPORTED = 13,
  // Password is used for sign-in because user types in passowrd. This is
  // unlikely to happen though.
  PASSWORD_SIGN_IN_WITH_AUTHENTICATED_PHONE = 14,

  EASY_SIGN_IN_LOGIN_EVENT_COUNT  // Must be the last.
};

void RecordEasyUnlockLoginEvent(EasyUnlockLoginEvent event);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_METRICS_H_
