// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the Command Buffer Helper.

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/GLES2/gles2_command_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

#if !defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
#define GLES2_SUPPORT_CLIENT_SIDE_ARRAYS
#endif

namespace gpu {

class GLES2MockCommandBufferHelper : public CommandBuffer {
 public:
  static const int32 kTransferBufferId = 0x123;

  GLES2MockCommandBufferHelper() { }
  virtual ~GLES2MockCommandBufferHelper() { }

  // CommandBuffer implementation:
  virtual bool Initialize(int32 size) {
    ring_buffer_.reset(new CommandBufferEntry[size]);
    ring_buffer_buffer_.ptr = ring_buffer_.get();
    ring_buffer_buffer_.size = size;
    state_.num_entries = size / sizeof(ring_buffer_[0]);
    state_.token = 10000;  // All token checks in the tests should pass.
    return true;
  }

  virtual Buffer GetRingBuffer() {
    return ring_buffer_buffer_;
  }

  virtual State GetState() {
    return state_;
  }

  virtual void Flush(int32 put_offset) {
    state_.put_offset = put_offset;
  }

  virtual State FlushSync(int32 put_offset) {
    state_.put_offset = put_offset;
    state_.get_offset = put_offset;
    OnFlush(transfer_buffer_buffer_.ptr);

    return state_;
  }

  virtual void SetGetOffset(int32 get_offset) {
    state_.get_offset = get_offset;
  }

  virtual int32 CreateTransferBuffer(size_t size) {
    transfer_buffer_.reset(new int8[size]);
    transfer_buffer_buffer_.ptr = transfer_buffer_.get();
    transfer_buffer_buffer_.size = size;
    return kTransferBufferId;
  }

  virtual void DestroyTransferBuffer(int32) {  // NOLINT
    GPU_NOTREACHED();
  }

  virtual Buffer GetTransferBuffer(int32 id) {
    GPU_DCHECK_EQ(id, kTransferBufferId);
    return transfer_buffer_buffer_;
  }

  virtual int32 RegisterTransferBuffer(base::SharedMemory* shared_memory,
                                       size_t size) {
    GPU_NOTREACHED();
    return -1;
  }

  virtual void SetToken(int32 token) {
    GPU_NOTREACHED();
    state_.token = token;
  }

  virtual void SetParseError(error::Error error) {
    GPU_NOTREACHED();
    state_.error = error;
  }

  virtual void OnFlush(void* transfer_buffer) = 0;

 private:
  scoped_array<int8> transfer_buffer_;
  Buffer transfer_buffer_buffer_;
  scoped_array<CommandBufferEntry> ring_buffer_;
  Buffer ring_buffer_buffer_;
  State state_;
};

class MockGLES2CommandBuffer : public GLES2MockCommandBufferHelper {
 public:
  virtual ~MockGLES2CommandBuffer() {
  }

  // This is so we can use all the gmock functions when Flush is called.
  MOCK_METHOD1(OnFlush, void(void* result));
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef _MSC_VER
const int32 GLES2MockCommandBufferHelper::kTransferBufferId;
#endif

namespace gles2 {

using testing::Return;
using testing::Mock;
using testing::Truly;
using testing::Sequence;
using testing::DoAll;
using testing::Invoke;
using testing::_;

ACTION_P(SetMemory, obj) {
  memcpy(arg0, &obj, sizeof(obj));
}

ACTION_P2(SetMemoryAtOffset, offset, obj) {
  memcpy(static_cast<char*>(arg0) + offset, &obj, sizeof(obj));
}

// Used to help set the transfer buffer result to SizedResult of a single value.
template <typename T>
class SizedResultHelper {
 public:
  explicit SizedResultHelper(T result)
      : size_(sizeof(result)),
        result_(result) {
  }

 private:
  uint32 size_;
  T result_;
};

// Struct to make it easy to pass a vec4 worth of floats.
struct FourFloats {
  FourFloats(float _x, float _y, float _z, float _w)
      : x(_x),
        y(_y),
        z(_z),
        w(_w) {
  }

  float x;
  float y;
  float z;
  float w;
};

#pragma pack(push, 1)
// Struct that holds 7 characters.
struct Str7 {
  char str[7];
};
#pragma pack(pop)

// Test fixture for CommandBufferHelper test.
class GLES2ImplementationTest : public testing::Test {
 protected:
  static const int32 kNumCommandEntries = 100;
  static const int32 kCommandBufferSizeBytes =
      kNumCommandEntries * sizeof(CommandBufferEntry);
  static const size_t kTransferBufferSize = 256;
  static const int32 kTransferBufferId =
      GLES2MockCommandBufferHelper::kTransferBufferId;
  static const uint8 kInitialValue = 0xBD;
  static const GLint kMaxVertexAttribs = 8;

