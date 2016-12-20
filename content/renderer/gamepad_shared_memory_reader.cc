// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gamepad_shared_memory_reader.h"

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "ipc/ipc_sync_message_filter.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/WebGamepadListener.h"
#include "third_party/WebKit/public/platform/WebPlatformEventListener.h"

namespace content {

GamepadSharedMemoryReader::GamepadSharedMemoryReader(RenderThread* thread)
    : RendererGamepadProvider(thread),
      gamepad_hardware_buffer_(NULL),
      ever_interacted_with_(false),
      binding_(this) {
  if (thread) {
    thread->GetRemoteInterfaces()->GetInterface(
        mojo::MakeRequest(&gamepad_monitor_));
    gamepad_monitor_->SetObserver(binding_.CreateInterfacePtrAndBind());
  }
}

void GamepadSharedMemoryReader::SendStartMessage() {
  if (gamepad_monitor_) {
    gamepad_monitor_->GamepadStartPolling(&renderer_shared_buffer_handle_);
  }
}

void GamepadSharedMemoryReader::SendStopMessage() {
  if (gamepad_monitor_) {
    gamepad_monitor_->GamepadStopPolling();
  }
}

void GamepadSharedMemoryReader::Start(
    blink::WebPlatformEventListener* listener) {
  PlatformEventObserver::Start(listener);

  // If we don't get a valid handle from the browser, don't try to Map (we're
  // probably out of memory or file handles).
  bool valid_handle = renderer_shared_buffer_handle_.is_valid();
  UMA_HISTOGRAM_BOOLEAN("Gamepad.ValidSharedMemoryHandle", valid_handle);
  if (!valid_handle)
    return;

  renderer_shared_buffer_mapping_ =
      renderer_shared_buffer_handle_->Map(sizeof(GamepadHardwareBuffer));
  CHECK(renderer_shared_buffer_mapping_);
  void* memory = renderer_shared_buffer_mapping_.get();
  CHECK(memory);
  gamepad_hardware_buffer_ =
      static_cast<GamepadHardwareBuffer*>(memory);
}

void GamepadSharedMemoryReader::SampleGamepads(blink::WebGamepads& gamepads) {
  // Blink should have started observing at that point.
  CHECK(is_observing());

  // ==========
  //   DANGER
  // ==========
  //
  // This logic is duplicated in Pepper as well. If you change it, that also
  // needs to be in sync. See ppapi/proxy/gamepad_resource.cc.
  blink::WebGamepads read_into;
  TRACE_EVENT0("GAMEPAD", "SampleGamepads");

  if (!renderer_shared_buffer_handle_.is_valid())
    return;

  // Only try to read this many times before failing to avoid waiting here
  // very long in case of contention with the writer. TODO(scottmg) Tune this
  // number (as low as 1?) if histogram shows distribution as mostly
  // 0-and-maximum.
  const int kMaximumContentionCount = 10;
  int contention_count = -1;
  base::subtle::Atomic32 version;
  do {
    version = gamepad_hardware_buffer_->seqlock.ReadBegin();
    memcpy(&read_into, &gamepad_hardware_buffer_->data, sizeof(read_into));
    ++contention_count;
    if (contention_count == kMaximumContentionCount)
      break;
  } while (gamepad_hardware_buffer_->seqlock.ReadRetry(version));
  UMA_HISTOGRAM_COUNTS("Gamepad.ReadContentionCount", contention_count);

  if (contention_count >= kMaximumContentionCount) {
    // We failed to successfully read, presumably because the hardware
    // thread was taking unusually long. Don't copy the data to the output
    // buffer, and simply leave what was there before.
    return;
  }

  // New data was read successfully, copy it into the output buffer.
  memcpy(&gamepads, &read_into, sizeof(gamepads));

  if (!ever_interacted_with_) {
    // Clear the connected flag if the user hasn't interacted with any of the
    // gamepads to prevent fingerprinting. The actual data is not cleared.
    // WebKit will only copy out data into the JS buffers for connected
    // gamepads so this is sufficient.
    for (unsigned i = 0; i < blink::WebGamepads::itemsLengthCap; i++)
      gamepads.items[i].connected = false;
  }
}

GamepadSharedMemoryReader::~GamepadSharedMemoryReader() {
  StopIfObserving();
}

void GamepadSharedMemoryReader::GamepadConnected(
    int index,
    const blink::WebGamepad& gamepad) {
  // The browser already checks if the user actually interacted with a device.
  ever_interacted_with_ = true;

  if (listener())
    listener()->didConnectGamepad(index, gamepad);
}

void GamepadSharedMemoryReader::GamepadDisconnected(
    int index,
    const blink::WebGamepad& gamepad) {
  if (listener())
    listener()->didDisconnectGamepad(index, gamepad);
}

} // namespace content
