// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/gamepad_monitor.h"

#include "base/memory/shared_memory.h"
#include "content/browser/gamepad/gamepad_service.h"
#include "content/common/gamepad_hardware_buffer.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

GamepadMonitor::GamepadMonitor() : is_started_(false) {}

GamepadMonitor::~GamepadMonitor() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (is_started_)
    GamepadService::GetInstance()->RemoveConsumer(this);
}

// static
void GamepadMonitor::Create(device::mojom::GamepadMonitorRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<GamepadMonitor>(),
                          std::move(request));
}

void GamepadMonitor::OnGamepadConnected(unsigned index,
                                        const blink::WebGamepad& gamepad) {
  if (gamepad_observer_)
    gamepad_observer_->GamepadConnected(index, gamepad);
}

void GamepadMonitor::OnGamepadDisconnected(unsigned index,
                                           const blink::WebGamepad& gamepad) {
  if (gamepad_observer_)
    gamepad_observer_->GamepadDisconnected(index, gamepad);
}

void GamepadMonitor::GamepadStartPolling(
    const GamepadStartPollingCallback& callback) {
  DCHECK(!is_started_);
  is_started_ = true;

  GamepadService* service = GamepadService::GetInstance();
  service->ConsumerBecameActive(this);
  callback.Run(service->GetSharedBufferHandle());
}

void GamepadMonitor::GamepadStopPolling(
    const GamepadStopPollingCallback& callback) {
  DCHECK(is_started_);
  is_started_ = false;

  GamepadService::GetInstance()->ConsumerBecameInactive(this);
  callback.Run();
}

void GamepadMonitor::SetObserver(
    device::mojom::GamepadObserverPtr gamepad_observer) {
  gamepad_observer_ = std::move(gamepad_observer);
}

}  // namespace content
