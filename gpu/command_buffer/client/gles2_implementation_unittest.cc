// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the Command Buffer Helper.

#include "gpu/command_buffer/client/gles2_implementation.h"

#include <GLES2/gl2ext.h>
#include "gpu/command_buffer/common/command_buffer.h"
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

  virtual bool Initialize(base::SharedMemory* buffer, int32 size) {
    GPU_NOTREACHED();
    return false;
  }

  virtual Buffer GetRingBuffer() {
    return ring_buffer_buffer_;
  }

  virtual State GetState() {
    return state_;
  }

  virtual State GetLastState() {
    return state_;
  }

  virtual void Flush(int32 put_offset) {
    state_.put_offset = put_offset;
  }

  virtual State FlushSync(int32 put_offset, int32 last_known_get) {
    state_.put_offset = put_offset;
    state_.get_offset = put_offset;
    OnFlush(transfer_buffer_buffer_.ptr);
    return state_;
  }

  virtual void SetGetOffset(int32 get_offset) {
    state_.get_offset = get_offset;
  }

  virtual int32 CreateTransferBuffer(size_t size, int32 id_request) {
    transfer_buffer_.reset(new int8[size]);
    transfer_buffer_buffer_.ptr = transfer_buffer_.get();
    transfer_buffer_buffer_.size = size;
    return kTransferBufferId;
  }

  virtual void DestroyTransferBuffer(int32 /* id */) {
    GPU_NOTREACHED();
  }

  virtual Buffer GetTransferBuffer(int32 id) {
    GPU_DCHECK_EQ(id, kTransferBufferId);
    return transfer_buffer_buffer_;
  }

  virtual int32 RegisterTransferBuffer(base::SharedMemory* shared_memory,
                                       size_t size,
                                       int32 id_request) {
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

  virtual void SetContextLostReason(error::ContextLostReason reason) {
    GPU_NOTREACHED();
    state_.context_lost_reason = reason;
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
  MOCK_METHOD1(DestroyTransferBuffer, void(int32 id));
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef _MSC_VER
const int32 GLES2MockCommandBufferHelper::kTransferBufferId;
#endif

namespace gles2 {

using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::Invoke;
using testing::Mock;
using testing::Sequence;
using testing::Truly;
using testing::Return;

ACTION_P(SetMemory, obj) {
  memcpy(arg0, &obj, sizeof(obj));
}

ACTION_P2(SetMemoryAtOffset, offset, obj) {
  memcpy(static_cast<char*>(arg0) + offset, &obj, sizeof(obj));
}

ACTION_P3(SetMemoryAtOffsetFromArray, offset, array, size) {
  memcpy(static_cast<char*>(arg0) + offset, array, size);
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

class GLES2CommandBufferTestBase : public testing::Test {
 protected:
  static const int32 kNumCommandEntries = 400;
  static const int32 kCommandBufferSizeBytes =
      kNumCommandEntries * sizeof(CommandBufferEntry);
  static const size_t kTransferBufferSize = 256;
  static const int32 kTransferBufferId =
      GLES2MockCommandBufferHelper::kTransferBufferId;
  static const uint8 kInitialValue = 0xBD;

  GLES2CommandBufferTestBase()
      : commands_(NULL),
        token_(0),
        offset_(0),
        initial_offset_(0),
        alignment_(0) {
  }

  void SetupCommandBuffer(unsigned int offset, unsigned alignment) {
    initial_offset_ = offset;
    offset_ = offset;
    alignment_ = alignment;

    command_buffer_.reset(new MockGLES2CommandBuffer());
    command_buffer_->Initialize(kCommandBufferSizeBytes);

    EXPECT_EQ(kTransferBufferId,
              command_buffer_->CreateTransferBuffer(kTransferBufferSize, -1));
    transfer_buffer_ = command_buffer_->GetTransferBuffer(kTransferBufferId);
    ClearTransferBuffer();

    helper_.reset(new GLES2CmdHelper(command_buffer_.get()));
    helper_->Initialize(kCommandBufferSizeBytes);
  }

  const void* GetPut() {
    return helper_->GetSpace(0);
  }

  size_t MaxTransferBufferSize() {
    return kTransferBufferSize - initial_offset_;
  }

  void ClearCommands() {
    Buffer ring_buffer = command_buffer_->GetRingBuffer();
    memset(ring_buffer.ptr, kInitialValue, ring_buffer.size);
  }

  bool NoCommandsWritten() {
    return static_cast<const uint8*>(static_cast<const void*>(commands_))[0] ==
           kInitialValue;
  }

  void ClearTransferBuffer() {
    memset(transfer_buffer_.ptr, kInitialValue, kTransferBufferSize);
  }

  unsigned int RoundToAlignment(unsigned int size) {
    return (size + alignment_ - 1) & ~(alignment_ - 1);
  }

  int GetNextToken() {
    return ++token_;
  }

  uint32 AllocateTransferBuffer(size_t size) {
    if (offset_ + size > kTransferBufferSize) {
      offset_ = initial_offset_;
    }
    uint32 offset = offset_;
    offset_ += RoundToAlignment(size);
    return offset;
  }

  void* GetTransferAddressFromOffset(uint32 offset, size_t size) {
    EXPECT_LE(offset + size, transfer_buffer_.size);
    return static_cast<int8*>(transfer_buffer_.ptr) + offset;
  }

  template <typename T>
  T* GetTransferAddressFromOffsetAs(uint32 offset, size_t size) {
    return static_cast<T*>(GetTransferAddressFromOffset(offset, size));
  }

  Buffer transfer_buffer_;
  CommandBufferEntry* commands_;
  scoped_ptr<MockGLES2CommandBuffer> command_buffer_;
  scoped_ptr<GLES2CmdHelper> helper_;
  int token_;
  uint32 offset_;
  uint32 initial_offset_;
  uint32 alignment_;
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef _MSC_VER
const int32 GLES2CommandBufferTestBase::kNumCommandEntries;
const int32 GLES2CommandBufferTestBase::kCommandBufferSizeBytes;
const size_t GLES2CommandBufferTestBase::kTransferBufferSize;
const int32 GLES2CommandBufferTestBase::kTransferBufferId;
const uint8 GLES2CommandBufferTestBase::kInitialValue;
#endif

class TransferBufferTest : public GLES2CommandBufferTestBase {
 protected:
  static const unsigned int kStartingOffset = 64;
  static const unsigned int kAlignment = 4;

  TransferBufferTest() { }

  virtual void SetUp() {
    SetupCommandBuffer(
        GLES2Implementation::kStartingOffset,
        GLES2Implementation::kAlignment);

    transfer_buffer_.reset(new TransferBuffer(
        helper_.get(),
        kTransferBufferId,
        GetTransferAddressFromOffset(0, 0),
        kTransferBufferSize,
        kStartingOffset,
        kAlignment));
  }

  virtual void TearDown() {
    transfer_buffer_.reset();
  }

  scoped_ptr<TransferBuffer> transfer_buffer_;
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef _MSC_VER
const unsigned int TransferBufferTest::kStartingOffset;
const unsigned int TransferBufferTest::kAlignment;
#endif

TEST_F(TransferBufferTest, Basic) {
  EXPECT_TRUE(transfer_buffer_->HaveBuffer());
  EXPECT_EQ(kTransferBufferId, transfer_buffer_->GetShmId());
}

TEST_F(TransferBufferTest, Free) {
  EXPECT_TRUE(transfer_buffer_->HaveBuffer());

  // Free buffer.
  EXPECT_CALL(*command_buffer_, DestroyTransferBuffer(_))
      .Times(1)
      .RetiresOnSaturation();
  transfer_buffer_->Free();
  // See it's freed.
  EXPECT_FALSE(transfer_buffer_->HaveBuffer());
  // See that it gets reallocated.
  EXPECT_EQ(kTransferBufferId, transfer_buffer_->GetShmId());
  EXPECT_TRUE(transfer_buffer_->HaveBuffer());

  // Free buffer.
  EXPECT_CALL(*command_buffer_, DestroyTransferBuffer(_))
      .Times(1)
      .RetiresOnSaturation();
  transfer_buffer_->Free();
  // See it's freed.
  EXPECT_FALSE(transfer_buffer_->HaveBuffer());
  // See that it gets reallocated.
  EXPECT_TRUE(transfer_buffer_->GetResultBuffer() != NULL);
  EXPECT_TRUE(transfer_buffer_->HaveBuffer());

  // Free buffer.
  EXPECT_CALL(*command_buffer_, DestroyTransferBuffer(_))
      .Times(1)
      .RetiresOnSaturation();
  transfer_buffer_->Free();
  // See it's freed.
  EXPECT_FALSE(transfer_buffer_->HaveBuffer());
  // See that it gets reallocated.
  EXPECT_TRUE(transfer_buffer_->GetBuffer() != NULL);
  EXPECT_TRUE(transfer_buffer_->HaveBuffer());

  // Free buffer.
  EXPECT_CALL(*command_buffer_, DestroyTransferBuffer(_))
      .Times(1)
      .RetiresOnSaturation();
  transfer_buffer_->Free();
  // See it's freed.
  EXPECT_FALSE(transfer_buffer_->HaveBuffer());
  // See that it gets reallocated.
  transfer_buffer_->GetResultOffset();
  EXPECT_TRUE(transfer_buffer_->HaveBuffer());

  // Test freeing twice.
  EXPECT_CALL(*command_buffer_, DestroyTransferBuffer(_))
      .Times(1)
      .RetiresOnSaturation();
  transfer_buffer_->Free();
  transfer_buffer_->Free();
}

class GLES2ImplementationTest : public GLES2CommandBufferTestBase {
 protected:
  static const GLint kMaxCombinedTextureImageUnits = 8;
  static const GLint kMaxCubeMapTextureSize = 64;
  static const GLint kMaxFragmentUniformVectors = 16;
  static const GLint kMaxRenderbufferSize = 64;
  static const GLint kMaxTextureImageUnits = 8;
  static const GLint kMaxTextureSize = 128;
  static const GLint kMaxVaryingVectors = 8;
  static const GLint kMaxVertexAttribs = 8;
  static const GLint kMaxVertexTextureImageUnits = 0;
  static const GLint kMaxVertexUniformVectors = 128;
  static const GLint kNumCompressedTextureFormats = 0;
  static const GLint kNumShaderBinaryFormats = 0;
  static const GLuint kStartId = 1024;

  GLES2ImplementationTest() { }

  virtual void SetUp() {
    Initialize(false, true);
  }

  virtual void TearDown() {
  }

  void Initialize(bool shared_resources, bool bind_generates_resource) {
    SetupCommandBuffer(
        GLES2Implementation::kStartingOffset,
        GLES2Implementation::kAlignment);

    GLES2Implementation::GLState state;
    state.max_combined_texture_image_units = kMaxCombinedTextureImageUnits;
    state.max_cube_map_texture_size = kMaxCubeMapTextureSize;
    state.max_fragment_uniform_vectors = kMaxFragmentUniformVectors;
    state.max_renderbuffer_size = kMaxRenderbufferSize;
    state.max_texture_image_units = kMaxTextureImageUnits;
    state.max_texture_size = kMaxTextureSize;
    state.max_varying_vectors = kMaxVaryingVectors;
    state.max_vertex_attribs = kMaxVertexAttribs;
    state.max_vertex_texture_image_units = kMaxVertexTextureImageUnits;
    state.max_vertex_uniform_vectors = kMaxVertexUniformVectors;
    state.num_compressed_texture_formats = kNumCompressedTextureFormats;
    state.num_shader_binary_formats = kNumShaderBinaryFormats;

    // This just happens to work for now because GLState has 1 GLint per
    // state. If GLState gets more complicated this code will need to get
    // more complicated.
    AllocateTransferBuffer(sizeof(state));  // in
    uint32 offset = AllocateTransferBuffer(sizeof(state));  // out

    {
      InSequence sequence;

      EXPECT_CALL(*command_buffer_, OnFlush(_))
          .WillOnce(SetMemoryAtOffset(offset, state))
          .RetiresOnSaturation();
      GetNextToken();  // eat the token that starting up will use.

      // Must match StrictSharedIdHandler::kNumIdsToGet.
      GLuint num_ids = 2048;
      scoped_array<GLuint> all_ids(new GLuint[num_ids]);
      if (shared_resources) {
        if (!bind_generates_resource) {
          GLuint start = kStartId;
          GLuint max_num_per = MaxTransferBufferSize() / sizeof(GLuint);
          GLuint* ids = all_ids.get();
          for (GLuint ii = 0; ii < num_ids; ++ii) {
            ids[ii] = start + ii;
          }
          while (num_ids) {
            GLuint num = std::min(num_ids, max_num_per);
            size_t size = num * sizeof(ids[0]);
            uint32 offset = AllocateTransferBuffer(size);
            EXPECT_CALL(*command_buffer_, OnFlush(_))
                .WillOnce(SetMemoryAtOffsetFromArray(offset, ids, size))
                .RetiresOnSaturation();
            GetNextToken();
            start += num;
            ids += num;
            num_ids -= num;
          }
        }
      }

      gl_.reset(new GLES2Implementation(
          helper_.get(),
          kTransferBufferSize,
          transfer_buffer_.ptr,
          kTransferBufferId,
          shared_resources,
          bind_generates_resource));
    }

    EXPECT_CALL(*command_buffer_, OnFlush(_))
        .Times(1)
        .RetiresOnSaturation();
    helper_->CommandBufferHelper::Finish();
    Buffer ring_buffer = command_buffer_->GetRingBuffer();
    commands_ = static_cast<CommandBufferEntry*>(ring_buffer.ptr) +
                command_buffer_->GetState().put_offset;
    ClearCommands();
  }

  Sequence sequence_;
  scoped_ptr<GLES2Implementation> gl_;
};

class GLES2ImplementationStrictSharedTest : public GLES2ImplementationTest {
 protected:
  virtual void SetUp() {
    Initialize(true, false);
  }
};

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
  const size_t kPaddedString3Size = RoundToAlignment(kString3Size);
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
  Cmds expected;
  expected.set_bucket_size.Init(kBucketId, kSourceSize);
  expected.set_bucket_data1.Init(
      kBucketId, 0, kString1Size, kTransferBufferId,
      AllocateTransferBuffer(kPaddedString1Size));
  expected.set_token1.Init(GetNextToken());
  expected.set_bucket_data2.Init(
      kBucketId, kString1Size, kString2Size, kTransferBufferId,
      AllocateTransferBuffer(kPaddedString2Size));
  expected.set_token2.Init(GetNextToken());
  expected.set_bucket_data3.Init(
      kBucketId, kString1Size + kString2Size,
      kString3Size, kTransferBufferId,
      AllocateTransferBuffer(kPaddedString3Size));
  expected.set_token3.Init(GetNextToken());
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
  uint32 offset = AllocateTransferBuffer(sizeof(kString));
  Cmds expected;
  expected.set_bucket_size1.Init(kBucketId, 0);
  expected.get_shader_source.Init(kShaderId, kBucketId);
  expected.get_bucket_size.Init(kBucketId, kTransferBufferId, 0);
  expected.get_bucket_data.Init(
      kBucketId, 0, sizeof(kString), kTransferBufferId, offset);
  expected.set_token1.Init(GetNextToken());
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
  Cmds expected;
  expected.enable1.Init(kAttribIndex1);
  expected.enable2.Init(kAttribIndex2);
  expected.bind_to_emu.Init(GL_ARRAY_BUFFER, kEmuBufferId);
  expected.set_size.Init(GL_ARRAY_BUFFER, kTotalSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data1.Init(
      GL_ARRAY_BUFFER, kEmuOffset1, kSize1, kTransferBufferId,
      AllocateTransferBuffer(kSize1));
  expected.set_token1.Init(GetNextToken());
  expected.set_pointer1.Init(kAttribIndex1, kNumComponents1,
                             GL_FLOAT, GL_FALSE, 0, kEmuOffset1);
  expected.copy_data2.Init(
      GL_ARRAY_BUFFER, kEmuOffset2, kSize2, kTransferBufferId,
      AllocateTransferBuffer(kSize2));
  expected.set_token2.Init(GetNextToken());
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
  Cmds expected;
  expected.enable1.Init(kAttribIndex1);
  expected.enable2.Init(kAttribIndex2);
  expected.bind_to_index_emu.Init(GL_ELEMENT_ARRAY_BUFFER, kEmuIndexBufferId);
  expected.set_index_size.Init(
      GL_ELEMENT_ARRAY_BUFFER, kIndexSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data0.Init(
      GL_ELEMENT_ARRAY_BUFFER, 0, kIndexSize, kTransferBufferId,
      AllocateTransferBuffer(kIndexSize));
  expected.set_token0.Init(GetNextToken());
  expected.bind_to_emu.Init(GL_ARRAY_BUFFER, kEmuBufferId);
  expected.set_size.Init(GL_ARRAY_BUFFER, kTotalSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data1.Init(
      GL_ARRAY_BUFFER, kEmuOffset1, kSize1, kTransferBufferId,
      AllocateTransferBuffer(kSize1));
  expected.set_token1.Init(GetNextToken());
  expected.set_pointer1.Init(kAttribIndex1, kNumComponents1,
                             GL_FLOAT, GL_FALSE, 0, kEmuOffset1);
  expected.copy_data2.Init(
      GL_ARRAY_BUFFER, kEmuOffset2, kSize2, kTransferBufferId,
      AllocateTransferBuffer(kSize2));
  expected.set_token2.Init(GetNextToken());
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
  Cmds expected;
  expected.enable1.Init(kAttribIndex1);
  expected.enable2.Init(kAttribIndex2);
  expected.bind_to_index.Init(GL_ELEMENT_ARRAY_BUFFER, kClientIndexBufferId);
  expected.get_max.Init(kClientIndexBufferId, kCount, GL_UNSIGNED_SHORT,
                        kIndexOffset, kTransferBufferId, 0);
  expected.bind_to_emu.Init(GL_ARRAY_BUFFER, kEmuBufferId);
  expected.set_size.Init(GL_ARRAY_BUFFER, kTotalSize, 0, 0, GL_DYNAMIC_DRAW);
  expected.copy_data1.Init(
      GL_ARRAY_BUFFER, kEmuOffset1, kSize1, kTransferBufferId,
      AllocateTransferBuffer(kSize1));
  expected.set_token1.Init(GetNextToken());
  expected.set_pointer1.Init(kAttribIndex1, kNumComponents1,
                             GL_FLOAT, GL_FALSE, 0, kEmuOffset1);
  expected.copy_data2.Init(
      GL_ARRAY_BUFFER, kEmuOffset2, kSize2, kTransferBufferId,
      AllocateTransferBuffer(kSize2));
  expected.set_token2.Init(GetNextToken());
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

  Cmds expected;
  expected.read1.Init(
      0, 0, kWidth, kHeight / 2, kFormat, kType,
      kTransferBufferId,
      AllocateTransferBuffer(kWidth * kHeight / 2 * kBytesPerPixel),
      kTransferBufferId, 0);
  expected.set_token1.Init(GetNextToken());
  expected.read2.Init(
      0, kHeight / 2, kWidth, kHeight / 2, kFormat, kType,
      kTransferBufferId,
      AllocateTransferBuffer(kWidth * kHeight / 2 * kBytesPerPixel),
      kTransferBufferId, 0);
  expected.set_token2.Init(GetNextToken());
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

  Cmds expected;
  expected.read.Init(
      0, 0, kWidth, kHeight / 2, kFormat, kType,
      kTransferBufferId,
      AllocateTransferBuffer(kWidth * kHeight * kBytesPerPixel),
      kTransferBufferId, 0);
  expected.set_token.Init(GetNextToken());
  scoped_array<int8> buffer(new int8[kWidth * kHeight * kBytesPerPixel]);

  EXPECT_CALL(*command_buffer_, OnFlush(_))
      .Times(1)
      .RetiresOnSaturation();

  gl_->ReadPixels(0, 0, kWidth, kHeight, kFormat, kType, buffer.get());
}

TEST_F(GLES2ImplementationTest, FreeUnusedSharedMemory) {
  struct Cmds {
    BufferSubData buf;
    cmd::SetToken set_token;
  };
  const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
  const GLintptr kOffset = 15;
  const GLsizeiptr kSize = 16;

  uint32 offset = 0;
  Cmds expected;
  expected.buf.Init(
    kTarget, kOffset, kSize, kTransferBufferId, offset);
  expected.set_token.Init(GetNextToken());

  void* mem = gl_->MapBufferSubDataCHROMIUM(
      kTarget, kOffset, kSize, GL_WRITE_ONLY);
  ASSERT_TRUE(mem != NULL);
  gl_->UnmapBufferSubDataCHROMIUM(mem);
  EXPECT_CALL(*command_buffer_, DestroyTransferBuffer(_))
      .Times(1)
      .RetiresOnSaturation();
  gl_->FreeUnusedSharedMemory();
}

TEST_F(GLES2ImplementationTest, MapUnmapBufferSubDataCHROMIUM) {
  struct Cmds {
    BufferSubData buf;
    cmd::SetToken set_token;
  };
  const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
  const GLintptr kOffset = 15;
  const GLsizeiptr kSize = 16;

  uint32 offset = 0;
  Cmds expected;
  expected.buf.Init(
    kTarget, kOffset, kSize, kTransferBufferId, offset);
  expected.set_token.Init(GetNextToken());

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

  uint32 offset = 0;
  Cmds expected;
  expected.tex.Init(
      GL_TEXTURE_2D, kLevel, kXOffset, kYOffset, kWidth, kHeight, kFormat,
      kType, kTransferBufferId, offset, GL_FALSE);
  expected.set_token.Init(GetNextToken());

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

TEST_F(GLES2ImplementationTest, GetMultipleIntegervCHROMIUMValidArgs) {
  const GLenum pnames[] = {
    GL_DEPTH_WRITEMASK,
    GL_COLOR_WRITEMASK,
    GL_STENCIL_WRITEMASK,
  };
  const GLint num_results = 6;
  GLint results[num_results + 1];
  struct Cmds {
    GetMultipleIntegervCHROMIUM get_multiple;
    cmd::SetToken set_token;
  };
  const GLsizei kNumPnames = arraysize(pnames);
  const GLsizeiptr kResultsSize = num_results * sizeof(results[0]);
  const uint32 kPnamesOffset =
      AllocateTransferBuffer(kNumPnames * sizeof(pnames[0]));
  const uint32 kResultsOffset = AllocateTransferBuffer(kResultsSize);
  Cmds expected;
  expected.get_multiple.Init(
      kTransferBufferId, kPnamesOffset, kNumPnames,
      kTransferBufferId, kResultsOffset, kResultsSize);
  expected.set_token.Init(GetNextToken());

  const GLint kSentinel = 0x12345678;
  memset(results, 0, sizeof(results));
  results[num_results] = kSentinel;
  const GLint returned_results[] = {
    1, 0, 1, 0, 1, -1,
  };
  // One call to flush to wait for results
  EXPECT_CALL(*command_buffer_, OnFlush(_))
      .WillOnce(SetMemoryAtOffsetFromArray(
          kResultsOffset, returned_results, sizeof(returned_results)))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  gl_->GetMultipleIntegervCHROMIUM(
      &pnames[0], kNumPnames, &results[0], kResultsSize);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(0, memcmp(&returned_results, results, sizeof(returned_results)));
  EXPECT_EQ(kSentinel, results[num_results]);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetError());
}

TEST_F(GLES2ImplementationTest, GetMultipleIntegervCHROMIUMBadArgs) {
  GLenum pnames[] = {
    GL_DEPTH_WRITEMASK,
    GL_COLOR_WRITEMASK,
    GL_STENCIL_WRITEMASK,
  };
  const GLint num_results = 6;
  GLint results[num_results + 1];
  const GLsizei kNumPnames = arraysize(pnames);
  const GLsizeiptr kResultsSize = num_results * sizeof(results[0]);

  // Calls to flush to wait for GetError
  EXPECT_CALL(*command_buffer_, OnFlush(_))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  const GLint kSentinel = 0x12345678;
  memset(results, 0, sizeof(results));
  results[num_results] = kSentinel;
  // try bad size.
  gl_->GetMultipleIntegervCHROMIUM(
      &pnames[0], kNumPnames, &results[0], kResultsSize + 1);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  EXPECT_EQ(0, results[0]);
  EXPECT_EQ(kSentinel, results[num_results]);
  // try bad size.
  ClearCommands();
  gl_->GetMultipleIntegervCHROMIUM(
      &pnames[0], kNumPnames, &results[0], kResultsSize - 1);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  EXPECT_EQ(0, results[0]);
  EXPECT_EQ(kSentinel, results[num_results]);
  // try uncleared results.
  ClearCommands();
  results[2] = 1;
  gl_->GetMultipleIntegervCHROMIUM(
      &pnames[0], kNumPnames, &results[0], kResultsSize);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  EXPECT_EQ(0, results[0]);
  EXPECT_EQ(kSentinel, results[num_results]);
  // try bad enum results.
  ClearCommands();
  results[2] = 0;
  pnames[1] = GL_TRUE;
  gl_->GetMultipleIntegervCHROMIUM(
      &pnames[0], kNumPnames, &results[0], kResultsSize);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), gl_->GetError());
  EXPECT_EQ(0, results[0]);
  EXPECT_EQ(kSentinel, results[num_results]);
}

TEST_F(GLES2ImplementationTest, GetProgramInfoCHROMIUMGoodArgs) {
  const uint32 kBucketId = 1;  // This id is hardcoded into GLES2Implemenation
  const GLuint kProgramId = 123;
  const char kBad = 0x12;
  GLsizei size = 0;
  const Str7 kString = {"foobar"};
  char buf[20];

  memset(buf, kBad, sizeof(buf));
  uint32 offset = AllocateTransferBuffer(sizeof(kString));
  EXPECT_CALL(*command_buffer_, OnFlush(_))
      .WillOnce(SetMemory(uint32(sizeof(kString))))
      .WillOnce(SetMemoryAtOffset(offset, kString))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  struct Cmds {
    cmd::SetBucketSize set_bucket_size1;
    GetProgramInfoCHROMIUM get_program_info;
    cmd::GetBucketSize get_bucket_size;
    cmd::GetBucketData get_bucket_data;
    cmd::SetToken set_token1;
    cmd::SetBucketSize set_bucket_size2;
  };
  Cmds expected;
  expected.set_bucket_size1.Init(kBucketId, 0);
  expected.get_program_info.Init(kProgramId, kBucketId);
  expected.get_bucket_size.Init(kBucketId, kTransferBufferId, 0);
  expected.get_bucket_data.Init(
      kBucketId, 0, sizeof(kString), kTransferBufferId, offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_bucket_size2.Init(kBucketId, 0);
  gl_->GetProgramInfoCHROMIUM(kProgramId, sizeof(buf), &size, &buf);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetError());
  EXPECT_EQ(sizeof(kString), static_cast<size_t>(size));
  EXPECT_STREQ(kString.str, buf);
  EXPECT_EQ(buf[sizeof(kString)], kBad);
}

TEST_F(GLES2ImplementationTest, GetProgramInfoCHROMIUMBadArgs) {
  const uint32 kBucketId = 1;  // This id is hardcoded into GLES2Implemenation
  const GLuint kProgramId = 123;
  GLsizei size = 0;
  const Str7 kString = {"foobar"};
  char buf[20];

  uint32 offset = AllocateTransferBuffer(sizeof(kString));
  EXPECT_CALL(*command_buffer_, OnFlush(_))
      .WillOnce(SetMemory(uint32(sizeof(kString))))
      .WillOnce(SetMemoryAtOffset(offset, kString))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  // try bufsize not big enough.
  struct Cmds {
    cmd::SetBucketSize set_bucket_size1;
    GetProgramInfoCHROMIUM get_program_info;
    cmd::GetBucketSize get_bucket_size;
    cmd::GetBucketData get_bucket_data;
    cmd::SetToken set_token1;
    cmd::SetBucketSize set_bucket_size2;
  };
  Cmds expected;
  expected.set_bucket_size1.Init(kBucketId, 0);
  expected.get_program_info.Init(kProgramId, kBucketId);
  expected.get_bucket_size.Init(kBucketId, kTransferBufferId, 0);
  expected.get_bucket_data.Init(
      kBucketId, 0, sizeof(kString), kTransferBufferId, offset);
  expected.set_token1.Init(GetNextToken());
  expected.set_bucket_size2.Init(kBucketId, 0);
  gl_->GetProgramInfoCHROMIUM(kProgramId, 6, &size, &buf);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), gl_->GetError());
  ClearCommands();

  // try bad bufsize
  gl_->GetProgramInfoCHROMIUM(kProgramId, -1, &size, &buf);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  ClearCommands();
  // try no size ptr.
  gl_->GetProgramInfoCHROMIUM(kProgramId, sizeof(buf), NULL, &buf);
  EXPECT_TRUE(NoCommandsWritten());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
}

// Test that things are cached
TEST_F(GLES2ImplementationTest, GetIntegerCacheRead) {
  struct PNameValue {
    GLenum pname;
    GLint expected;
  };
  const PNameValue pairs[] = {
    { GL_ACTIVE_TEXTURE, GL_TEXTURE0, },
    { GL_TEXTURE_BINDING_2D, 0, },
    { GL_TEXTURE_BINDING_CUBE_MAP, 0, },
    { GL_FRAMEBUFFER_BINDING, 0, },
    { GL_RENDERBUFFER_BINDING, 0, },
    { GL_ARRAY_BUFFER_BINDING, 0, },
    { GL_ELEMENT_ARRAY_BUFFER_BINDING, 0, },
    { GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, kMaxCombinedTextureImageUnits, },
    { GL_MAX_CUBE_MAP_TEXTURE_SIZE, kMaxCubeMapTextureSize, },
    { GL_MAX_FRAGMENT_UNIFORM_VECTORS, kMaxFragmentUniformVectors, },
    { GL_MAX_RENDERBUFFER_SIZE, kMaxRenderbufferSize, },
    { GL_MAX_TEXTURE_IMAGE_UNITS, kMaxTextureImageUnits, },
    { GL_MAX_TEXTURE_SIZE, kMaxTextureSize, },
    { GL_MAX_VARYING_VECTORS, kMaxVaryingVectors, },
    { GL_MAX_VERTEX_ATTRIBS, kMaxVertexAttribs, },
    { GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, kMaxVertexTextureImageUnits, },
    { GL_MAX_VERTEX_UNIFORM_VECTORS, kMaxVertexUniformVectors, },
    { GL_NUM_COMPRESSED_TEXTURE_FORMATS, kNumCompressedTextureFormats, },
    { GL_NUM_SHADER_BINARY_FORMATS, kNumShaderBinaryFormats, },
  };
  size_t num_pairs = sizeof(pairs) / sizeof(pairs[0]);
  for (size_t ii = 0; ii < num_pairs; ++ii) {
    const PNameValue& pv = pairs[ii];
    GLint v = -1;
    gl_->GetIntegerv(pv.pname, &v);
    EXPECT_TRUE(NoCommandsWritten());
    EXPECT_EQ(pv.expected, v);
  }

  EXPECT_CALL(*command_buffer_, OnFlush(_))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetError());
}

TEST_F(GLES2ImplementationTest, GetIntegerCacheWrite) {
  struct PNameValue {
    GLenum pname;
    GLint expected;
  };
  gl_->ActiveTexture(GL_TEXTURE4);
  gl_->BindBuffer(GL_ARRAY_BUFFER, 2);
  gl_->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 3);
  gl_->BindFramebuffer(GL_FRAMEBUFFER, 4);
  gl_->BindRenderbuffer(GL_RENDERBUFFER, 5);
  gl_->BindTexture(GL_TEXTURE_2D, 6);
  gl_->BindTexture(GL_TEXTURE_CUBE_MAP, 7);

  const PNameValue pairs[] = {
    { GL_ACTIVE_TEXTURE, GL_TEXTURE4, },
    { GL_ARRAY_BUFFER_BINDING, 2, },
    { GL_ELEMENT_ARRAY_BUFFER_BINDING, 3, },
    { GL_FRAMEBUFFER_BINDING, 4, },
    { GL_RENDERBUFFER_BINDING, 5, },
    { GL_TEXTURE_BINDING_2D, 6, },
    { GL_TEXTURE_BINDING_CUBE_MAP, 7, },
  };
  size_t num_pairs = sizeof(pairs) / sizeof(pairs[0]);
  for (size_t ii = 0; ii < num_pairs; ++ii) {
    const PNameValue& pv = pairs[ii];
    GLint v = -1;
    gl_->GetIntegerv(pv.pname, &v);
    EXPECT_EQ(pv.expected, v);
  }

  EXPECT_CALL(*command_buffer_, OnFlush(_))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetError());
}

static bool ComputeImageDataSizes(
    int width, int height, int format, int type, int unpack_alignment,
    uint32* size, uint32* unpadded_row_size, uint32* padded_row_size) {
  uint32 temp_size;
  if (!GLES2Util::ComputeImageDataSize(
      width, 1, format, type, unpack_alignment, &temp_size)) {
    return false;
  }
  *unpadded_row_size = temp_size;
  if (!GLES2Util::ComputeImageDataSize(
      width, 2, format, type, unpack_alignment, &temp_size)) {
    return false;
  }
  *padded_row_size = temp_size - *unpadded_row_size;
  return GLES2Util::ComputeImageDataSize(
      width, height, format, type, unpack_alignment, size);
}

static bool CheckRect(
    int width, int height, GLenum format, GLenum type, int alignment,
    bool flip_y, const uint8* r1, const uint8* r2) {
  uint32 size = 0;
  uint32 unpadded_row_size = 0;
  uint32 padded_row_size = 0;
  if (!ComputeImageDataSizes(
      width, height, format, type, alignment, &size, &unpadded_row_size,
      &padded_row_size)) {
    return false;
  }

  int r2_stride = flip_y ?
      -static_cast<int>(padded_row_size) :
      static_cast<int>(padded_row_size);
  r2 = flip_y ? (r2 + (height - 1) * padded_row_size) : r2;

  for (int y = 0; y < height; ++y) {
    if (memcmp(r1, r2, unpadded_row_size) != 0) {
      return false;
    }
    r1 += padded_row_size;
    r2 += r2_stride;
  }
  return true;
}

ACTION_P8(CheckRectAction, width, height, format, type, alignment, flip_y,
          r1, r2) {
  EXPECT_TRUE(CheckRect(
      width, height, format, type, alignment, flip_y, r1, r2));
}

// Test TexImage2D with and without flip_y
TEST_F(GLES2ImplementationTest, TexImage2D) {
  struct Cmds {
    TexImage2D tex_image_2d;
    cmd::SetToken set_token;
  };
  struct Cmds2 {
    TexImage2D tex_image_2d;
    cmd::SetToken set_token;
  };
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLint kLevel = 0;
  const GLenum kFormat = GL_RGB;
  const GLsizei kWidth = 3;
  const GLsizei kHeight = 4;
  const GLint kBorder = 0;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLint kPixelStoreUnpackAlignment = 4;
  static uint8 pixels[] = {
    11, 12, 13, 13, 14, 15, 15, 16, 17, 101, 102, 103,
    21, 22, 23, 23, 24, 25, 25, 26, 27, 201, 202, 203,
    31, 32, 33, 33, 34, 35, 35, 36, 37, 123, 124, 125,
    41, 42, 43, 43, 44, 45, 45, 46, 47,
  };
  uint32 offset = AllocateTransferBuffer(sizeof(pixels));
  Cmds expected;
  expected.tex_image_2d.Init(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      kTransferBufferId, offset);
  expected.set_token.Init(GetNextToken());
  gl_->TexImage2D(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      pixels);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(CheckRect(
      kWidth, kHeight, kFormat, kType, kPixelStoreUnpackAlignment, false,
      pixels, GetTransferAddressFromOffsetAs<uint8>(offset, sizeof(pixels))));

  ClearCommands();
  uint32 offset2 = AllocateTransferBuffer(sizeof(pixels));
  Cmds2 expected2;
  expected2.tex_image_2d.Init(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      kTransferBufferId, offset2);
  expected2.set_token.Init(GetNextToken());
  const void* commands2 = GetPut();
  gl_->PixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, GL_TRUE);
  gl_->TexImage2D(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      pixels);
  EXPECT_EQ(0, memcmp(&expected2, commands2, sizeof(expected2)));
  EXPECT_TRUE(CheckRect(
      kWidth, kHeight, kFormat, kType, kPixelStoreUnpackAlignment, true,
      pixels, GetTransferAddressFromOffsetAs<uint8>(offset2, sizeof(pixels))));
}