  virtual void SetUp() {
    command_buffer_.reset(new MockGLES2CommandBuffer());
    command_buffer_->Initialize(kCommandBufferSizeBytes);

    EXPECT_EQ(kTransferBufferId,
              command_buffer_->CreateTransferBuffer(kTransferBufferSize));
    transfer_buffer_ = command_buffer_->GetTransferBuffer(kTransferBufferId);
    ClearTransferBuffer();

    helper_.reset(new GLES2CmdHelper(command_buffer_.get()));
    helper_->Initialize(kCommandBufferSizeBytes);

    #if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
      EXPECT_CALL(*command_buffer_, OnFlush(_))
          .WillOnce(SetMemory(SizedResultHelper<GLint>(kMaxVertexAttribs)))
          .RetiresOnSaturation();
    #endif

    gl_.reset(new GLES2Implementation(
        helper_.get(),
        kTransferBufferSize,
        transfer_buffer_.ptr,
        kTransferBufferId,
        false));

    EXPECT_CALL(*command_buffer_, OnFlush(_)).Times(1).RetiresOnSaturation();
    helper_->CommandBufferHelper::FlushSync();
    Buffer ring_buffer = command_buffer_->GetRingBuffer();
    commands_ = static_cast<CommandBufferEntry*>(ring_buffer.ptr) +
                command_buffer_->GetState().put_offset;
  }

  virtual void TearDown() {
  }

  void ClearTransferBuffer() {
    memset(transfer_buffer_.ptr, kInitialValue, kTransferBufferSize);
  }

  static unsigned int RoundToAlignment(unsigned int size) {
    return (size + GLES2Implementation::kAlignment - 1) &
           ~(GLES2Implementation::kAlignment - 1);
  }

