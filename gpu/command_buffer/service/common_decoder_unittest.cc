// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/common_decoder.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

TEST(CommonDecoderBucket, Basic) {
  CommonDecoder::Bucket bucket;
  EXPECT_EQ(0, bucket.size());
  EXPECT_TRUE(NULL == bucket.GetData(0, 0));
}

TEST(CommonDecoderBucket, Size) {
  CommonDecoder::Bucket bucket;
  bucket.SetSize(24);
  EXPECT_EQ(24, bucket.size());
  bucket.SetSize(12);
  EXPECT_EQ(12, bucket.size());
}

TEST(CommonDecoderBucket, GetData) {
  CommonDecoder::Bucket bucket;

  bucket.SetSize(24);
  EXPECT_TRUE(NULL != bucket.GetData(0, 0));
  EXPECT_TRUE(NULL != bucket.GetData(24, 0));
  EXPECT_TRUE(NULL == bucket.GetData(25, 0));
  EXPECT_TRUE(NULL != bucket.GetData(0, 24));
  EXPECT_TRUE(NULL == bucket.GetData(0, 25));
  bucket.SetSize(23);
  EXPECT_TRUE(NULL == bucket.GetData(0, 24));
}

TEST(CommonDecoderBucket, SetData) {
  CommonDecoder::Bucket bucket;
  static const char data[] = "testing";

  bucket.SetSize(10);
  EXPECT_TRUE(bucket.SetData(data, 0, sizeof(data)));
  EXPECT_EQ(0, memcmp(data, bucket.GetData(0, sizeof(data)), sizeof(data)));
  EXPECT_TRUE(bucket.SetData(data, 2, sizeof(data)));
  EXPECT_EQ(0, memcmp(data, bucket.GetData(2, sizeof(data)), sizeof(data)));
  EXPECT_FALSE(bucket.SetData(data, 0, sizeof(data) * 2));
  EXPECT_FALSE(bucket.SetData(data, 5, sizeof(data)));
}

class TestCommonDecoder : public CommonDecoder {
 public:
  // Overridden from AsyncAPIInterface
  const char* GetCommandName(unsigned int command_id) const {
    return GetCommonCommandName(static_cast<cmd::CommandId>(command_id));
  }

  // Overridden from AsyncAPIInterface
  parse_error::ParseError DoCommand(
      unsigned int command,
      unsigned int arg_count,
      const void* cmd_data) {
    return DoCommonCommand(command, arg_count, cmd_data);
  }

  CommonDecoder::Bucket* GetBucket(uint32 id) const {
    return CommonDecoder::GetBucket(id);
  }
};

class MockCommandBufferEngine : public CommandBufferEngine {
 public:
  static const int32 kStartValidShmId = 1;
  static const int32 kValidShmId = 2;
  static const int32 kInvalidShmId = 3;
  static const size_t kBufferSize = 1024;
  static const uint32 kValidOffset = kBufferSize / 2;
  static const uint32 kInvalidOffset = kBufferSize;

  MockCommandBufferEngine()
      : CommandBufferEngine(),
        token_(),
        get_offset_(0) {
  }

  // Overridden from CommandBufferEngine.
  virtual Buffer GetSharedMemoryBuffer(int32 shm_id) {
    Buffer buffer;
    if (IsValidSharedMemoryId(shm_id)) {
      buffer.ptr = buffer_;
      buffer.size = kBufferSize;
    }
    return buffer;
  }

  template <typename T>
  T GetSharedMemoryAs(uint32 offset) {
    DCHECK(offset < kBufferSize);
    return reinterpret_cast<T>(&buffer_[offset]);
  }

  uint32 GetSharedMemoryOffset(const void* memory) {
    ptrdiff_t offset = reinterpret_cast<const int8*>(memory) - &buffer_[0];
    DCHECK_GE(offset, 0);
    DCHECK(offset < kBufferSize);
    return static_cast<uint32>(offset);
  }

  // Overridden from CommandBufferEngine.
  virtual void set_token(int32 token) {
    token_ = token;
  }

  int32 token() const {
    return token_;
  }

  // Overridden from CommandBufferEngine.
  virtual bool SetGetOffset(int32 offset) {
    if (offset < kBufferSize) {
      get_offset_ = offset;
      return true;
    }
    return false;
  }

  // Overridden from CommandBufferEngine.
  virtual int32 GetGetOffset() {
    return get_offset_;
  }

 private:
  bool IsValidSharedMemoryId(int32 shm_id) {
    return shm_id == kValidShmId || shm_id == kStartValidShmId;
  }

