// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/gamepad_provider.h"

#include <stddef.h>
#include <string.h>
#include <cmath>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/gamepad/gamepad_data_fetcher.h"
#include "content/browser/gamepad/gamepad_platform_data_fetcher.h"
#include "content/browser/gamepad/gamepad_service.h"
#include "content/common/gamepad_hardware_buffer.h"
#include "content/common/gamepad_messages.h"
#include "content/common/gamepad_user_gesture.h"
#include "content/public/browser/browser_thread.h"

using blink::WebGamepad;
using blink::WebGamepads;

namespace {

const float kMinAxisResetValue = 0.1f;

} // namespace

namespace content {

GamepadProvider::ClosureAndThread::ClosureAndThread(
    const base::Closure& c,
    const scoped_refptr<base::SingleThreadTaskRunner>& m)
    : closure(c), task_runner(m) {
}

GamepadProvider::ClosureAndThread::~ClosureAndThread() {
}

GamepadProvider::GamepadProvider()
    : is_paused_(true),
      have_scheduled_do_poll_(false),
      devices_changed_(true),
      ever_had_user_gesture_(false),
      sanitize_(true) {
  Initialize(scoped_ptr<GamepadDataFetcher>());
}

GamepadProvider::GamepadProvider(scoped_ptr<GamepadDataFetcher> fetcher)
    : is_paused_(true),
      have_scheduled_do_poll_(false),
      devices_changed_(true),
      ever_had_user_gesture_(false) {
  Initialize(std::move(fetcher));
}

GamepadProvider::~GamepadProvider() {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->RemoveDevicesChangedObserver(this);

  // Use Stop() to join the polling thread, as there may be pending callbacks
  // which dereference |polling_thread_|.
  polling_thread_->Stop();
}

base::SharedMemoryHandle GamepadProvider::GetSharedMemoryHandleForProcess(
    base::ProcessHandle process) {
  base::SharedMemoryHandle renderer_handle;
  gamepad_shared_memory_.ShareToProcess(process, &renderer_handle);
  return renderer_handle;
}

void GamepadProvider::GetCurrentGamepadData(WebGamepads* data) {
  const WebGamepads& pads = SharedMemoryAsHardwareBuffer()->buffer;
  base::AutoLock lock(shared_memory_lock_);
  *data = pads;
}

void GamepadProvider::Pause() {
  {
    base::AutoLock lock(is_paused_lock_);
    is_paused_ = true;
  }
  base::MessageLoop* polling_loop = polling_thread_->message_loop();
  polling_loop->task_runner()->PostTask(
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

  base::MessageLoop* polling_loop = polling_thread_->message_loop();
  polling_loop->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&GamepadProvider::SendPauseHint, Unretained(this), false));
  polling_loop->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&GamepadProvider::ScheduleDoPoll, Unretained(this)));
}

void GamepadProvider::RegisterForUserGesture(const base::Closure& closure) {
  base::AutoLock lock(user_gesture_lock_);
  user_gesture_observers_.push_back(
      ClosureAndThread(closure, base::MessageLoop::current()->task_runner()));
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
  pad_states_.reset(new PadState[WebGamepads::itemsLengthCap]);

  for (unsigned i = 0; i < WebGamepads::itemsLengthCap; ++i)
    ClearPadState(pad_states_.get()[i]);

  polling_thread_.reset(new base::Thread("Gamepad polling thread"));
#if defined(OS_LINUX)
  // On Linux, the data fetcher needs to watch file descriptors, so the message
  // loop needs to be a libevent loop.
  const base::MessageLoop::Type kMessageLoopType = base::MessageLoop::TYPE_IO;
#elif defined(OS_ANDROID)
  // On Android, keeping a message loop of default type.
  const base::MessageLoop::Type kMessageLoopType =
      base::MessageLoop::TYPE_DEFAULT;
#else
  // On Mac, the data fetcher uses IOKit which depends on CFRunLoop, so the
  // message loop needs to be a UI-type loop. On Windows it must be a UI loop
  // to properly pump the MessageWindow that captures device state.
  const base::MessageLoop::Type kMessageLoopType = base::MessageLoop::TYPE_UI;
#endif
  polling_thread_->StartWithOptions(base::Thread::Options(kMessageLoopType, 0));

  if (fetcher) {
    AddGamepadDataFetcher(std::move(fetcher));
  } else {
    AddGamepadPlatformDataFetchers(this);
  }
}

