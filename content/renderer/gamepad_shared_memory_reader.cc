// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gamepad_shared_memory_reader.h"

#include "base/debug/trace_event.h"
#include "base/metrics/histogram.h"
#include "content/common/gamepad_messages.h"
#include "content/public/renderer/render_thread.h"
#include "content/common/gamepad_hardware_buffer.h"
#include "ipc/ipc_sync_message_filter.h"

namespace content {

GamepadSharedMemoryReader::GamepadSharedMemoryReader() {
  memset(ever_interacted_with_, 0, sizeof(ever_interacted_with_));
  CHECK(RenderThread::Get()->Send(new GamepadHostMsg_StartPolling(
      &renderer_shared_memory_handle_)));
  renderer_shared_memory_.reset(
      new base::SharedMemory(renderer_shared_memory_handle_, true));
  CHECK(renderer_shared_memory_->Map(sizeof(GamepadHardwareBuffer)));
  void *memory = renderer_shared_memory_->memory();
  CHECK(memory);
  gamepad_hardware_buffer_ =
      static_cast<GamepadHardwareBuffer*>(memory);
}

void GamepadSharedMemoryReader::SampleGamepads(WebKit::WebGamepads& gamepads) {
  WebKit::WebGamepads read_into;
  TRACE_EVENT0("GAMEPAD", "SampleGamepads");

  // Only try to read this many times before failing to avoid waiting here
  // very long in case of contention with the writer. TODO(scottmg) Tune this
  // number (as low as 1?) if histogram shows distribution as mostly
  // 0-and-maximum.
  const int kMaximumContentionCount = 10;
  int contention_count = -1;
  base::subtle::Atomic32 version;
  do {
    version = gamepad_hardware_buffer_->sequence.ReadBegin();
    memcpy(&read_into, &gamepad_hardware_buffer_->buffer, sizeof(read_into));
    ++contention_count;
    if (contention_count == kMaximumContentionCount)
      break;
  } while (gamepad_hardware_buffer_->sequence.ReadRetry(version));
  HISTOGRAM_COUNTS("Gamepad.ReadContentionCount", contention_count);

  if (contention_count >= kMaximumContentionCount) {
      // We failed to successfully read, presumably because the hardware
      // thread was taking unusually long. Don't copy the data to the output
      // buffer, and simply leave what was there before.
      return;
  }

  // New data was read successfully, copy it into the output buffer.
  memcpy(&gamepads, &read_into, sizeof(gamepads));

  // Override the "connected" with false until the user has interacted
  // with the gamepad. This is to prevent fingerprinting on drive-by pages.
  for (unsigned i = 0; i < WebKit::WebGamepads::itemsLengthCap; ++i) {
    WebKit::WebGamepad& pad = gamepads.items[i];
    // If the device is physically connected, then check if we should
    // keep it disabled. We track if any of the primary 4 buttons have been
    // pressed to determine a reasonable intentional interaction from the user.
    if (pad.connected) {
      if (ever_interacted_with_[i])
        continue;
      const unsigned kPrimaryInteractionButtons = 4;
      for (unsigned j = 0; j < kPrimaryInteractionButtons; ++j)
        ever_interacted_with_[i] |= pad.buttons[j] > 0.5f;
      // If we've not previously set, and the user still hasn't touched
      // these buttons, then don't pass the data on to the Chromium port.
      if (!ever_interacted_with_[i])
        pad.connected = false;
    }
  }
}

GamepadSharedMemoryReader::~GamepadSharedMemoryReader() {
  RenderThread::Get()->Send(new GamepadHostMsg_StopPolling());
}

} // namespace content