  int8 buffer_[kBufferSize];
  int32 token_;
  int32 get_offset_;
};

class CommonDecoderTest : public testing::Test {
 protected:
  virtual void SetUp() {
    decoder_.set_engine(&engine_);
  }

  virtual void TearDown() {
  }

  template <typename T>
  parse_error::ParseError ExecuteCmd(const T& cmd) {
    COMPILE_ASSERT(T::kArgFlags == cmd::kFixed, Cmd_kArgFlags_not_kFixed);
    return decoder_.DoCommand(cmd.kCmdId,
                              ComputeNumEntries(sizeof(cmd)) - 1,
                              &cmd);
  }

  template <typename T>
  parse_error::ParseError ExecuteImmediateCmd(const T& cmd, size_t data_size) {
    COMPILE_ASSERT(T::kArgFlags == cmd::kAtLeastN, Cmd_kArgFlags_not_kAtLeastN);
    return decoder_.DoCommand(cmd.kCmdId,
                              ComputeNumEntries(sizeof(cmd) + data_size) - 1,
                              &cmd);
  }

  MockCommandBufferEngine engine_;
  TestCommonDecoder decoder_;
};

TEST_F(CommonDecoderTest, Initialize) {
  EXPECT_EQ(0u, engine_.GetGetOffset());
}

TEST_F(CommonDecoderTest, HandleNoop) {
  cmd::Noop cmd;
  const uint32 kSkipCount = 5;
  cmd.Init(kSkipCount);
  EXPECT_EQ(parse_error::kParseNoError,
            ExecuteImmediateCmd(
                cmd, kSkipCount * kCommandBufferEntrySize));
}

TEST_F(CommonDecoderTest, SetToken) {
  cmd::SetToken cmd;
  const int32 kTokenId = 123;
  EXPECT_EQ(0, engine_.token());
  cmd.Init(kTokenId);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(kTokenId, engine_.token());
}

TEST_F(CommonDecoderTest, Jump) {
  cmd::Jump cmd;
  // Check valid args succeed.
  cmd.Init(MockCommandBufferEngine::kValidOffset);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(MockCommandBufferEngine::kValidOffset,
            engine_.GetGetOffset());
  // Check invalid offset fails.
  cmd.Init(MockCommandBufferEngine::kInvalidOffset);
  EXPECT_NE(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(MockCommandBufferEngine::kValidOffset,
            engine_.GetGetOffset());
}

// NOTE: The read_pointer checks for relative commands do not take into account
//     that the actual implementation of CommandBufferEngine uses the parse
//     which will advance the read pointer to the start of the next command.

TEST_F(CommonDecoderTest, JumpRelative) {
  cmd::JumpRelative cmd;
  // Check valid positive offset succeeds.
  const int32 kPositiveOffset = 16;
  cmd.Init(kPositiveOffset);
  uint32 read_pointer = engine_.GetGetOffset();
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  // See note above.
  EXPECT_EQ(read_pointer + kPositiveOffset, engine_.GetGetOffset());
  // Check valid negative offset succeeds.
  const int32 kNegativeOffset = -8;
  read_pointer = engine_.GetGetOffset();
  cmd.Init(kNegativeOffset);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  // See note above.
  EXPECT_EQ(read_pointer + kNegativeOffset, engine_.GetGetOffset());
  // Check invalid offset fails.
  cmd.Init(MockCommandBufferEngine::kInvalidOffset);
  EXPECT_NE(parse_error::kParseNoError, ExecuteCmd(cmd));
  // See note above.
  EXPECT_EQ(read_pointer + kNegativeOffset, engine_.GetGetOffset());
}

TEST_F(CommonDecoderTest, Call) {
  cmd::Call cmd;
  // Check valid args succeed.
  cmd.Init(MockCommandBufferEngine::kValidOffset);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(MockCommandBufferEngine::kValidOffset,
            engine_.GetGetOffset());
  // Check invalid offset fails.
  cmd.Init(MockCommandBufferEngine::kInvalidOffset);
  EXPECT_NE(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(MockCommandBufferEngine::kValidOffset,
            engine_.GetGetOffset());
  // Check that the call values are on the stack.
  cmd::Return return_cmd;
  return_cmd.Init();
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(return_cmd));
  EXPECT_EQ(0u, engine_.GetGetOffset());
  // Check that stack overflow fails.
  cmd.Init(MockCommandBufferEngine::kValidOffset);
  for (unsigned int ii = 0; ii < CommonDecoder::kMaxStackDepth; ++ii) {
    EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  }
  EXPECT_NE(parse_error::kParseNoError, ExecuteCmd(cmd));
}

TEST_F(CommonDecoderTest, CallRelative) {
  cmd::CallRelative cmd;
  // Check valid positive offset succeeds.
  const int32 kPositiveOffset = 16;
  cmd.Init(kPositiveOffset);
  uint32 read_pointer_1 = engine_.GetGetOffset();
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  // See note above.
  EXPECT_EQ(read_pointer_1 + kPositiveOffset, engine_.GetGetOffset());
  // Check valid negative offset succeeds.
  const int32 kNegativeOffset = -8;
  uint32 read_pointer_2 = engine_.GetGetOffset();
  cmd.Init(kNegativeOffset);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  // See note above.
  EXPECT_EQ(read_pointer_2 + kNegativeOffset, engine_.GetGetOffset());
  // Check invalid offset fails.
  cmd.Init(MockCommandBufferEngine::kInvalidOffset);
  EXPECT_NE(parse_error::kParseNoError, ExecuteCmd(cmd));
  // See note above.
  EXPECT_EQ(read_pointer_2 + kNegativeOffset, engine_.GetGetOffset());

  // Check that the call values are on the stack.
  cmd::Return return_cmd;
  return_cmd.Init();
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(return_cmd));
  // See note above.
  EXPECT_EQ(read_pointer_1 + kPositiveOffset, engine_.GetGetOffset());

  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(return_cmd));
  // See note above.
  EXPECT_EQ(0u, engine_.GetGetOffset());
  // Check that stack overflow fails.
  cmd.Init(kPositiveOffset);
  for (unsigned int ii = 0; ii < CommonDecoder::kMaxStackDepth; ++ii) {
    EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  }
  EXPECT_NE(parse_error::kParseNoError, ExecuteCmd(cmd));
}

