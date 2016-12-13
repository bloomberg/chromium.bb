// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GAMEPAD_SHARED_MEMORY_READER_H_
#define CONTENT_RENDERER_GAMEPAD_SHARED_MEMORY_READER_H_

#include <memory>

#include "base/macros.h"
#include "content/public/renderer/renderer_gamepad_provider.h"
#include "device/base/synchronization/shared_memory_seqlock_buffer.h"
#include "device/gamepad/public/interfaces/gamepad.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/buffer.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace content {

typedef device::SharedMemorySeqLockBuffer<blink::WebGamepads>
    GamepadHardwareBuffer;

class GamepadSharedMemoryReader : public RendererGamepadProvider,
                                  public device::mojom::GamepadObserver {
 public:
  explicit GamepadSharedMemoryReader(RenderThread* thread);
  ~GamepadSharedMemoryReader() override;

  // RendererGamepadProvider implementation.
  void SampleGamepads(blink::WebGamepads& gamepads) override;
  void Start(blink::WebPlatformEventListener* listener) override;

 protected:
  // PlatformEventObserver protected methods.
  void SendStartMessage() override;
  void SendStopMessage() override;

 private:
  // device::mojom::GamepadObserver methods.
  void GamepadConnected(int index, const blink::WebGamepad& gamepad) override;
  void GamepadDisconnected(int index,
                           const blink::WebGamepad& gamepad) override;

  mojo::ScopedSharedBufferHandle renderer_shared_buffer_handle_;
  mojo::ScopedSharedBufferMapping renderer_shared_buffer_mapping_;
  GamepadHardwareBuffer* gamepad_hardware_buffer_;

  bool ever_interacted_with_;

  mojo::Binding<device::mojom::GamepadObserver> binding_;
  device::mojom::GamepadMonitorPtr gamepad_monitor_;

  DISALLOW_COPY_AND_ASSIGN(GamepadSharedMemoryReader);
};

}  // namespace content

#endif  // CONTENT_RENDERER_GAMEPAD_SHARED_MEMORY_READER_H_
