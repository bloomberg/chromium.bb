// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper classes for implementing gpu client side unit tests.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CLIENT_TEST_HELPER_H_
#define GPU_COMMAND_BUFFER_CLIENT_CLIENT_TEST_HELPER_H_

#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/compiler_specific.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class CommandBufferHelper;

class MockCommandBufferBase : public CommandBuffer {
 public:
  static const int32 kTransferBufferBaseId = 0x123;
  static const int32 kMaxTransferBuffers = 6;

  MockCommandBufferBase() { }
  virtual ~MockCommandBufferBase();

  virtual bool Initialize() OVERRIDE;
  virtual State GetState() OVERRIDE;
  virtual State GetLastState() OVERRIDE;
  virtual int32 GetLastToken() OVERRIDE;
  virtual State FlushSync(int32 put_offset, int32 last_known_get) OVERRIDE;
  virtual void SetGetBuffer(int transfer_buffer_id) OVERRIDE;
  virtual void SetGetOffset(int32 get_offset) OVERRIDE;
  virtual Buffer CreateTransferBuffer(size_t size, int32* id) OVERRIDE;
  virtual Buffer GetTransferBuffer(int32 id) OVERRIDE;
  virtual void SetToken(int32 token) OVERRIDE;
  virtual void SetParseError(error::Error error) OVERRIDE;
  virtual void SetContextLostReason(error::ContextLostReason reason) OVERRIDE;
  virtual uint32 InsertSyncPoint() OVERRIDE;

  // Get's the Id of the next transfer buffer that will be returned
  // by CreateTransferBuffer. This is useful for testing expected ids.
  int32 GetNextFreeTransferBufferId();

  void FlushHelper(int32 put_offset);
  void DestroyTransferBufferHelper(int32 id);

  virtual void OnFlush() = 0;

 private:
  scoped_ptr<int8[]> transfer_buffers_[kMaxTransferBuffers];
  Buffer transfer_buffer_buffers_[kMaxTransferBuffers];
  CommandBufferEntry* ring_buffer_;
  Buffer ring_buffer_buffer_;
  State state_;
};

class MockClientCommandBuffer : public MockCommandBufferBase {
 public:
  MockClientCommandBuffer();
  virtual ~MockClientCommandBuffer();

  // This is so we can use all the gmock functions when Flush is called.
  MOCK_METHOD0(OnFlush, void());
  MOCK_METHOD1(DestroyTransferBuffer, void(int32 id));

  virtual void Flush(int32 put_offset) OVERRIDE;

  void DelegateToFake();
};

class MockClientCommandBufferMockFlush : public MockClientCommandBuffer {
 public:
  MockClientCommandBufferMockFlush();
  virtual ~MockClientCommandBufferMockFlush();

  MOCK_METHOD1(Flush, void(int32 put_offset));

  void DelegateToFake();
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_CLIENT_TEST_HELPER_H_