// Test TexImage2D with 2 writes
TEST_F(GLES2ImplementationTest, TexImage2D2Writes) {
  struct Cmds {
    TexImage2D tex_image_2d;
    TexSubImage2D tex_sub_image_2d1;
    cmd::SetToken set_token1;
    TexSubImage2D tex_sub_image_2d2;
    cmd::SetToken set_token2;
  };
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLint kLevel = 0;
  const GLenum kFormat = GL_RGB;
  const GLint kBorder = 0;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLint kPixelStoreUnpackAlignment = 4;
  const GLsizei kWidth = 3;

  uint32 size = 0;
  uint32 unpadded_row_size = 0;
  uint32 padded_row_size = 0;
  ASSERT_TRUE(ComputeImageDataSizes(
      kWidth, 2, kFormat, kType, kPixelStoreUnpackAlignment,
      &size, &unpadded_row_size, &padded_row_size));
  const GLsizei kHeight = (MaxTransferBufferSize() / padded_row_size) * 2;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSize(
      kWidth, kHeight, kFormat, kType, kPixelStoreUnpackAlignment,
      &size));
  uint32 half_size = 0;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSize(
      kWidth, kHeight / 2, kFormat, kType, kPixelStoreUnpackAlignment,
      &half_size));

  scoped_array<uint8> pixels(new uint8[size]);
  for (uint32 ii = 0; ii < size; ++ii) {
    pixels[ii] = static_cast<uint8>(ii);
  }
  uint32 offset1 = AllocateTransferBuffer(half_size);
  uint32 offset2 = AllocateTransferBuffer(half_size);
  Cmds expected;
  expected.tex_image_2d.Init(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      0, 0);
  expected.tex_sub_image_2d1.Init(
      kTarget, kLevel, 0, 0, kWidth, kHeight / 2, kFormat, kType,
      kTransferBufferId, offset1, true);
  expected.set_token1.Init(GetNextToken());
  expected.tex_sub_image_2d2.Init(
      kTarget, kLevel, 0, kHeight / 2, kWidth, kHeight / 2, kFormat, kType,
      kTransferBufferId, offset2, true);
  expected.set_token2.Init(GetNextToken());

  // TODO(gman): Make it possible to run this test
  // EXPECT_CALL(*command_buffer_, OnFlush(_))
  //     .WillOnce(CheckRectAction(
  //         kWidth, kHeight / 2, kFormat, kType, kPixelStoreUnpackAlignment,
  //         false, pixels.get(),
  //         GetTransferAddressFromOffsetAs<uint8>(offset1, half_size)))
  //     .RetiresOnSaturation();

  gl_->TexImage2D(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      pixels.get());
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(CheckRect(
      kWidth, kHeight / 2, kFormat, kType, kPixelStoreUnpackAlignment, false,
      pixels.get() + kHeight / 2 * padded_row_size,
      GetTransferAddressFromOffsetAs<uint8>(offset2, half_size)));

  ClearCommands();
  const void* commands2 = GetPut();
  uint32 offset3 = AllocateTransferBuffer(half_size);
  uint32 offset4 = AllocateTransferBuffer(half_size);
  expected.tex_image_2d.Init(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      0, 0);
  expected.tex_sub_image_2d1.Init(
      kTarget, kLevel, 0, kHeight / 2, kWidth, kHeight / 2, kFormat, kType,
      kTransferBufferId, offset3, true);
  expected.set_token1.Init(GetNextToken());
  expected.tex_sub_image_2d2.Init(
      kTarget, kLevel, 0, 0, kWidth, kHeight / 2, kFormat, kType,
      kTransferBufferId, offset4, true);
  expected.set_token2.Init(GetNextToken());

  // TODO(gman): Make it possible to run this test
  // EXPECT_CALL(*command_buffer_, OnFlush(_))
  //     .WillOnce(CheckRectAction(
  //         kWidth, kHeight / 2, kFormat, kType, kPixelStoreUnpackAlignment,
  //         true, pixels.get(),
  //         GetTransferAddressFromOffsetAs<uint8>(offset3, half_size)))
  //     .RetiresOnSaturation();

  gl_->PixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, GL_TRUE);
  gl_->TexImage2D(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      pixels.get());
  EXPECT_EQ(0, memcmp(&expected, commands2, sizeof(expected)));
  EXPECT_TRUE(CheckRect(
      kWidth, kHeight / 2, kFormat, kType, kPixelStoreUnpackAlignment, true,
      pixels.get() + kHeight / 2 * padded_row_size,
      GetTransferAddressFromOffsetAs<uint8>(offset4, half_size)));
}

