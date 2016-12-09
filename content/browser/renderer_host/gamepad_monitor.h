// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_GAMEPAD_MONITOR_H_
#define CONTENT_BROWSER_RENDERER_HOST_GAMEPAD_MONITOR_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "device/gamepad/gamepad_consumer.h"
#include "device/gamepad/public/interfaces/gamepad.mojom.h"

namespace content {

class GamepadMonitor : public device::GamepadConsumer,
                       public device::mojom::GamepadMonitor {
 public:
  GamepadMonitor();
  ~GamepadMonitor() override;

  static void Create(device::mojom::GamepadMonitorRequest request);

  // GamepadConsumer implementation.
  void OnGamepadConnected(unsigned index,
                          const blink::WebGamepad& gamepad) override;
  void OnGamepadDisconnected(unsigned index,
                             const blink::WebGamepad& gamepad) override;

  // device::mojom::GamepadMonitor implementation.
  void GamepadStartPolling(
      const GamepadStartPollingCallback& callback) override;
  void GamepadStopPolling(const GamepadStopPollingCallback& callback) override;
  void SetObserver(device::mojom::GamepadObserverPtr gamepad_observer) override;

 private:
  device::mojom::GamepadObserverPtr gamepad_observer_;
  bool is_started_;

  DISALLOW_COPY_AND_ASSIGN(GamepadMonitor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_GAMEPAD_MONITOR_H_