  Buffer transfer_buffer_;
  CommandBufferEntry* commands_;
  scoped_ptr<MockGLES2CommandBuffer> command_buffer_;
  scoped_ptr<GLES2CmdHelper> helper_;
  Sequence sequence_;
  scoped_ptr<GLES2Implementation> gl_;
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef _MSC_VER
const int32 GLES2ImplementationTest::kTransferBufferId;
#endif


TEST_F(GLES2ImplementationTest, ShaderSource) {
  const uint32 kBucketId = 1;  // This id is hardcoded into GLES2Implemenation
  const GLuint kShaderId = 456;
  const char* kString1 = "foobar";
  const char* kString2 = "barfoo";
  const size_t kString1Size = strlen(kString1);
  const size_t kString2Size = strlen(kString2);
  const size_t kString3Size = 1;  // Want the NULL;
  const size_t kSourceSize = kString1Size + kString2Size + kString3Size;
  const size_t kPaddedString1Size = RoundToAlignment(kString1Size);
  const size_t kPaddedString2Size = RoundToAlignment(kString2Size);
  struct Cmds {
    cmd::SetBucketSize set_bucket_size;
    cmd::SetBucketData set_bucket_data1;
    cmd::SetToken set_token1;
    cmd::SetBucketData set_bucket_data2;
    cmd::SetToken set_token2;
    cmd::SetBucketData set_bucket_data3;
    cmd::SetToken set_token3;
    ShaderSourceBucket shader_source_bucket;
    cmd::SetBucketSize clear_bucket_size;
  };
  int32 token = 1;
  uint32 offset = GLES2Implementation::kStartingOffset;
  Cmds expected;
  expected.set_bucket_size.Init(kBucketId, kSourceSize);
  expected.set_bucket_data1.Init(
      kBucketId, 0, kString1Size, kTransferBufferId, offset);
  expected.set_token1.Init(token++);
  expected.set_bucket_data2.Init(
      kBucketId, kString1Size, kString2Size, kTransferBufferId,
      offset + kPaddedString1Size);
  expected.set_token2.Init(token++);
  expected.set_bucket_data3.Init(
      kBucketId, kString1Size + kString2Size,
      kString3Size, kTransferBufferId,
      offset + kPaddedString1Size + kPaddedString2Size);
  expected.set_token3.Init(token++);
  expected.shader_source_bucket.Init(kShaderId, kBucketId);
  expected.clear_bucket_size.Init(kBucketId, 0);
  const char* strings[] = {
    kString1,
    kString2,
  };
  gl_->ShaderSource(kShaderId, 2, strings, NULL);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, GetShaderSource) {
  const uint32 kBucketId = 1;  // This id is hardcoded into GLES2Implemenation
  const GLuint kShaderId = 456;
  const Str7 kString = {"foobar"};
  const char kBad = 0x12;
  struct Cmds {
    cmd::SetBucketSize set_bucket_size1;
    GetShaderSource get_shader_source;
    cmd::GetBucketSize get_bucket_size;
    cmd::GetBucketData get_bucket_data;
    cmd::SetToken set_token1;
    cmd::SetBucketSize set_bucket_size2;
  };
  int32 token = 1;
  uint32 offset = GLES2Implementation::kStartingOffset;
  Cmds expected;
  expected.set_bucket_size1.Init(kBucketId, 0);
  expected.get_shader_source.Init(kShaderId, kBucketId);
  expected.get_bucket_size.Init(kBucketId, kTransferBufferId, 0);
  expected.get_bucket_data.Init(
      kBucketId, 0, sizeof(kString), kTransferBufferId, offset);
  expected.set_token1.Init(token++);
  expected.set_bucket_size2.Init(kBucketId, 0);
  char buf[sizeof(kString) + 1];
  memset(buf, kBad, sizeof(buf));

  EXPECT_CALL(*command_buffer_, OnFlush(_))
      .WillOnce(SetMemory(uint32(sizeof(kString))))
      .WillOnce(SetMemoryAtOffset(offset, kString))
      .RetiresOnSaturation();

  GLsizei length = 0;
  gl_->GetShaderSource(kShaderId, sizeof(buf), &length, buf);
  EXPECT_EQ(sizeof(kString) - 1, static_cast<size_t>(length));
  EXPECT_STREQ(kString.str, buf);
  EXPECT_EQ(buf[sizeof(kString)], kBad);
}

#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)

TEST_F(GLES2ImplementationTest, DrawArraysClientSideBuffers) {
  static const float verts[][4] = {
    { 12.0f, 23.0f, 34.0f, 45.0f, },
    { 56.0f, 67.0f, 78.0f, 89.0f, },
    { 13.0f, 24.0f, 35.0f, 46.0f, },
  };
  struct Cmds {
    EnableVertexAttribArray enable1;
    EnableVertexAttribArray enable2;
    BindBuffer bind_to_emu;
    BufferData set_size;
    BufferSubData copy_data1;
    cmd::SetToken set_token1;
    VertexAttribPointer set_pointer1;
    BufferSubData copy_data2;
    cmd::SetToken set_token2;
    VertexAttribPointer set_pointer2;
    DrawArrays draw;
    BindBuffer restore;
  };
  const GLuint kEmuBufferId = GLES2Implementation::kClientSideArrayId;
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kClientStride = sizeof(verts[0]);
  const GLint kFirst = 1;
  const GLsizei kCount = 2;
  const GLsizei kSize1 =
      arraysize(verts) * kNumComponents1 * sizeof(verts[0][0]);
  const GLsizei kSize2 =
      arraysize(verts) * kNumComponents2 * sizeof(verts[0][0]);
  const GLsizei kEmuOffset1 = 0;
  const GLsizei kEmuOffset2 = kSize1;

  const GLsizei kTotalSize = kSize1 + kSize2;
  int32 token = 1;
  uint32 offset = GLES2Implementation::kStartingOffset;
  Cmds expected;
  expected.enable1.Init(kAttribIndex1);
  expected.enable2.Init(kAttribIndex2);
  expected.bind_to_emu.Init(GL_ARRAY_BUFFER, kEmuBufferId);
  expected.set_size.Init(GL_ARRAY_BUFFER, kTotalSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data1.Init(
      GL_ARRAY_BUFFER, kEmuOffset1, kSize1, kTransferBufferId, offset);
  expected.set_token1.Init(token++);
  expected.set_pointer1.Init(kAttribIndex1, kNumComponents1,
                             GL_FLOAT, GL_FALSE, 0, kEmuOffset1);
  expected.copy_data2.Init(
      GL_ARRAY_BUFFER, kEmuOffset2, kSize2, kTransferBufferId, offset + kSize1);
  expected.set_token2.Init(token++);
  expected.set_pointer2.Init(kAttribIndex2, kNumComponents2,
                             GL_FLOAT, GL_FALSE, 0, kEmuOffset2);
  expected.draw.Init(GL_POINTS, kFirst, kCount);
  expected.restore.Init(GL_ARRAY_BUFFER, 0);
  gl_->EnableVertexAttribArray(kAttribIndex1);
  gl_->EnableVertexAttribArray(kAttribIndex2);
  gl_->VertexAttribPointer(kAttribIndex1, kNumComponents1,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribPointer(kAttribIndex2, kNumComponents2,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->DrawArrays(GL_POINTS, kFirst, kCount);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DrawElementsClientSideBuffers) {
  static const float verts[][4] = {
    { 12.0f, 23.0f, 34.0f, 45.0f, },
    { 56.0f, 67.0f, 78.0f, 89.0f, },
    { 13.0f, 24.0f, 35.0f, 46.0f, },
  };
  static const uint16 indices[] = {
    1, 2,
  };
  struct Cmds {
    EnableVertexAttribArray enable1;
    EnableVertexAttribArray enable2;
    BindBuffer bind_to_index_emu;
    BufferData set_index_size;
    BufferSubData copy_data0;
    cmd::SetToken set_token0;
    BindBuffer bind_to_emu;
    BufferData set_size;
    BufferSubData copy_data1;
    cmd::SetToken set_token1;
    VertexAttribPointer set_pointer1;
    BufferSubData copy_data2;
    cmd::SetToken set_token2;
    VertexAttribPointer set_pointer2;
    DrawElements draw;
    BindBuffer restore;
    BindBuffer restore_element;
  };
  const GLsizei kIndexSize = sizeof(indices);
  const GLuint kEmuBufferId = GLES2Implementation::kClientSideArrayId;
  const GLuint kEmuIndexBufferId =
      GLES2Implementation::kClientSideElementArrayId;
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kClientStride = sizeof(verts[0]);
  const GLsizei kCount = 2;
  const GLsizei kSize1 =
      arraysize(verts) * kNumComponents1 * sizeof(verts[0][0]);
  const GLsizei kSize2 =
      arraysize(verts) * kNumComponents2 * sizeof(verts[0][0]);
  const GLsizei kEmuOffset1 = 0;
  const GLsizei kEmuOffset2 = kSize1;

  const GLsizei kTotalSize = kSize1 + kSize2;
  int32 token = 1;
  uint32 offset = GLES2Implementation::kStartingOffset;
  Cmds expected;
  expected.enable1.Init(kAttribIndex1);
  expected.enable2.Init(kAttribIndex2);
  expected.bind_to_index_emu.Init(GL_ELEMENT_ARRAY_BUFFER, kEmuIndexBufferId);
  expected.set_index_size.Init(
      GL_ELEMENT_ARRAY_BUFFER, kIndexSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data0.Init(
      GL_ELEMENT_ARRAY_BUFFER, 0, kIndexSize, kTransferBufferId, offset);
  offset += kIndexSize;
  expected.set_token0.Init(token++);
  expected.bind_to_emu.Init(GL_ARRAY_BUFFER, kEmuBufferId);
  expected.set_size.Init(GL_ARRAY_BUFFER, kTotalSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data1.Init(
      GL_ARRAY_BUFFER, kEmuOffset1, kSize1, kTransferBufferId, offset);
  offset += kSize1;
  expected.set_token1.Init(token++);
  expected.set_pointer1.Init(kAttribIndex1, kNumComponents1,
                             GL_FLOAT, GL_FALSE, 0, kEmuOffset1);
  expected.copy_data2.Init(
      GL_ARRAY_BUFFER, kEmuOffset2, kSize2, kTransferBufferId, offset);
  expected.set_token2.Init(token++);
  expected.set_pointer2.Init(kAttribIndex2, kNumComponents2,
                             GL_FLOAT, GL_FALSE, 0, kEmuOffset2);
  expected.draw.Init(GL_POINTS, kCount, GL_UNSIGNED_SHORT, 0);
  expected.restore.Init(GL_ARRAY_BUFFER, 0);
  expected.restore_element.Init(GL_ELEMENT_ARRAY_BUFFER, 0);
  gl_->EnableVertexAttribArray(kAttribIndex1);
  gl_->EnableVertexAttribArray(kAttribIndex2);
  gl_->VertexAttribPointer(kAttribIndex1, kNumComponents1,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribPointer(kAttribIndex2, kNumComponents2,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->DrawElements(GL_POINTS, kCount, GL_UNSIGNED_SHORT, indices);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest,
       DrawElementsClientSideBuffersServiceSideIndices) {
  static const float verts[][4] = {
    { 12.0f, 23.0f, 34.0f, 45.0f, },
    { 56.0f, 67.0f, 78.0f, 89.0f, },
    { 13.0f, 24.0f, 35.0f, 46.0f, },
  };
  struct Cmds {
    EnableVertexAttribArray enable1;
    EnableVertexAttribArray enable2;
    BindBuffer bind_to_index;
    GetMaxValueInBufferCHROMIUM get_max;
    BindBuffer bind_to_emu;
    BufferData set_size;
    BufferSubData copy_data1;
    cmd::SetToken set_token1;
    VertexAttribPointer set_pointer1;
    BufferSubData copy_data2;
    cmd::SetToken set_token2;
    VertexAttribPointer set_pointer2;
    DrawElements draw;
    BindBuffer restore;
  };
  const GLuint kEmuBufferId = GLES2Implementation::kClientSideArrayId;
  const GLuint kClientIndexBufferId = 0x789;
  const GLuint kIndexOffset = 0x40;
  const GLuint kMaxIndex = 2;
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kClientStride = sizeof(verts[0]);
  const GLsizei kCount = 2;
  const GLsizei kSize1 =
      arraysize(verts) * kNumComponents1 * sizeof(verts[0][0]);
  const GLsizei kSize2 =
      arraysize(verts) * kNumComponents2 * sizeof(verts[0][0]);
  const GLsizei kEmuOffset1 = 0;
  const GLsizei kEmuOffset2 = kSize1;

  const GLsizei kTotalSize = kSize1 + kSize2;
  int32 token = 1;
  uint32 offset = GLES2Implementation::kStartingOffset;
  Cmds expected;
  expected.enable1.Init(kAttribIndex1);
  expected.enable2.Init(kAttribIndex2);
  expected.bind_to_index.Init(GL_ELEMENT_ARRAY_BUFFER, kClientIndexBufferId);
  expected.get_max.Init(kClientIndexBufferId, kCount, GL_UNSIGNED_SHORT,
                        kIndexOffset, kTransferBufferId, 0);
  expected.bind_to_emu.Init(GL_ARRAY_BUFFER, kEmuBufferId);
  expected.set_size.Init(GL_ARRAY_BUFFER, kTotalSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data1.Init(
      GL_ARRAY_BUFFER, kEmuOffset1, kSize1, kTransferBufferId, offset);
  offset += kSize1;
  expected.set_token1.Init(token++);
  expected.set_pointer1.Init(kAttribIndex1, kNumComponents1,
                             GL_FLOAT, GL_FALSE, 0, kEmuOffset1);
  expected.copy_data2.Init(
      GL_ARRAY_BUFFER, kEmuOffset2, kSize2, kTransferBufferId, offset);
  expected.set_token2.Init(token++);
  expected.set_pointer2.Init(kAttribIndex2, kNumComponents2,
                             GL_FLOAT, GL_FALSE, 0, kEmuOffset2);
  expected.draw.Init(GL_POINTS, kCount, GL_UNSIGNED_SHORT, kIndexOffset);
  expected.restore.Init(GL_ARRAY_BUFFER, 0);

  EXPECT_CALL(*command_buffer_, OnFlush(_))
      .WillOnce(SetMemory(kMaxIndex))
      .RetiresOnSaturation();

  gl_->EnableVertexAttribArray(kAttribIndex1);
  gl_->EnableVertexAttribArray(kAttribIndex2);
  gl_->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, kClientIndexBufferId);
  gl_->VertexAttribPointer(kAttribIndex1, kNumComponents1,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->VertexAttribPointer(kAttribIndex2, kNumComponents2,
                           GL_FLOAT, GL_FALSE, kClientStride, verts);
  gl_->DrawElements(GL_POINTS, kCount, GL_UNSIGNED_SHORT,
                    reinterpret_cast<const void*>(kIndexOffset));
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, GetVertexBufferPointerv) {
  static const float verts[1] = { 0.0f, };
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kStride1 = 12;
  const GLsizei kStride2 = 0;
  const GLuint kBufferId = 0x123;
  const GLint kOffset2 = 0x456;

  // Only one set and one get because the client side buffer's info is stored
  // on the client side.
  struct Cmds {
    BindBuffer bind;
    VertexAttribPointer set_pointer;
    GetVertexAttribPointerv get_pointer;
  };
  Cmds expected;
  expected.bind.Init(GL_ARRAY_BUFFER, kBufferId);
  expected.set_pointer.Init(kAttribIndex2, kNumComponents2, GL_FLOAT, GL_FALSE,
                            kStride2, kOffset2);
  expected.get_pointer.Init(kAttribIndex2, GL_VERTEX_ATTRIB_ARRAY_POINTER,
                            kTransferBufferId, 0);

  // One call to flush to way for GetVertexAttribPointerv
  EXPECT_CALL(*command_buffer_, OnFlush(_))
      .WillOnce(SetMemory(SizedResultHelper<uint32>(kOffset2)))
      .RetiresOnSaturation();

  // Set one client side buffer.
  gl_->VertexAttribPointer(kAttribIndex1, kNumComponents1,
                           GL_FLOAT, GL_FALSE, kStride1, verts);
  // Set one VBO
  gl_->BindBuffer(GL_ARRAY_BUFFER, kBufferId);
  gl_->VertexAttribPointer(kAttribIndex2, kNumComponents2,
                           GL_FLOAT, GL_FALSE, kStride2,
                           reinterpret_cast<const void*>(kOffset2));
  // now get them both.
  void* ptr1 = NULL;
  void* ptr2 = NULL;

  gl_->GetVertexAttribPointerv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr1);
  gl_->GetVertexAttribPointerv(
      kAttribIndex2, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr2);

  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(static_cast<const void*>(&verts) == ptr1);
  // because the service is not running ptr2 is not read.
  EXPECT_TRUE(ptr2 == reinterpret_cast<void*>(kOffset2));
}

TEST_F(GLES2ImplementationTest, GetVertexAttrib) {
  static const float verts[1] = { 0.0f, };
  const GLuint kAttribIndex1 = 1;
  const GLuint kAttribIndex2 = 3;
  const GLint kNumComponents1 = 3;
  const GLint kNumComponents2 = 2;
  const GLsizei kStride1 = 12;
  const GLsizei kStride2 = 0;
  const GLuint kBufferId = 0x123;
  const GLint kOffset2 = 0x456;

  // Only one set and one get because the client side buffer's info is stored
  // on the client side.
  struct Cmds {
    EnableVertexAttribArray enable;
    BindBuffer bind;
    VertexAttribPointer set_pointer;
    GetVertexAttribiv get1;  // for getting the buffer from attrib2
    GetVertexAttribfv get2;  // for getting the value from attrib1
  };
  Cmds expected;
  expected.enable.Init(kAttribIndex1);
  expected.bind.Init(GL_ARRAY_BUFFER, kBufferId);
  expected.set_pointer.Init(kAttribIndex2, kNumComponents2, GL_FLOAT, GL_FALSE,
                            kStride2, kOffset2);
  expected.get1.Init(kAttribIndex2,
                     GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,
                     kTransferBufferId, 0);
  expected.get2.Init(kAttribIndex1,
                     GL_CURRENT_VERTEX_ATTRIB,
                     kTransferBufferId, 0);

  FourFloats current_attrib(1.2f, 3.4f, 5.6f, 7.8f);

  // One call to flush to way for GetVertexAttribiv
  EXPECT_CALL(*command_buffer_, OnFlush(_))
      .WillOnce(SetMemory(SizedResultHelper<GLuint>(kBufferId)))
      .WillOnce(SetMemory(SizedResultHelper<FourFloats>(current_attrib)))
      .RetiresOnSaturation();

  gl_->EnableVertexAttribArray(kAttribIndex1);
  // Set one client side buffer.
  gl_->VertexAttribPointer(kAttribIndex1, kNumComponents1,
                           GL_FLOAT, GL_FALSE, kStride1, verts);
  // Set one VBO
  gl_->BindBuffer(GL_ARRAY_BUFFER, kBufferId);
  gl_->VertexAttribPointer(kAttribIndex2, kNumComponents2,
                           GL_FLOAT, GL_FALSE, kStride2,
                           reinterpret_cast<const void*>(kOffset2));
  // first get the service side once to see that we make a command
  GLint buffer_id = 0;
  GLint enabled = 0;
  GLint size = 0;
  GLint stride = 0;
  GLint type = 0;
  GLint normalized = 1;
  float current[4] = { 0.0f, };

  gl_->GetVertexAttribiv(
      kAttribIndex2, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buffer_id);
  EXPECT_EQ(kBufferId, static_cast<GLuint>(buffer_id));
  gl_->GetVertexAttribiv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buffer_id);
  gl_->GetVertexAttribiv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
  gl_->GetVertexAttribiv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
  gl_->GetVertexAttribiv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
  gl_->GetVertexAttribiv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_TYPE, &type);
  gl_->GetVertexAttribiv(
      kAttribIndex1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &normalized);
  gl_->GetVertexAttribfv(
      kAttribIndex1, GL_CURRENT_VERTEX_ATTRIB, &current[0]);

  EXPECT_EQ(0, buffer_id);
  EXPECT_EQ(GL_TRUE, enabled);
  EXPECT_EQ(kNumComponents1, size);
  EXPECT_EQ(kStride1, stride);
  EXPECT_EQ(GL_FLOAT, type);
  EXPECT_EQ(GL_FALSE, normalized);
  EXPECT_EQ(0, memcmp(&current_attrib, &current, sizeof(current_attrib)));

  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ReservedIds) {
  // Only the get error command should be issued.
  struct Cmds {
    GetError get;
  };
  Cmds expected;
  expected.get.Init(kTransferBufferId, 0);

  // One call to flush to wait for GetError
  EXPECT_CALL(*command_buffer_, OnFlush(_))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  gl_->BindBuffer(
      GL_ARRAY_BUFFER,
      GLES2Implementation::kClientSideArrayId);
  gl_->BindBuffer(
      GL_ARRAY_BUFFER,
      GLES2Implementation::kClientSideElementArrayId);
  GLenum err = gl_->GetError();
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), err);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

#endif  // defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)

TEST_F(GLES2ImplementationTest, ReadPixels2Reads) {
  struct Cmds {
    ReadPixels read1;
    cmd::SetToken set_token1;
    ReadPixels read2;
    cmd::SetToken set_token2;
  };
  const GLint kBytesPerPixel = 4;
  const GLint kWidth =
      (kTransferBufferSize - GLES2Implementation::kStartingOffset) /
      kBytesPerPixel;
  const GLint kHeight = 2;
  const GLenum kFormat = GL_RGBA;
  const GLenum kType = GL_UNSIGNED_BYTE;

  int32 token = 1;
  uint32 offset = GLES2Implementation::kStartingOffset;
  Cmds expected;
  expected.read1.Init(0, 0, kWidth, kHeight / 2, kFormat, kType,
                      kTransferBufferId, offset,
                      kTransferBufferId, 0);
  expected.set_token1.Init(token++);
  expected.read2.Init(0, kHeight / 2, kWidth, kHeight / 2, kFormat, kType,
                      kTransferBufferId, offset,
                      kTransferBufferId, 0);
  expected.set_token2.Init(token++);
  scoped_array<int8> buffer(new int8[kWidth * kHeight * kBytesPerPixel]);

  EXPECT_CALL(*command_buffer_, OnFlush(_))
      .WillOnce(SetMemory(static_cast<uint32>(1)))
      .WillOnce(SetMemory(static_cast<uint32>(1)))
      .RetiresOnSaturation();

  gl_->ReadPixels(0, 0, kWidth, kHeight, kFormat, kType, buffer.get());
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, ReadPixelsBadFormatType) {
  struct Cmds {
    ReadPixels read;
    cmd::SetToken set_token;
  };
  const GLint kBytesPerPixel = 4;
  const GLint kWidth = 2;
  const GLint kHeight = 2;
  const GLenum kFormat = 0;
  const GLenum kType = 0;

  int32 token = 1;
  uint32 offset = GLES2Implementation::kStartingOffset;
  Cmds expected;
  expected.read.Init(0, 0, kWidth, kHeight / 2, kFormat, kType,
                     kTransferBufferId, offset,
                     kTransferBufferId, 0);
  expected.set_token.Init(token++);
  scoped_array<int8> buffer(new int8[kWidth * kHeight * kBytesPerPixel]);

  EXPECT_CALL(*command_buffer_, OnFlush(_))
      .Times(1)
      .RetiresOnSaturation();

  gl_->ReadPixels(0, 0, kWidth, kHeight, kFormat, kType, buffer.get());
}

TEST_F(GLES2ImplementationTest, MapUnmapBufferSubDataCHROMIUM) {
  struct Cmds {
    BufferSubData buf;
    cmd::SetToken set_token;
  };
  const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
  const GLintptr kOffset = 15;
  const GLsizeiptr kSize = 16;

  int32 token = 1;
  uint32 offset = 0;
  Cmds expected;
  expected.buf.Init(
    kTarget, kOffset, kSize, kTransferBufferId, offset);
  expected.set_token.Init(token++);

  void* mem = gl_->MapBufferSubDataCHROMIUM(
      kTarget, kOffset, kSize, GL_WRITE_ONLY);
  ASSERT_TRUE(mem != NULL);
  gl_->UnmapBufferSubDataCHROMIUM(mem);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, MapUnmapBufferSubDataCHROMIUMBadArgs) {
  const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
  const GLintptr kOffset = 15;
  const GLsizeiptr kSize = 16;

  // Calls to flush to wait for GetError
  EXPECT_CALL(*command_buffer_, OnFlush(_))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  void* mem;
  mem = gl_->MapBufferSubDataCHROMIUM(kTarget, -1, kSize, GL_WRITE_ONLY);
  ASSERT_TRUE(mem == NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapBufferSubDataCHROMIUM(kTarget, kOffset, -1, GL_WRITE_ONLY);
  ASSERT_TRUE(mem == NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapBufferSubDataCHROMIUM(kTarget, kOffset, kSize, GL_READ_ONLY);
  ASSERT_TRUE(mem == NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), gl_->GetError());
  const char* kPtr = "something";
  gl_->UnmapBufferSubDataCHROMIUM(kPtr);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
}

TEST_F(GLES2ImplementationTest, MapUnmapTexSubImage2DCHROMIUM) {
  struct Cmds {
    TexSubImage2D tex;
    cmd::SetToken set_token;
  };
  const GLint kLevel = 1;
  const GLint kXOffset = 2;
  const GLint kYOffset = 3;
  const GLint kWidth = 4;
  const GLint kHeight = 5;
  const GLenum kFormat = GL_RGBA;
  const GLenum kType = GL_UNSIGNED_BYTE;

  int32 token = 1;
  uint32 offset = 0;
  Cmds expected;
  expected.tex.Init(
      GL_TEXTURE_2D, kLevel, kXOffset, kYOffset, kWidth, kHeight, kFormat,
      kType, kTransferBufferId, offset);
  expected.set_token.Init(token++);

  void* mem = gl_->MapTexSubImage2DCHROMIUM(
      GL_TEXTURE_2D,
      kLevel,
      kXOffset,
      kYOffset,
      kWidth,
      kHeight,
      kFormat,
      kType,
      GL_WRITE_ONLY);
  ASSERT_TRUE(mem != NULL);
  gl_->UnmapTexSubImage2DCHROMIUM(mem);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, MapUnmapTexSubImage2DCHROMIUMBadArgs) {
  const GLint kLevel = 1;
  const GLint kXOffset = 2;
  const GLint kYOffset = 3;
  const GLint kWidth = 4;
  const GLint kHeight = 5;
  const GLenum kFormat = GL_RGBA;
  const GLenum kType = GL_UNSIGNED_BYTE;

  // Calls to flush to wait for GetError
  EXPECT_CALL(*command_buffer_, OnFlush(_))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  void* mem;
  mem = gl_->MapTexSubImage2DCHROMIUM(
    GL_TEXTURE_2D,
    -1,
    kXOffset,
    kYOffset,
    kWidth,
    kHeight,
    kFormat,
    kType,
    GL_WRITE_ONLY);
  EXPECT_TRUE(mem == NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapTexSubImage2DCHROMIUM(
    GL_TEXTURE_2D,
    kLevel,
    -1,
    kYOffset,
    kWidth,
    kHeight,
    kFormat,
    kType,
    GL_WRITE_ONLY);
  EXPECT_TRUE(mem == NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapTexSubImage2DCHROMIUM(
    GL_TEXTURE_2D,
    kLevel,
    kXOffset,
    -1,
    kWidth,
    kHeight,
    kFormat,
    kType,
    GL_WRITE_ONLY);
  EXPECT_TRUE(mem == NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapTexSubImage2DCHROMIUM(
    GL_TEXTURE_2D,
    kLevel,
    kXOffset,
    kYOffset,
    -1,
    kHeight,
    kFormat,
    kType,
    GL_WRITE_ONLY);
  EXPECT_TRUE(mem == NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapTexSubImage2DCHROMIUM(
    GL_TEXTURE_2D,
    kLevel,
    kXOffset,
    kYOffset,
    kWidth,
    -1,
    kFormat,
    kType,
    GL_WRITE_ONLY);
  EXPECT_TRUE(mem == NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  mem = gl_->MapTexSubImage2DCHROMIUM(
    GL_TEXTURE_2D,
    kLevel,
    kXOffset,
    kYOffset,
    kWidth,
    kHeight,
    kFormat,
    kType,
    GL_READ_ONLY);
  EXPECT_TRUE(mem == NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), gl_->GetError());
  const char* kPtr = "something";
  gl_->UnmapTexSubImage2DCHROMIUM(kPtr);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
}

}  // namespace gles2
}  // namespace gpu


