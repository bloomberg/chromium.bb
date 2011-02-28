// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_MOCK_H_
#define GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_MOCK_H_

#include "../common/command_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class SharedMemory;
}

namespace gpu {

// An NPObject that implements a shared memory command buffer and a synchronous
// API to manage the put and get pointers.
class MockCommandBuffer : public CommandBuffer {
 public:
  MockCommandBuffer();
  virtual ~MockCommandBuffer();

  MOCK_METHOD1(Initialize, bool(int32 size));
  MOCK_METHOD2(Initialize, bool(base::SharedMemory* buffer, int32 size));
  MOCK_METHOD0(GetRingBuffer, Buffer());
  MOCK_METHOD0(GetState, State());
  MOCK_METHOD1(Flush, void(int32 put_offset));
  MOCK_METHOD1(FlushSync, State(int32 put_offset));
  MOCK_METHOD1(SetGetOffset, void(int32 get_offset));
  MOCK_METHOD1(CreateTransferBuffer, int32(size_t size));
  MOCK_METHOD1(DestroyTransferBuffer, void(int32 handle));
  MOCK_METHOD1(GetTransferBuffer, Buffer(int32 handle));
  MOCK_METHOD2(RegisterTransferBuffer, int32(base::SharedMemory* shared_memory,
                                             size_t size));
  MOCK_METHOD1(SetToken, void(int32 token));
  MOCK_METHOD1(SetParseError, void(error::Error error));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCommandBuffer);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_MOCK_H_
