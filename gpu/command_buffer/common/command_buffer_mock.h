// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_MOCK_H_
#define GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_MOCK_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gpu {

// An NPObject that implements a shared memory command buffer and a synchronous
// API to manage the put and get pointers.
class MockCommandBuffer : public CommandBufferServiceBase {
 public:
  MockCommandBuffer();
  virtual ~MockCommandBuffer();

  MOCK_METHOD0(GetLastState, State());
  MOCK_METHOD0(GetLastToken, int32_t());
  MOCK_METHOD1(Flush, void(int32_t put_offset));
  MOCK_METHOD1(OrderingBarrier, void(int32_t put_offset));
  MOCK_METHOD2(WaitForTokenInRange, State(int32_t start, int32_t end));
  MOCK_METHOD3(WaitForGetOffsetInRange,
               State(uint32_t set_get_buffer_count,
                     int32_t start,
                     int32_t end));
  MOCK_METHOD1(SetGetBuffer, void(int32_t transfer_buffer_id));
  MOCK_METHOD1(SetGetOffset, void(int32_t get_offset));
  MOCK_METHOD1(SetReleaseCount, void(uint64_t release_count));
  MOCK_METHOD2(CreateTransferBuffer,
               scoped_refptr<gpu::Buffer>(size_t size, int32_t* id));
  MOCK_METHOD1(DestroyTransferBuffer, void(int32_t id));
  MOCK_METHOD1(GetTransferBuffer, scoped_refptr<gpu::Buffer>(int32_t id));
  MOCK_METHOD1(SetToken, void(int32_t token));
  MOCK_METHOD1(SetParseError, void(error::Error error));
  MOCK_METHOD1(SetContextLostReason,
               void(error::ContextLostReason context_lost_reason));
  MOCK_METHOD0(InsertSyncPoint, uint32_t());
  MOCK_METHOD0(GetPutOffset, int32_t());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCommandBuffer);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_MOCK_H_