void GamepadProvider::AddGamepadDataFetcher(
    scoped_ptr<GamepadDataFetcher> fetcher) {
  polling_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&GamepadProvider::DoAddGamepadDataFetcher,
                            base::Unretained(this), base::Passed(&fetcher)));
}

void GamepadProvider::DoAddGamepadDataFetcher(
    scoped_ptr<GamepadDataFetcher> fetcher) {
  DCHECK(base::MessageLoop::current() == polling_thread_->message_loop());

  fetcher->InitializeProvider(this);

  data_fetchers_.push_back(std::move(fetcher));
}

void GamepadProvider::SendPauseHint(bool paused) {
  DCHECK(base::MessageLoop::current() == polling_thread_->message_loop());
  for (const auto& it : data_fetchers_) {
    it->PauseHint(paused);
  }
}

void GamepadProvider::DoPoll() {
  DCHECK(base::MessageLoop::current() == polling_thread_->message_loop());
  DCHECK(have_scheduled_do_poll_);
  have_scheduled_do_poll_ = false;

  bool changed;
  GamepadHardwareBuffer* hwbuf = SharedMemoryAsHardwareBuffer();

  ANNOTATE_BENIGN_RACE_SIZED(
      &hwbuf->buffer,
      sizeof(WebGamepads),
      "Racey reads are discarded");

  {
    base::AutoLock lock(devices_changed_lock_);
    changed = devices_changed_;
    devices_changed_ = false;
  }

  // Loop through each registered data fetcher and poll its gamepad data.
  // It's expected that GetGamepadData will mark each gamepad as active (via
  // GetPadState). If a gamepad is not marked as active during the calls to
  // GetGamepadData then it's assumed to be disconnected.
  for (const auto& it : data_fetchers_) {
    it->GetGamepadData(changed);
  }

  // Send out disconnect events using the last polled data before we wipe it out
  // in the mapping step.
  if (ever_had_user_gesture_) {
    for (unsigned i = 0; i < WebGamepads::itemsLengthCap; ++i) {
      PadState& state = pad_states_.get()[i];

      if (!state.active_state && state.source != GAMEPAD_SOURCE_NONE) {
        OnGamepadConnectionChange(false, i, hwbuf->buffer.items[i]);
        ClearPadState(state);
      }
    }
  }

  {
    base::AutoLock lock(shared_memory_lock_);

    // Acquire the SeqLock. There is only ever one writer to this data.
    // See gamepad_hardware_buffer.h.
    hwbuf->sequence.WriteBegin();
    hwbuf->buffer.length = 0;
    for (unsigned i = 0; i < WebGamepads::itemsLengthCap; ++i) {
      PadState& state = pad_states_.get()[i];
      // Must run through the map+sanitize here or CheckForUserGesture may fail.
      MapAndSanitizeGamepadData(&state, &hwbuf->buffer.items[i]);
      if (state.active_state)
        hwbuf->buffer.length++;
    }
    hwbuf->sequence.WriteEnd();
  }

  if (ever_had_user_gesture_) {
    for (unsigned i = 0; i < WebGamepads::itemsLengthCap; ++i) {
      PadState& state = pad_states_.get()[i];

      if (state.active_state) {
        if (state.active_state == GAMEPAD_NEWLY_ACTIVE)
          OnGamepadConnectionChange(true, i, hwbuf->buffer.items[i]);
        state.active_state = GAMEPAD_INACTIVE;
      }
    }
  }

  CheckForUserGesture();

  // Schedule our next interval of polling.
  ScheduleDoPoll();
}

void GamepadProvider::ScheduleDoPoll() {
  DCHECK(base::MessageLoop::current() == polling_thread_->message_loop());
  if (have_scheduled_do_poll_)
    return;

  {
    base::AutoLock lock(is_paused_lock_);
    if (is_paused_)
      return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&GamepadProvider::DoPoll, Unretained(this)),
      base::TimeDelta::FromMilliseconds(kDesiredSamplingIntervalMs));
  have_scheduled_do_poll_ = true;
}

void GamepadProvider::OnGamepadConnectionChange(
    bool connected, int index, const WebGamepad& pad) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GamepadProvider::DispatchGamepadConnectionChange,
                 base::Unretained(this),
                 connected,
                 index,
                 pad));
}