// Test TexImage2D with sub rows
TEST_F(GLES2ImplementationTest, TexImage2DSubRows) {
  struct Cmds {
    TexImage2D tex_image_2d;
    TexSubImage2D tex_sub_image_2d1;
    cmd::SetToken set_token1;
    TexSubImage2D tex_sub_image_2d2;
    cmd::SetToken set_token2;
    TexSubImage2D tex_sub_image_2d3;
    cmd::SetToken set_token3;
    TexSubImage2D tex_sub_image_2d4;
    cmd::SetToken set_token4;
  };
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLint kLevel = 0;
  const GLenum kFormat = GL_RGB;
  const GLint kBorder = 0;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLint kPixelStoreUnpackAlignment = 4;
  const GLsizei kHeight = 2;
  const GLsizei kWidth = (MaxTransferBufferSize() / 3) * 2;

  uint32 size = 0;
  uint32 unpadded_row_size = 0;
  uint32 padded_row_size = 0;
  ASSERT_TRUE(ComputeImageDataSizes(
      kWidth, kHeight, kFormat, kType, kPixelStoreUnpackAlignment,
      &size, &unpadded_row_size, &padded_row_size));
  uint32 part_size = kWidth * 3 / 2;

  scoped_array<uint8> pixels(new uint8[size]);
  for (uint32 ii = 0; ii < size; ++ii) {
    pixels[ii] = static_cast<uint8>(ii);
  }
  uint32 offset1 = AllocateTransferBuffer(part_size);
  uint32 offset2 = AllocateTransferBuffer(part_size);
  uint32 offset3 = AllocateTransferBuffer(part_size);
  uint32 offset4 = AllocateTransferBuffer(part_size);
  Cmds expected;
  expected.tex_image_2d.Init(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      0, 0);
  expected.tex_sub_image_2d1.Init(
      kTarget, kLevel, 0, 0, kWidth / 2, 1, kFormat, kType,
      kTransferBufferId, offset1, true);
  expected.set_token1.Init(GetNextToken());
  expected.tex_sub_image_2d2.Init(
      kTarget, kLevel, kWidth / 2, 0, kWidth / 2, 1, kFormat, kType,
      kTransferBufferId, offset2, true);
  expected.set_token2.Init(GetNextToken());
  expected.tex_sub_image_2d3.Init(
      kTarget, kLevel, 0, 1, kWidth / 2, 1, kFormat, kType,
      kTransferBufferId, offset3, true);
  expected.set_token3.Init(GetNextToken());
  expected.tex_sub_image_2d4.Init(
      kTarget, kLevel, kWidth / 2, 1, kWidth / 2, 1, kFormat, kType,
      kTransferBufferId, offset4, true);
  expected.set_token4.Init(GetNextToken());

  // TODO(gman): Make it possible to run this test
  // EXPECT_CALL(*command_buffer_, OnFlush(_))
  //     .WillOnce(CheckRectAction(
  //         kWidth / 2, 1, kFormat, kType, kPixelStoreUnpackAlignment, false,
  //         pixels.get(),
  //         GetTransferAddressFromOffsetAs<uint8>(offset1, part_size)))
  //     .WillOnce(CheckRectAction(
  //         kWidth / 2, 1, kFormat, kType, kPixelStoreUnpackAlignment, false,
  //         pixels.get() + part_size,
  //         GetTransferAddressFromOffsetAs<uint8>(offset2, part_size)))
  //     .WillOnce(CheckRectAction(
  //         kWidth / 2, 1, kFormat, kType, kPixelStoreUnpackAlignment, false,
  //         pixels.get() + padded_row_size,
  //         GetTransferAddressFromOffsetAs<uint8>(offset3, part_size)))
  //     .RetiresOnSaturation();

  gl_->TexImage2D(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      pixels.get());
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(CheckRect(
      kWidth / 2, 1, kFormat, kType, kPixelStoreUnpackAlignment, false,
      pixels.get() + padded_row_size + part_size,
      GetTransferAddressFromOffsetAs<uint8>(offset4, part_size)));

  ClearCommands();
  const void* commands2 = GetPut();
  offset1 = AllocateTransferBuffer(part_size);
  offset2 = AllocateTransferBuffer(part_size);
  offset3 = AllocateTransferBuffer(part_size);
  offset4 = AllocateTransferBuffer(part_size);
  expected.tex_image_2d.Init(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      0, 0);
  expected.tex_sub_image_2d1.Init(
      kTarget, kLevel, 0, 1, kWidth / 2, 1, kFormat, kType,
      kTransferBufferId, offset1, true);
  expected.set_token1.Init(GetNextToken());
  expected.tex_sub_image_2d2.Init(
      kTarget, kLevel, kWidth / 2, 1, kWidth / 2, 1, kFormat, kType,
      kTransferBufferId, offset2, true);
  expected.set_token2.Init(GetNextToken());
  expected.tex_sub_image_2d3.Init(
      kTarget, kLevel, 0, 0, kWidth / 2, 1, kFormat, kType,
      kTransferBufferId, offset3, true);
  expected.set_token3.Init(GetNextToken());
  expected.tex_sub_image_2d4.Init(
      kTarget, kLevel, kWidth / 2, 0, kWidth / 2, 1, kFormat, kType,
      kTransferBufferId, offset4, true);
  expected.set_token4.Init(GetNextToken());

  // TODO(gman): Make it possible to run this test
  // EXPECT_CALL(*command_buffer_, OnFlush(_))
  //     .WillOnce(CheckRectAction(
  //         kWidth / 2, 1, kFormat, kType, kPixelStoreUnpackAlignment, false,
  //         pixels.get(),
  //         GetTransferAddressFromOffsetAs<uint8>(offset1, part_size)))
  //     .WillOnce(CheckRectAction(
  //         kWidth / 2, 1, kFormat, kType, kPixelStoreUnpackAlignment, false,
  //         pixels.get() + part_size,
  //         GetTransferAddressFromOffsetAs<uint8>(offset2, part_size)))
  //     .WillOnce(CheckRectAction(
  //         kWidth / 2, 1, kFormat, kType, kPixelStoreUnpackAlignment, false,
  //         pixels.get() + padded_row_size,
  //         GetTransferAddressFromOffsetAs<uint8>(offset3, part_size)))
  //     .RetiresOnSaturation();

  gl_->PixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, GL_TRUE);
  gl_->TexImage2D(
      kTarget, kLevel, kFormat, kWidth, kHeight, kBorder, kFormat, kType,
      pixels.get());
  EXPECT_EQ(0, memcmp(&expected, commands2, sizeof(expected)));
  EXPECT_TRUE(CheckRect(
      kWidth / 2, 1, kFormat, kType, kPixelStoreUnpackAlignment, false,
      pixels.get() + padded_row_size + part_size,
      GetTransferAddressFromOffsetAs<uint8>(offset4, part_size)));
}

