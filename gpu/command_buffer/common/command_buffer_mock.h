// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_MOCK_H_
#define GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_MOCK_H_

#include "gpu/command_buffer/common/command_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace command_buffer {

// An NPObject that implements a shared memory command buffer and a synchronous
// API to manage the put and get pointers.
class MockCommandBuffer : public CommandBuffer {
 public:
  MockCommandBuffer() {
    ON_CALL(*this, GetRingBuffer())
      .WillByDefault(testing::Return(static_cast<::base::SharedMemory*>(NULL)));
    ON_CALL(*this, GetTransferBuffer(testing::_))
      .WillByDefault(testing::Return(static_cast<::base::SharedMemory*>(NULL)));
  }

  MOCK_METHOD1(Initialize, bool(::base::SharedMemory* ring_buffer));
  MOCK_METHOD0(GetRingBuffer, ::base::SharedMemory*());
  MOCK_METHOD0(GetSize, int32());
  MOCK_METHOD1(SyncOffsets, int32(int32 put_offset));
  MOCK_METHOD0(GetGetOffset, int32());
  MOCK_METHOD1(SetGetOffset, void(int32 get_offset));
  MOCK_METHOD0(GetPutOffset, int32());
  MOCK_METHOD1(SetPutOffsetChangeCallback, void(Callback0::Type* callback));
  MOCK_METHOD1(CreateTransferBuffer, int32(size_t size));
  MOCK_METHOD1(DestroyTransferBuffer, void(int32 handle));
  MOCK_METHOD1(GetTransferBuffer, ::base::SharedMemory*(int32 handle));
  MOCK_METHOD0(GetToken, int32());
  MOCK_METHOD1(SetToken, void(int32 token));
  MOCK_METHOD0(ResetParseError, int32());
  MOCK_METHOD1(SetParseError, void(int32 parse_erro));
  MOCK_METHOD0(GetErrorStatus, bool());
  MOCK_METHOD0(RaiseErrorStatus, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCommandBuffer);
};

}  // namespace command_buffer

#endif  // GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_MOCK_H_