TEST_F(CommonDecoderTest, Return) {
  // Success is tested by Call and CallRelative
  // Test that an empty stack fails.
  cmd::Return cmd;
  cmd.Init();
  EXPECT_NE(parse_error::kParseNoError, ExecuteCmd(cmd));
}

TEST_F(CommonDecoderTest, SetBucketSize) {
  cmd::SetBucketSize cmd;
  const uint32 kBucketId = 123;
  const uint32 kBucketLength1 = 1234;
  const uint32 kBucketLength2 = 78;
  // Check the bucket does not exist.
  EXPECT_TRUE(NULL == decoder_.GetBucket(kBucketId));
  // Check we can create one.
  cmd.Init(kBucketId, kBucketLength1);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  CommonDecoder::Bucket* bucket;
  bucket = decoder_.GetBucket(kBucketId);
  EXPECT_TRUE(NULL != bucket);
  EXPECT_EQ(kBucketLength1, bucket->size());
  // Check we can change it.
  cmd.Init(kBucketId, kBucketLength2);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  bucket = decoder_.GetBucket(kBucketId);
  EXPECT_TRUE(NULL != bucket);
  EXPECT_EQ(kBucketLength2, bucket->size());
  // Check we can delete it.
  cmd.Init(kBucketId, 0);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  bucket = decoder_.GetBucket(kBucketId);
  EXPECT_EQ(0, bucket->size());
}