// Test TexSubImage2D with GL_PACK_FLIP_Y set and partial multirow transfers
TEST_F(GLES2ImplementationTest, TexSubImage2DFlipY) {
  const GLsizei kTextureWidth = MaxTransferBufferSize() / 4;
  const GLsizei kTextureHeight = 7;
  const GLsizei kSubImageWidth = MaxTransferBufferSize() / 8;
  const GLsizei kSubImageHeight = 4;
  const GLint kSubImageXOffset = 1;
  const GLint kSubImageYOffset = 2;
  const GLenum kFormat = GL_RGBA;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLint kLevel = 0;
  const GLint kBorder = 0;
  const GLint kPixelStoreUnpackAlignment = 4;

  struct Cmds {
    PixelStorei pixel_store_i1;
    TexImage2D tex_image_2d;
    TexSubImage2D tex_sub_image_2d1;
    cmd::SetToken set_token1;
    TexSubImage2D tex_sub_image_2d2;
    cmd::SetToken set_token2;
  };

  uint32 sub_2_high_size = 0;
  ASSERT_TRUE(GLES2Util::ComputeImageDataSize(
      kSubImageWidth, 2, kFormat, kType, kPixelStoreUnpackAlignment,
      &sub_2_high_size));
  uint32 offset1 = AllocateTransferBuffer(sub_2_high_size);
  uint32 offset2 = AllocateTransferBuffer(sub_2_high_size);

  Cmds expected;
  expected.pixel_store_i1.Init(GL_UNPACK_ALIGNMENT, kPixelStoreUnpackAlignment);
  expected.tex_image_2d.Init(
      kTarget, kLevel, kFormat, kTextureWidth, kTextureHeight, kBorder, kFormat,
      kType, 0, NULL);
  expected.tex_sub_image_2d1.Init(kTarget, kLevel, kSubImageXOffset,
      kSubImageYOffset + 2, kSubImageWidth, 2, kFormat, kType,
      kTransferBufferId, offset1, false);
  expected.set_token1.Init(GetNextToken());
  expected.tex_sub_image_2d2.Init(kTarget, kLevel, kSubImageXOffset,
      kSubImageYOffset, kSubImageWidth , 2, kFormat, kType, kTransferBufferId,
      offset2, false);
  expected.set_token2.Init(GetNextToken());

  gl_->PixelStorei(GL_UNPACK_ALIGNMENT, kPixelStoreUnpackAlignment);
  gl_->TexImage2D(
      kTarget, kLevel, kFormat, kTextureWidth, kTextureHeight, kBorder, kFormat,
      kType, NULL);
  // this call should not emit commands (handled client-side)
  gl_->PixelStorei(GL_UNPACK_FLIP_Y_CHROMIUM, GL_TRUE);
  scoped_array<uint32> pixels(new uint32[kSubImageWidth * kSubImageHeight]);
  for (int y = 0; y < kSubImageHeight; ++y) {
    for (int x = 0; x < kSubImageWidth; ++x) {
        pixels.get()[kSubImageWidth * y + x] = x | (y << 16);
    }
  }
  gl_->TexSubImage2D(
      GL_TEXTURE_2D, 0, kSubImageXOffset, kSubImageYOffset, kSubImageWidth,
      kSubImageHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());

  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_TRUE(CheckRect(
      kSubImageWidth, 2, kFormat, kType, kPixelStoreUnpackAlignment, true,
      reinterpret_cast<uint8*>(pixels.get() + 2 * kSubImageWidth),
      GetTransferAddressFromOffsetAs<uint8>(offset2, sub_2_high_size)));
}

