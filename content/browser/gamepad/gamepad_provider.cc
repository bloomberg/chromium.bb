// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/gamepad/gamepad_data_fetcher.h"
#include "content/browser/gamepad/gamepad_provider.h"
#include "content/browser/gamepad/gamepad_platform_data_fetcher.h"
#include "content/common/gamepad_hardware_buffer.h"
#include "content/common/gamepad_messages.h"
#include "content/common/gamepad_user_gesture.h"

namespace content {

GamepadProvider::ClosureAndThread::ClosureAndThread(
    const base::Closure& c,
    const scoped_refptr<base::MessageLoopProxy>& m)
    : closure(c),
      message_loop(m) {
}

GamepadProvider::ClosureAndThread::~ClosureAndThread() {
}

GamepadProvider::GamepadProvider()
    : is_paused_(true),
      have_scheduled_do_poll_(false),
      devices_changed_(true) {
  Initialize(scoped_ptr<GamepadDataFetcher>());
}

GamepadProvider::GamepadProvider(scoped_ptr<GamepadDataFetcher> fetcher)
    : is_paused_(true),
      have_scheduled_do_poll_(false),
      devices_changed_(true) {
  Initialize(fetcher.Pass());
}

GamepadProvider::~GamepadProvider() {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->RemoveDevicesChangedObserver(this);

  polling_thread_.reset();
  data_fetcher_.reset();
}

base::SharedMemoryHandle GamepadProvider::GetSharedMemoryHandleForProcess(
    base::ProcessHandle process) {
  base::SharedMemoryHandle renderer_handle;
  gamepad_shared_memory_.ShareToProcess(process, &renderer_handle);
  return renderer_handle;
}

void GamepadProvider::Pause() {
  {
    base::AutoLock lock(is_paused_lock_);
    is_paused_ = true;
  }
  MessageLoop* polling_loop = polling_thread_->message_loop();
  polling_loop->PostTask(
      FROM_HERE,
      base::Bind(&GamepadProvider::SendPauseHint, Unretained(this), true));
}

void GamepadProvider::Resume() {
  {
    base::AutoLock lock(is_paused_lock_);
    if (!is_paused_)
        return;
    is_paused_ = false;
  }

  MessageLoop* polling_loop = polling_thread_->message_loop();
  polling_loop->PostTask(
      FROM_HERE,
      base::Bind(&GamepadProvider::SendPauseHint, Unretained(this), false));
  polling_loop->PostTask(
      FROM_HERE,
      base::Bind(&GamepadProvider::ScheduleDoPoll, Unretained(this)));
}

void GamepadProvider::RegisterForUserGesture(const base::Closure& closure) {
  base::AutoLock lock(user_gesture_lock_);
  user_gesture_observers_.push_back(
      ClosureAndThread(closure, MessageLoop::current()->message_loop_proxy()));
}

void GamepadProvider::OnDevicesChanged(base::SystemMonitor::DeviceType type) {
  base::AutoLock lock(devices_changed_lock_);
  devices_changed_ = true;
}

void GamepadProvider::Initialize(scoped_ptr<GamepadDataFetcher> fetcher) {
  size_t data_size = sizeof(GamepadHardwareBuffer);
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->AddDevicesChangedObserver(this);
  bool res = gamepad_shared_memory_.CreateAndMapAnonymous(data_size);
  CHECK(res);
  GamepadHardwareBuffer* hwbuf = SharedMemoryAsHardwareBuffer();
  memset(hwbuf, 0, sizeof(GamepadHardwareBuffer));

  polling_thread_.reset(new base::Thread("Gamepad polling thread"));
  polling_thread_->StartWithOptions(
      base::Thread::Options(MessageLoop::TYPE_IO, 0));

  polling_thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&GamepadProvider::DoInitializePollingThread,
                 base::Unretained(this),
                 base::Passed(&fetcher)));
}

void GamepadProvider::DoInitializePollingThread(
    scoped_ptr<GamepadDataFetcher> fetcher) {
  DCHECK(MessageLoop::current() == polling_thread_->message_loop());
  DCHECK(!data_fetcher_.get());  // Should only initialize once.

  if (!fetcher.get())
    fetcher.reset(new GamepadPlatformDataFetcher);
  data_fetcher_ = fetcher.Pass();
}

void GamepadProvider::SendPauseHint(bool paused) {
  DCHECK(MessageLoop::current() == polling_thread_->message_loop());
  if (data_fetcher_.get())
    data_fetcher_->PauseHint(paused);
}

void GamepadProvider::DoPoll() {
  DCHECK(MessageLoop::current() == polling_thread_->message_loop());
  DCHECK(have_scheduled_do_poll_);
  have_scheduled_do_poll_ = false;

  bool changed;
  GamepadHardwareBuffer* hwbuf = SharedMemoryAsHardwareBuffer();

  ANNOTATE_BENIGN_RACE_SIZED(
      &hwbuf->buffer,
      sizeof(WebKit::WebGamepads),
      "Racey reads are discarded");

  {
    base::AutoLock lock(devices_changed_lock_);
    changed = devices_changed_;
    devices_changed_ = false;
  }

  // Acquire the SeqLock. There is only ever one writer to this data.
  // See gamepad_hardware_buffer.h.
  hwbuf->sequence.WriteBegin();
  data_fetcher_->GetGamepadData(&hwbuf->buffer, changed);
  hwbuf->sequence.WriteEnd();

  CheckForUserGesture();

  // Schedule our next interval of polling.
  ScheduleDoPoll();
}

void GamepadProvider::ScheduleDoPoll() {
  DCHECK(MessageLoop::current() == polling_thread_->message_loop());
  if (have_scheduled_do_poll_)
    return;

  {
    base::AutoLock lock(is_paused_lock_);
    if (is_paused_)
      return;
  }

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&GamepadProvider::DoPoll, Unretained(this)),
      base::TimeDelta::FromMilliseconds(kDesiredSamplingIntervalMs));
  have_scheduled_do_poll_ = true;
}

GamepadHardwareBuffer* GamepadProvider::SharedMemoryAsHardwareBuffer() {
  void* mem = gamepad_shared_memory_.memory();
  CHECK(mem);
  return static_cast<GamepadHardwareBuffer*>(mem);
}

void GamepadProvider::CheckForUserGesture() {
  base::AutoLock lock(user_gesture_lock_);
  if (user_gesture_observers_.empty())
    return;  // Don't need to check if nobody is listening.

  if (GamepadsHaveUserGesture(SharedMemoryAsHardwareBuffer()->buffer)) {
    for (size_t i = 0; i < user_gesture_observers_.size(); i++) {
      user_gesture_observers_[i].message_loop->PostTask(FROM_HERE,
          user_gesture_observers_[i].closure);
    }
    user_gesture_observers_.clear();
  }
}

}  // namespace content
