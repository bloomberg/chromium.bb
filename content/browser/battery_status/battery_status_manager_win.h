// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BATTERY_STATUS_BATTERY_STATUS_MANAGER_WIN_H_
#define CHROME_BROWSER_BATTERY_STATUS_BATTERY_STATUS_MANAGER_WIN_H_

#include <windows.h>
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebBatteryStatus.h"

namespace content {

enum WinACLineStatus {
  WIN_AC_LINE_STATUS_OFFLINE = 0,
  WIN_AC_LINE_STATUS_ONLINE = 1,
  WIN_AC_LINE_STATUS_UNKNOWN = 255,
};

// Returns WebBatteryStatus corresponding to the given SYSTEM_POWER_STATUS.
CONTENT_EXPORT blink::WebBatteryStatus ComputeWebBatteryStatus(
    const SYSTEM_POWER_STATUS& win_status);

}  // namespace content

#endif  // CHROME_BROWSER_BATTERY_STATUS_BATTERY_STATUS_MANAGER_WIN_H_