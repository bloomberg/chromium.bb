// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_MOCK_H_
#define GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_MOCK_H_

#include "gpu/command_buffer/common/command_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gpu {

// An NPObject that implements a shared memory command buffer and a synchronous
// API to manage the put and get pointers.
class MockCommandBuffer : public CommandBuffer {
 public:
  MockCommandBuffer() {
    ON_CALL(*this, GetRingBuffer())
      .WillByDefault(testing::Return(Buffer()));
    ON_CALL(*this, GetTransferBuffer(testing::_))
      .WillByDefault(testing::Return(Buffer()));
  }

  MOCK_METHOD1(Initialize, bool(int32 size));
  MOCK_METHOD0(GetRingBuffer, Buffer());
  MOCK_METHOD0(GetState, State());
  MOCK_METHOD1(Flush, State(int32 put_offset));
  MOCK_METHOD1(SetGetOffset, void(int32 get_offset));
  MOCK_METHOD1(CreateTransferBuffer, int32(size_t size));
  MOCK_METHOD1(DestroyTransferBuffer, void(int32 handle));
  MOCK_METHOD1(GetTransferBuffer, Buffer(int32 handle));
  MOCK_METHOD1(SetToken, void(int32 token));
  MOCK_METHOD1(SetParseError, void(error::Error error));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCommandBuffer);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_MOCK_H_
