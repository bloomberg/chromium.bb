// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/service/gl_mock.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::InSequence;
using ::testing::Pointee;

namespace gpu {
namespace gles2 {

class GLES2DecoderTest : public testing::Test {
 public:
  GLES2DecoderTest()
      : client_buffer_id_(100),
        client_framebuffer_id_(101),
        client_program_id_(102),
        client_renderbuffer_id_(103),
        client_shader_id_(104),
        client_texture_id_(105) {
    memset(immediate_buffer_, 0xEE, sizeof(immediate_buffer_));
  }

 protected:
  static const GLint kNumVertexAttribs = 16;

  static const GLuint kServiceBufferId = 301;
  static const GLuint kServiceFramebufferId = 302;
  static const GLuint kServiceRenderbufferId = 303;
  static const GLuint kServiceTextureId = 304;
  static const GLuint kServiceProgramId = 305;
  static const GLuint kServiceShaderId = 306;

  static const int32 kSharedMemoryId = 401;
  static const size_t kSharedBufferSize = 2048;
  static const uint32 kSharedMemoryOffset = 132;
  static const int32 kInvalidSharedMemoryId = 402;
  static const uint32 kInvalidSharedMemoryOffset = kSharedBufferSize + 1;

  static const uint32 kNewClientId = 501;
  static const uint32 kNewServiceId = 502;
  static const uint32 kInvalidClientId = 601;

  // Template to call glGenXXX functions.
  template <typename T>
  void GenHelper(GLuint client_id) {
    int8 buffer[sizeof(T) + sizeof(client_id)];
    T& cmd = *reinterpret_cast<T*>(&buffer);
    cmd.Init(1, &client_id);
    EXPECT_EQ(parse_error::kParseNoError,
              ExecuteImmediateCmd(cmd, sizeof(client_id)));
  }

  // This template exists solely so we can specialize it for
  // certain commands.
  template <typename T, int id>
  void SpecializedSetup() {
  }

  template <typename T>
  T* GetImmediateAs() {
    return reinterpret_cast<T*>(immediate_buffer_);
  }

  template <typename T, typename Command>
  T GetImmediateDataAs(Command* cmd) {
    reinterpret_cast<T>(ImmediateDataAddress(cmd));
  }

  virtual void SetUp() {
    gl_.reset(new ::gles2::MockGLInterface());
    ::gles2::GLInterface::SetGLInterface(gl_.get());

    EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_VERTEX_ATTRIBS, _))
        .WillOnce(SetArgumentPointee<1>(kNumVertexAttribs))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GetError())
        .WillRepeatedly(Return(GL_NO_ERROR));

    engine_.reset(new MockCommandBufferEngine());
    Buffer buffer = engine_->GetSharedMemoryBuffer(kSharedMemoryId);
    shared_memory_offset_ = kSharedMemoryOffset;
    shared_memory_address_ = reinterpret_cast<int8*>(buffer.ptr) +
        shared_memory_offset_;
    shared_memory_id_ = kSharedMemoryId;

    decoder_.reset(GLES2Decoder::Create());
    decoder_->Initialize();
    decoder_->set_engine(engine_.get());

    EXPECT_CALL(*gl_, GenBuffersARB(_, _))
        .WillOnce(SetArgumentPointee<1>(kServiceBufferId))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GenFramebuffersEXT(_, _))
        .WillOnce(SetArgumentPointee<1>(kServiceFramebufferId))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GenRenderbuffersEXT(_, _))
        .WillOnce(SetArgumentPointee<1>(kServiceRenderbufferId))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GenTextures(_, _))
        .WillOnce(SetArgumentPointee<1>(kServiceTextureId))
        .RetiresOnSaturation();

    GenHelper<GenBuffersImmediate>(client_buffer_id_);
    GenHelper<GenFramebuffersImmediate>(client_framebuffer_id_);
    GenHelper<GenRenderbuffersImmediate>(client_renderbuffer_id_);
    GenHelper<GenTexturesImmediate>(client_texture_id_);

    {
      EXPECT_CALL(*gl_, CreateProgram())
          .Times(1)
          .WillOnce(Return(kServiceProgramId))
          .RetiresOnSaturation();
      CreateProgram cmd;
      cmd.Init(client_program_id_);
      EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
    }

    {
      EXPECT_CALL(*gl_, CreateShader(_))
          .Times(1)
          .WillOnce(Return(kServiceShaderId))
          .RetiresOnSaturation();
      CreateShader cmd;
      cmd.Init(GL_VERTEX_SHADER, client_shader_id_);
      EXPECT_EQ(parse_error::kParseNoError, ExecuteCmd(cmd));
    }
  }

  virtual void TearDown() {
    decoder_->Destroy();
    decoder_.reset();
    engine_.reset();
    ::gles2::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  template <typename T>
  parse_error::ParseError ExecuteCmd(const T& cmd) {
    COMPILE_ASSERT(T::kArgFlags == cmd::kFixed, Cmd_kArgFlags_not_kFixed);
    return decoder_->DoCommand(cmd.kCmdId,
                               ComputeNumEntries(sizeof(cmd)) - 1,
                               &cmd);
  }

  template <typename T>
  parse_error::ParseError ExecuteImmediateCmd(const T& cmd, size_t data_size) {
    COMPILE_ASSERT(T::kArgFlags == cmd::kAtLeastN, Cmd_kArgFlags_not_kAtLeastN);
    return decoder_->DoCommand(cmd.kCmdId,
                               ComputeNumEntries(sizeof(cmd) + data_size) - 1,
                               &cmd);
  }

  template <typename T>
  T GetSharedMemoryAs() {
    return reinterpret_cast<T>(shared_memory_address_);
  }

  uint32 GetServiceId(uint32 client_id) {
    return decoder_->GetServiceIdForTesting(client_id);
  }

  scoped_ptr<::gles2::MockGLInterface> gl_;
  scoped_ptr<GLES2Decoder> decoder_;

  GLuint client_buffer_id_;
  GLuint client_framebuffer_id_;
  GLuint client_program_id_;
  GLuint client_renderbuffer_id_;
  GLuint client_shader_id_;
  GLuint client_texture_id_;

  uint32 shared_memory_id_;
  uint32 shared_memory_offset_;
  void* shared_memory_address_;

  int8 immediate_buffer_[256];

 private:
  class MockCommandBufferEngine : public CommandBufferEngine {
   public:
    MockCommandBufferEngine() {
      data_.reset(new int8[kSharedBufferSize]);
      valid_buffer_.ptr = data_.get();
      valid_buffer_.size = kSharedBufferSize;
    }

    virtual ~MockCommandBufferEngine() {
    }

    Buffer GetSharedMemoryBuffer(int32 shm_id) {
      return shm_id == kSharedMemoryId ? valid_buffer_ : invalid_buffer_;
    }

    void set_token(int32 token) {
      DCHECK(false);
    }

   private:
    scoped_array<int8> data_;
    Buffer valid_buffer_;
    Buffer invalid_buffer_;
  };

  scoped_ptr<MockCommandBufferEngine> engine_;
};

template <>
void GLES2DecoderTest::SpecializedSetup<LinkProgram, 0>() {
  InSequence dummy;
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_ACTIVE_ATTRIBUTES, _))
      .WillOnce(SetArgumentPointee<2>(0));
  EXPECT_CALL(
      *gl_,
      GetProgramiv(kServiceProgramId, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, _))
      .WillOnce(SetArgumentPointee<2>(0));
};

#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_autogen.h"

}  // namespace gles2
}  // namespace gpu