// Test that GenBuffer does not call GenSharedIds.
// This is because with client side arrays on we know the StrictSharedIdHandler
// for buffers has already gotten a set of ids
TEST_F(GLES2ImplementationStrictSharedTest, GenBuffer) {
  // Starts at + 2 because client side arrays take first 2 ids.
  GLuint ids[3] = { kStartId + 2, kStartId + 3, kStartId + 4 };
  struct Cmds {
    GenBuffersImmediate gen;
    GLuint data[3];
  };
  Cmds expected;
  expected.gen.Init(arraysize(ids), &ids[0]);
  gl_->GenBuffers(arraysize(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_NE(0u, ids[0]);
  EXPECT_NE(0u, ids[1]);
  EXPECT_NE(0u, ids[2]);
}

// Binds can not be cached with bind_generates_resource = false because
// our id might not be valid.
TEST_F(GLES2ImplementationStrictSharedTest, BindsNotCached) {
  struct PNameValue {
    GLenum pname;
    GLint expected;
  };
  const PNameValue pairs[] = {
    { GL_TEXTURE_BINDING_2D, 1, },
    { GL_TEXTURE_BINDING_CUBE_MAP, 2, },
    { GL_FRAMEBUFFER_BINDING, 3, },
    { GL_RENDERBUFFER_BINDING, 4, },
    { GL_ARRAY_BUFFER_BINDING, 5, },
    { GL_ELEMENT_ARRAY_BUFFER_BINDING, 6, },
  };
  size_t num_pairs = sizeof(pairs) / sizeof(pairs[0]);
  for (size_t ii = 0; ii < num_pairs; ++ii) {
    const PNameValue& pv = pairs[ii];
    GLint v = -1;
    EXPECT_CALL(*command_buffer_, OnFlush(_))
        .WillOnce(SetMemory(SizedResultHelper<GLuint>(pv.expected)))
        .RetiresOnSaturation();
    gl_->GetIntegerv(pv.pname, &v);
    EXPECT_EQ(pv.expected, v);
  }
}

TEST_F(GLES2ImplementationStrictSharedTest, CanNotDeleteIdsWeDidNotCreate) {
  GLuint id = 0x12345678;

  EXPECT_CALL(*command_buffer_, OnFlush(_))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  gl_->DeleteBuffers(1, &id);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  gl_->DeleteFramebuffers(1, &id);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  gl_->DeleteRenderbuffers(1, &id);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  gl_->DeleteTextures(1, &id);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  gl_->DeleteProgram(id);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
  gl_->DeleteShader(id);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), gl_->GetError());
}

TEST_F(GLES2ImplementationTest, CreateStreamTextureCHROMIUM) {
  const GLuint kTextureId = 123;
  const GLuint kResult = 456;
  const uint32 kResultOffset = 0;

  struct Cmds {
    CreateStreamTextureCHROMIUM create_stream;
  };

  Cmds expected;
  expected.create_stream.Init(kTextureId, kTransferBufferId, kResultOffset);

  EXPECT_CALL(*command_buffer_, OnFlush(_))
      .WillOnce(SetMemoryAtOffset(kResultOffset, kResult))
      .WillOnce(SetMemory(GLuint(GL_NO_ERROR)))
      .RetiresOnSaturation();

  GLuint handle = gl_->CreateStreamTextureCHROMIUM(kTextureId);
  EXPECT_EQ(handle, kResult);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gl_->GetError());
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(GLES2ImplementationTest, DestroyStreamTextureCHROMIUM) {
  const GLuint kTextureHandle = 456;

  struct Cmds {
    DestroyStreamTextureCHROMIUM destroy_stream;
  };

  Cmds expected;
  expected.destroy_stream.Init(kTextureHandle);

  gl_->DestroyStreamTextureCHROMIUM(kTextureHandle);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

}  // namespace gles2
}  // namespace gpu


