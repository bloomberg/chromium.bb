// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_METRICS_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_METRICS_HELPER_H_

namespace chromeos {
namespace kiosk_next_home {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BridgeAction {
  kListApps = 0,
  kLaunchApp = 1,
  kUninstallApp = 2,
  kNotifiedAppChange = 3,
  kGetAndroidId = 4,
  kLaunchIntent = 5,
  kGetUserInfo = 6,
  kMaxValue = kGetUserInfo,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LaunchIntentResult {
  kSuccess = 0,
  kNotAllowed = 1,
  kArcUnavailable = 2,
  kMaxValue = kArcUnavailable,
};

// Records KioskNextHome bridge method calls.
void RecordBridgeAction(BridgeAction action);

}  // namespace kiosk_next_home
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_KIOSK_NEXT_HOME_METRICS_HELPER_H_