TEST_F(CommonDecoderTest, SetBucketData) {
  cmd::SetBucketSize size_cmd;
  cmd::SetBucketData cmd;

  static const char kData[] = "1234567890123456789";

  const uint32 kBucketId = 123;
  const uint32 kInvalidBucketId = 124;

  size_cmd.Init(kBucketId, sizeof(kData));
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(size_cmd));
  CommonDecoder::Bucket* bucket = decoder_.GetBucket(kBucketId);
  // Check the data is not there.
  EXPECT_NE(0, memcmp(bucket->GetData(0, sizeof(kData)), kData, sizeof(kData)));

  // Check we can set it.
  const uint32 kSomeOffsetInSharedMemory = 50;
  void* memory = engine_.GetSharedMemoryAs<void*>(kSomeOffsetInSharedMemory);
  memcpy(memory, kData, sizeof(kData));
  cmd.Init(kBucketId, 0, sizeof(kData),
           MockCommandBufferEngine::kValidShmId, kSomeOffsetInSharedMemory);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, memcmp(bucket->GetData(0, sizeof(kData)), kData, sizeof(kData)));

  // Check we can set it partially.
  static const char kData2[] = "ABCEDFG";
  const uint32 kSomeOffsetInBucket = 5;
  memcpy(memory, kData2, sizeof(kData2));
  cmd.Init(kBucketId, kSomeOffsetInBucket, sizeof(kData2),
           MockCommandBufferEngine::kValidShmId, kSomeOffsetInSharedMemory);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, memcmp(bucket->GetData(kSomeOffsetInBucket, sizeof(kData2)),
                      kData2, sizeof(kData2)));
  const char* bucket_data = bucket->GetDataAs<const char*>(0, sizeof(kData));
  // Check that nothing was affected outside of updated area.
  EXPECT_EQ(kData[kSomeOffsetInBucket - 1],
            bucket_data[kSomeOffsetInBucket - 1]);
  EXPECT_EQ(kData[kSomeOffsetInBucket + sizeof(kData2)],
            bucket_data[kSomeOffsetInBucket + sizeof(kData2)]);

  // Check that it fails if the bucket_id is invalid
  cmd.Init(kInvalidBucketId, kSomeOffsetInBucket, sizeof(kData2),
           MockCommandBufferEngine::kValidShmId, kSomeOffsetInSharedMemory);
  EXPECT_NE(parse_error::kParseNoError, ExecuteCmd(cmd));

  // Check that it fails if the offset is out of range.
  cmd.Init(kBucketId, bucket->size(), 1,
           MockCommandBufferEngine::kValidShmId, kSomeOffsetInSharedMemory);
  EXPECT_NE(parse_error::kParseNoError, ExecuteCmd(cmd));

  // Check that it fails if the size is out of range.
  cmd.Init(kBucketId, 0, bucket->size() + 1,
           MockCommandBufferEngine::kValidShmId, kSomeOffsetInSharedMemory);
  EXPECT_NE(parse_error::kParseNoError, ExecuteCmd(cmd));
}

TEST_F(CommonDecoderTest, SetBucketDataImmediate) {
  cmd::SetBucketSize size_cmd;
  int8 buffer[1024];
  cmd::SetBucketDataImmediate& cmd =
      *reinterpret_cast<cmd::SetBucketDataImmediate*>(&buffer);

  static const char kData[] = "1234567890123456789";

  const uint32 kBucketId = 123;
  const uint32 kInvalidBucketId = 124;

  size_cmd.Init(kBucketId, sizeof(kData));
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(size_cmd));
  CommonDecoder::Bucket* bucket = decoder_.GetBucket(kBucketId);
  // Check the data is not there.
  EXPECT_NE(0, memcmp(bucket->GetData(0, sizeof(kData)), kData, sizeof(kData)));

  // Check we can set it.
  void* memory = &buffer[0] + sizeof(cmd);
  memcpy(memory, kData, sizeof(kData));
  cmd.Init(kBucketId, 0, sizeof(kData));
  EXPECT_EQ(parse_error::kParseNoError,
            ExecuteImmediateCmd(cmd, sizeof(kData)));
  EXPECT_EQ(0, memcmp(bucket->GetData(0, sizeof(kData)), kData, sizeof(kData)));

  // Check we can set it partially.
  static const char kData2[] = "ABCEDFG";
  const uint32 kSomeOffsetInBucket = 5;
  memcpy(memory, kData2, sizeof(kData2));
  cmd.Init(kBucketId, kSomeOffsetInBucket, sizeof(kData2));
  EXPECT_EQ(parse_error::kParseNoError,
            ExecuteImmediateCmd(cmd, sizeof(kData2)));
  EXPECT_EQ(0, memcmp(bucket->GetData(kSomeOffsetInBucket, sizeof(kData2)),
                      kData2, sizeof(kData2)));
  const char* bucket_data = bucket->GetDataAs<const char*>(0, sizeof(kData));
  // Check that nothing was affected outside of updated area.
  EXPECT_EQ(kData[kSomeOffsetInBucket - 1],
            bucket_data[kSomeOffsetInBucket - 1]);
  EXPECT_EQ(kData[kSomeOffsetInBucket + sizeof(kData2)],
            bucket_data[kSomeOffsetInBucket + sizeof(kData2)]);

  // Check that it fails if the bucket_id is invalid
  cmd.Init(kInvalidBucketId, kSomeOffsetInBucket, sizeof(kData2));
  EXPECT_NE(parse_error::kParseNoError,
            ExecuteImmediateCmd(cmd, sizeof(kData2)));

  // Check that it fails if the offset is out of range.
  cmd.Init(kBucketId, bucket->size(), 1);
  EXPECT_NE(parse_error::kParseNoError,
            ExecuteImmediateCmd(cmd, sizeof(kData2)));

  // Check that it fails if the size is out of range.
  cmd.Init(kBucketId, 0, bucket->size() + 1);
  EXPECT_NE(parse_error::kParseNoError,
            ExecuteImmediateCmd(cmd, sizeof(kData2)));
}