void GamepadProvider::DispatchGamepadConnectionChange(
    bool connected, int index, const WebGamepad& pad) {
  if (connected)
    GamepadService::GetInstance()->OnGamepadConnected(index, pad);
  else
    GamepadService::GetInstance()->OnGamepadDisconnected(index, pad);
}

GamepadHardwareBuffer* GamepadProvider::SharedMemoryAsHardwareBuffer() {
  void* mem = gamepad_shared_memory_.memory();
  CHECK(mem);
  return static_cast<GamepadHardwareBuffer*>(mem);
}

void GamepadProvider::CheckForUserGesture() {
  base::AutoLock lock(user_gesture_lock_);
  if (user_gesture_observers_.empty() && ever_had_user_gesture_)
    return;

  const WebGamepads& pads = SharedMemoryAsHardwareBuffer()->buffer;
  if (GamepadsHaveUserGesture(pads)) {
    ever_had_user_gesture_ = true;
    for (size_t i = 0; i < user_gesture_observers_.size(); i++) {
      user_gesture_observers_[i].task_runner->PostTask(
          FROM_HERE, user_gesture_observers_[i].closure);
    }
    user_gesture_observers_.clear();
  }
}

void GamepadProvider::ClearPadState(PadState& state) {
  memset(&state, 0, sizeof(PadState));
}

PadState* GamepadProvider::GetPadState(GamepadSource source, int source_id) {
  // Check to see if the device already has a reserved slot
  PadState* empty_slot = nullptr;
  for (size_t i = 0; i < WebGamepads::itemsLengthCap; ++i) {
    PadState& state = pad_states_.get()[i];
    if (state.source == source &&
        state.source_id == source_id) {
      // Retrieving the pad state marks this gamepad as active.
      state.active_state = GAMEPAD_ACTIVE;
      return &state;
    }
    if (!empty_slot && state.source == GAMEPAD_SOURCE_NONE)
      empty_slot = &state;
  }
  if (empty_slot) {
    empty_slot->source = source;
    empty_slot->source_id = source_id;
    empty_slot->active_state = GAMEPAD_NEWLY_ACTIVE;
  }
  return empty_slot;
}

void GamepadProvider::MapAndSanitizeGamepadData(
    PadState* pad_state, WebGamepad* pad) {
  DCHECK(pad_state);
  DCHECK(pad);

  if (!pad_state->active_state) {
    memset(pad, 0, sizeof(WebGamepad));
    return;
  }

  // Copy the current state to the output buffer, using the mapping
  // function, if there is one available.
  if (pad_state->mapper)
    pad_state->mapper(pad_state->data, pad);
  else
    *pad = pad_state->data;

  pad->connected = true;

  if (!sanitize_)
    return;

  // About sanitization: Gamepads may report input event if the user is not
  // interacting with it, due to hardware problems or environmental ones (pad
  // has something heavy leaning against an axis.) This may cause user gestures
  // to be detected erroniously, exposing gamepad information when the user had
  // no intention of doing so. To avoid this we require that each button or axis
  // report being at rest (zero) at least once before exposing its value to the
  // Gamepad API. This state is tracked by the axis_mask and button_mask
  // bitfields. If the bit for an axis or button is 0 it means the axis has
  // never reported being at rest, and the value will be forced to zero.

  // We can skip axis sanitation if all available axes have been masked.
  uint32_t full_axis_mask = (1 << pad->axesLength) - 1;
  if (pad_state->axis_mask != full_axis_mask) {
    for (size_t axis = 0; axis < pad->axesLength; ++axis) {
      if (!(pad_state->axis_mask & 1 << axis)) {
        if (fabs(pad->axes[axis]) < kMinAxisResetValue) {
          pad_state->axis_mask |= 1 << axis;
        } else {
          pad->axes[axis] = 0.0f;
        }
      }
    }
  }

  // We can skip button sanitation if all available buttons have been masked.
  uint32_t full_button_mask = (1 << pad->buttonsLength) - 1;
  if (pad_state->button_mask != full_button_mask) {
    for (size_t button = 0; button < pad->buttonsLength; ++button) {
      if (!(pad_state->button_mask & 1 << button)) {
        if (!pad->buttons[button].pressed) {
          pad_state->button_mask |= 1 << button;
        } else {
          pad->buttons[button].pressed = false;
          pad->buttons[button].value = 0.0f;
        }
      }
    }
  }
}

}  // namespace content
