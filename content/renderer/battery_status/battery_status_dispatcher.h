// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BATTERY_STATUS_BATTERY_STATUS_DISPATCHER_H_
#define CONTENT_RENDERER_BATTERY_STATUS_BATTERY_STATUS_DISPATCHER_H_

#include "content/public/renderer/platform_event_observer.h"

namespace blink {
class WebBatteryStatus;
class WebBatteryStatusListener;
}

namespace content {
class RenderThread;

class CONTENT_EXPORT BatteryStatusDispatcher
    : NON_EXPORTED_BASE(
          public PlatformEventObserver<blink::WebBatteryStatusListener>) {
 public:
  explicit BatteryStatusDispatcher(RenderThread* thread);
  virtual ~BatteryStatusDispatcher();

  // PlatformEventObserver public methods.
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void SendFakeDataForTesting(void* data) OVERRIDE;

 protected:
  // PlatformEventObserver protected methods.
  virtual void SendStartMessage() OVERRIDE;
  virtual void SendStopMessage() OVERRIDE;

 private:
  void OnDidChange(const blink::WebBatteryStatus& status);

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_BATTERY_STATUS_BATTERY_STATUS_DISPATCHER_H_
