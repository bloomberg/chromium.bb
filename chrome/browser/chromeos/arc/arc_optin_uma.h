// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_OPTIN_UMA_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_OPTIN_UMA_H_

namespace arc {

// These enums are used to define the buckets for an enumerated UMA histogram
// and need to be synced with histograms.xml
enum class OptInActionType : int {
  OPTED_OUT = 0,               // Arc was opted out by user.
  OPTED_IN = 1,                // Arc was opted in by user.
  NOTIFICATION_ACCEPTED = 2,   // Arc OptIn notification was accepted.
  NOTIFICATION_DECLINED = 3,   // Arc OptIn notification was declined.
  NOTIFICATION_TIMED_OUT = 4,  // Arc OptIn notification was timed out.
  RETRY = 5,                   // User asked to retry OptIn.
  SIZE,                        // The size of this enum; keep last.
};

enum class OptInCancelReason {
  USER_CANCEL = 0,                  // Canceled by user.
  UNKNOWN_ERROR = 1,                // Unclassified failure.
  NETWORK_ERROR = 2,                // Network failure.
  SERVICE_UNAVAILABLE = 3,          // GMS Services are not available.
  BAD_AUTHENTICATION = 4,           // Bad authentication returned by server.
  GMS_CORE_NOT_AVAILABLE = 5,       // GMS Core is not available.
  CLOUD_PROVISION_FLOW_FAIL = 6,    // Cloud provision flow failed.
  ANDROID_MANAGEMENT_REQUIRED = 7,  // Android management is required for user.
  SIZE,                             // The size of this enum; keep last.
};

void UpdateOptInActionUMA(OptInActionType type);
void UpdateOptInCancelUMA(OptInCancelReason reason);
void UpdateEnabledStateUMA(bool enabled);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_OPTIN_UMA_H_
