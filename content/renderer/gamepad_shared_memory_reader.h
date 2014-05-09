// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GAMEPAD_SHARED_MEMORY_READER_H_
#define CONTENT_RENDERER_GAMEPAD_SHARED_MEMORY_READER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "content/common/gamepad_messages.h"
#include "content/public/renderer/render_process_observer.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace blink { class WebGamepadListener; }

namespace content {

struct GamepadHardwareBuffer;

class GamepadSharedMemoryReader : public RenderProcessObserver {
 public:
  GamepadSharedMemoryReader();
  virtual ~GamepadSharedMemoryReader();

  void SampleGamepads(blink::WebGamepads& gamepads);
  void SetGamepadListener(blink::WebGamepadListener* listener);

  // RenderProcessObserver implementation.
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  void OnGamepadConnected(int index, const blink::WebGamepad& gamepad);
  void OnGamepadDisconnected(int index, const blink::WebGamepad& gamepad);

  void StartPollingIfNecessary();
  void StopPollingIfNecessary();

  base::SharedMemoryHandle renderer_shared_memory_handle_;
  scoped_ptr<base::SharedMemory> renderer_shared_memory_;
  GamepadHardwareBuffer* gamepad_hardware_buffer_;
  blink::WebGamepadListener* gamepad_listener_;

  bool is_polling_;
  bool ever_interacted_with_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GAMEPAD_SHARED_MEMORY_READER_H_
