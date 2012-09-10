// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GAMEPAD_UTIL_H_
#define CONTENT_RENDERER_GAMEPAD_UTIL_H_

#include "base/shared_memory.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebGamepads.h"

namespace content {

struct GamepadHardwareBuffer;

class GamepadSharedMemoryReader {
 public:
  GamepadSharedMemoryReader();
  virtual ~GamepadSharedMemoryReader();
  void SampleGamepads(WebKit::WebGamepads&);

 private:
  base::SharedMemoryHandle renderer_shared_memory_handle_;
  scoped_ptr<base::SharedMemory> renderer_shared_memory_;
  GamepadHardwareBuffer* gamepad_hardware_buffer_;

  bool ever_interacted_with_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GAMEPAD_UTIL_H_
