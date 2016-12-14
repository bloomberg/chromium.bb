// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_service.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/gamepad/gamepad_consumer.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_provider.h"

namespace device {

namespace {
GamepadService* g_gamepad_service = 0;
}

GamepadService::GamepadService()
    : main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      num_active_consumers_(0),
      gesture_callback_pending_(false) {
  SetInstance(this);
}

GamepadService::GamepadService(
    std::unique_ptr<device::GamepadDataFetcher> fetcher)
    : provider_(new device::GamepadProvider(this, std::move(fetcher))),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      num_active_consumers_(0),
      gesture_callback_pending_(false) {
  SetInstance(this);
}

GamepadService::~GamepadService() {
  SetInstance(NULL);
}

void GamepadService::SetInstance(GamepadService* instance) {
  // Unit tests can create multiple instances but only one should exist at any
  // given time so g_gamepad_service should only go from NULL to non-NULL and
  // vica versa.
  CHECK(!!instance != !!g_gamepad_service);
  g_gamepad_service = instance;
}

GamepadService* GamepadService::GetInstance() {
  if (!g_gamepad_service)
    g_gamepad_service = new GamepadService;
  return g_gamepad_service;
}

void GamepadService::ConsumerBecameActive(device::GamepadConsumer* consumer) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  if (!provider_)
    provider_.reset(new device::GamepadProvider(this));

  std::pair<ConsumerSet::iterator, bool> insert_result =
      consumers_.insert(consumer);
  insert_result.first->is_active = true;
  if (!insert_result.first->did_observe_user_gesture &&
      !gesture_callback_pending_) {
    gesture_callback_pending_ = true;
    provider_->RegisterForUserGesture(
        base::Bind(&GamepadService::OnUserGesture, base::Unretained(this)));
  }

  if (num_active_consumers_++ == 0)
    provider_->Resume();
}

void GamepadService::ConsumerBecameInactive(device::GamepadConsumer* consumer) {
  DCHECK(provider_);
  DCHECK(num_active_consumers_ > 0);
  DCHECK(consumers_.count(consumer) > 0);
  DCHECK(consumers_.find(consumer)->is_active);

  consumers_.find(consumer)->is_active = false;
  if (--num_active_consumers_ == 0)
    provider_->Pause();
}

void GamepadService::RemoveConsumer(device::GamepadConsumer* consumer) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  ConsumerSet::iterator it = consumers_.find(consumer);
  if (it->is_active && --num_active_consumers_ == 0)
    provider_->Pause();
  consumers_.erase(it);
}

void GamepadService::RegisterForUserGesture(const base::Closure& closure) {
  DCHECK(consumers_.size() > 0);
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  provider_->RegisterForUserGesture(closure);
}

void GamepadService::Terminate() {
  provider_.reset();
}

void GamepadService::OnGamepadConnectionChange(bool connected,
                                               int index,
                                               const blink::WebGamepad& pad) {
  if (connected) {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&GamepadService::OnGamepadConnected,
                              base::Unretained(this), index, pad));
  } else {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&GamepadService::OnGamepadDisconnected,
                              base::Unretained(this), index, pad));
  }
}

void GamepadService::OnGamepadConnected(int index,
                                        const blink::WebGamepad& pad) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  for (ConsumerSet::iterator it = consumers_.begin(); it != consumers_.end();
       ++it) {
    if (it->did_observe_user_gesture && it->is_active)
      it->consumer->OnGamepadConnected(index, pad);
  }
}

void GamepadService::OnGamepadDisconnected(int index,
                                           const blink::WebGamepad& pad) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  for (ConsumerSet::iterator it = consumers_.begin(); it != consumers_.end();
       ++it) {
    if (it->did_observe_user_gesture && it->is_active)
      it->consumer->OnGamepadDisconnected(index, pad);
  }
}

base::SharedMemoryHandle GamepadService::GetSharedMemoryHandleForProcess(
    base::ProcessHandle handle) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  return provider_->GetSharedMemoryHandleForProcess(handle);
}

mojo::ScopedSharedBufferHandle GamepadService::GetSharedBufferHandle() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  return provider_->GetSharedBufferHandle();
}

void GamepadService::OnUserGesture() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  gesture_callback_pending_ = false;

  if (!provider_ || num_active_consumers_ == 0)
    return;

  for (ConsumerSet::iterator it = consumers_.begin(); it != consumers_.end();
       ++it) {
    if (!it->did_observe_user_gesture && it->is_active) {
      const ConsumerInfo& info = *it;
      info.did_observe_user_gesture = true;
      blink::WebGamepads gamepads;
      provider_->GetCurrentGamepadData(&gamepads);
      for (unsigned i = 0; i < blink::WebGamepads::itemsLengthCap; ++i) {
        const blink::WebGamepad& pad = gamepads.items[i];
        if (pad.connected)
          info.consumer->OnGamepadConnected(i, pad);
      }
    }
  }
}

void GamepadService::SetSanitizationEnabled(bool sanitize) {
  provider_->SetSanitizationEnabled(sanitize);
}

}  // namespace device
