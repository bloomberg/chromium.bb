// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BATTERY_STATUS_BATTERY_STATUS_UPDATE_CALLBACK_H_
#define CONTENT_BROWSER_BATTERY_STATUS_BATTERY_STATUS_UPDATE_CALLBACK_H_

#include "base/callback.h"
#include "third_party/WebKit/public/platform/WebBatteryStatus.h"

namespace content {

typedef base::Callback<void(const blink::WebBatteryStatus&)>
    BatteryStatusUpdateCallback;

}  // namespace content

#endif  // CONTENT_BROWSER_BATTERY_STATUS_BATTERY_STATUS_UPDATE_CALLBACK_H_