TEST_F(CommonDecoderTest, GetBucketSize) {
  cmd::SetBucketSize size_cmd;
  cmd::GetBucketSize cmd;

  const uint32 kBucketSize = 456;
  const uint32 kBucketId = 123;
  const uint32 kInvalidBucketId = 124;

  size_cmd.Init(kBucketId, kBucketSize);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(size_cmd));

  // Check that the size is correct.
  const uint32 kSomeOffsetInSharedMemory = 50;
  uint32* memory =
      engine_.GetSharedMemoryAs<uint32*>(kSomeOffsetInSharedMemory);
  *memory = 0xFFFFFFFF;
  cmd.Init(kBucketId,
           MockCommandBufferEngine::kValidShmId, kSomeOffsetInSharedMemory);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(kBucketSize, *memory);

  // Check that it fails if the bucket_id is invalid
  cmd.Init(kInvalidBucketId,
           MockCommandBufferEngine::kValidShmId, kSomeOffsetInSharedMemory);
  EXPECT_NE(parse_error::kParseNoError, ExecuteCmd(cmd));
}

TEST_F(CommonDecoderTest, GetBucketData) {
  cmd::SetBucketSize size_cmd;
  cmd::SetBucketData set_cmd;
  cmd::GetBucketData cmd;

  static const char kData[] = "1234567890123456789";
  static const char zero[sizeof(kData)] = { 0, };

  const uint32 kBucketId = 123;
  const uint32 kInvalidBucketId = 124;

  size_cmd.Init(kBucketId, sizeof(kData));
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(size_cmd));
  CommonDecoder::Bucket* bucket = decoder_.GetBucket(kBucketId);
  const uint32 kSomeOffsetInSharedMemory = 50;
  uint8* memory = engine_.GetSharedMemoryAs<uint8*>(kSomeOffsetInSharedMemory);
  memcpy(memory, kData, sizeof(kData));
  set_cmd.Init(kBucketId, 0, sizeof(kData),
               MockCommandBufferEngine::kValidShmId, kSomeOffsetInSharedMemory);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(set_cmd));

  // Check we can get the whole thing.
  memset(memory, 0, sizeof(kData));
  cmd.Init(kBucketId, 0, sizeof(kData),
           MockCommandBufferEngine::kValidShmId, kSomeOffsetInSharedMemory);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, memcmp(memory, kData, sizeof(kData)));

  // Check we can get a piece.
  const uint32 kSomeOffsetInBucket = 5;
  const uint32 kLengthOfPiece = 6;
  const uint8 kSentinel = 0xff;
  memset(memory, 0, sizeof(kData));
  memory[-1] = kSentinel;
  cmd.Init(kBucketId, kSomeOffsetInBucket, kLengthOfPiece,
           MockCommandBufferEngine::kValidShmId, kSomeOffsetInSharedMemory);
  EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, memcmp(memory, kData + kSomeOffsetInBucket, kLengthOfPiece));
  EXPECT_EQ(0, memcmp(memory + kLengthOfPiece, zero,
                      sizeof(kData) - kLengthOfPiece));
  EXPECT_EQ(kSentinel, memory[-1]);

  // Check that it fails if the bucket_id is invalid
  cmd.Init(kInvalidBucketId, kSomeOffsetInBucket, sizeof(kData),
           MockCommandBufferEngine::kValidShmId, kSomeOffsetInSharedMemory);
  EXPECT_NE(parse_error::kParseNoError, ExecuteCmd(cmd));

  // Check that it fails if the offset is invalid
  cmd.Init(kBucketId, sizeof(kData) + 1, 1,
           MockCommandBufferEngine::kValidShmId, kSomeOffsetInSharedMemory);
  EXPECT_NE(parse_error::kParseNoError, ExecuteCmd(cmd));

  // Check that it fails if the size is invalid
  cmd.Init(kBucketId, 0, sizeof(kData) + 1,
           MockCommandBufferEngine::kValidShmId, kSomeOffsetInSharedMemory);
  EXPECT_NE(parse_error::kParseNoError, ExecuteCmd(cmd));
}

}  // namespace gpu

