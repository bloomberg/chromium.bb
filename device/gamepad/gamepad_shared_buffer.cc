// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_shared_buffer.h"

namespace device {

GamepadSharedBuffer::GamepadSharedBuffer() {
  size_t data_size = sizeof(GamepadHardwareBuffer);
  bool res = shared_memory_.CreateAndMapAnonymous(data_size);
  CHECK(res);

  void* mem = shared_memory_.memory();
  DCHECK(mem);
  hardware_buffer_ = new (mem) GamepadHardwareBuffer();
  memset(&(hardware_buffer_->data), 0, sizeof(blink::WebGamepads));
}

GamepadSharedBuffer::~GamepadSharedBuffer() {}

base::SharedMemory* GamepadSharedBuffer::shared_memory() {
  return &shared_memory_;
}

blink::WebGamepads* GamepadSharedBuffer::buffer() {
  return &(hardware_buffer()->data);
}

GamepadHardwareBuffer* GamepadSharedBuffer::hardware_buffer() {
  return hardware_buffer_;
}

void GamepadSharedBuffer::WriteBegin() {
  hardware_buffer_->seqlock.WriteBegin();
}

void GamepadSharedBuffer::WriteEnd() {
  hardware_buffer_->seqlock.WriteEnd();
}

}  // namespace device
