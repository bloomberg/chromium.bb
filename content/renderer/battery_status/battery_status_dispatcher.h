// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BATTERY_STATUS_BATTERY_STATUS_DISPATCHER_H_
#define CONTENT_RENDERER_BATTERY_STATUS_BATTERY_STATUS_DISPATCHER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "device/battery/battery_monitor.mojom.h"

namespace blink {
class WebBatteryStatusListener;
}

namespace content {

class CONTENT_EXPORT BatteryStatusDispatcher {
 public:
  explicit BatteryStatusDispatcher(blink::WebBatteryStatusListener* listener);
  ~BatteryStatusDispatcher();

 private:
  friend class BatteryStatusDispatcherTest;

  void QueryNextStatus();
  void DidChange(device::BatteryStatusPtr battery_status);

  device::BatteryMonitorPtr monitor_;
  blink::WebBatteryStatusListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_BATTERY_STATUS_BATTERY_STATUS_DISPATCHER_H_
