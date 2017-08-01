// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PREF_MANAGER_H
#define COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PREF_MANAGER_H

#include <string>
#include <vector>

#include "base/macros.h"

namespace proximity_auth {

// Interface for setting and getting persistent user preferences. There is an
// implementation for a logged in profile and the device local state when the
// user is logged out.
class ProximityAuthPrefManager {
 public:
  ProximityAuthPrefManager() {}
  virtual ~ProximityAuthPrefManager() {}

  // Returns true if EasyUnlock is allowed. Note: there is no corresponding
  // setter because this pref is pushed through an enterprise policy.
  virtual bool IsEasyUnlockAllowed() const = 0;

  // Setter and getter for the timestamp of the last password entry. This
  // preference is used to enforce reauthing with the password after a given
  // time period has elapsed.
  virtual void SetLastPasswordEntryTimestampMs(int64_t timestamp_ms) = 0;
  virtual int64_t GetLastPasswordEntryTimestampMs() const = 0;

  // Setter and getter for the timestamp of the last time the promotion was
  // shown to the user.
  virtual void SetLastPromotionCheckTimestampMs(int64_t timestamp_ms) = 0;
  virtual int64_t GetLastPromotionCheckTimestampMs() const = 0;

  // Setter and getter for the number of times the promotion was shown to the
  // user.
  virtual void SetPromotionShownCount(int count) = 0;
  virtual int GetPromotionShownCount() const = 0;

  // These are arbitrary labels displayed in the settings page for the user
  // to select. The actual mapping is done in the ProximityMonitorImpl.
  enum ProximityThreshold {
    kVeryClose = 0,
    kClose = 1,
    kFar = 2,
    kVeryFar = 3
  };

  // Setter and getter for the proximity threshold. This preference is
  // exposed to the user, allowing him / her to change how close the
  // phone must for the unlock to be allowed.
  // Note: These are arbitrary values,
  virtual void SetProximityThreshold(ProximityThreshold value) = 0;
  virtual ProximityThreshold GetProximityThreshold() const = 0;

  // Setting and getter for whether EasyUnlock is enabled for ChromeOS login (in
  // addition to screen lock).
  virtual void SetIsChromeOSLoginEnabled(bool is_enabled) = 0;
  virtual bool IsChromeOSLoginEnabled() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProximityAuthPrefManager);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_PROXIMITY_AUTH_PREF_MANAGER_H
