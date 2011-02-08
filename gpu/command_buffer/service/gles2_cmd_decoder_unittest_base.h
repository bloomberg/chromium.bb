// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_BASE_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_BASE_H_

#include "app/gfx/gl/gl_context_stub.h"
#include "gpu/command_buffer/common/gl_mock.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/GLES2/gles2_command_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace gles2 {

class GLES2DecoderTestBase : public testing::Test {
 public:
  GLES2DecoderTestBase();
  virtual ~GLES2DecoderTestBase();

 protected:
  static const GLint kMaxTextureSize = 2048;
  static const GLint kMaxCubeMapTextureSize = 256;
  static const GLint kNumVertexAttribs = 16;
  static const GLint kNumTextureUnits = 8;
  static const GLint kMaxTextureImageUnits = 8;
  static const GLint kMaxVertexTextureImageUnits = 2;
  static const GLint kMaxFragmentUniformVectors = 16;
  static const GLint kMaxVaryingVectors = 8;
  static const GLint kMaxVertexUniformVectors = 128;

  static const GLuint kServiceAttrib0BufferId = 801;
  static const GLuint kServiceFixedAttribBufferId = 802;

  static const GLuint kServiceBufferId = 301;
  static const GLuint kServiceFramebufferId = 302;
  static const GLuint kServiceRenderbufferId = 303;
  static const GLuint kServiceTextureId = 304;
  static const GLuint kServiceProgramId = 305;
  static const GLuint kServiceShaderId = 306;
  static const GLuint kServiceElementBufferId = 308;

  static const int32 kSharedMemoryId = 401;
  static const size_t kSharedBufferSize = 2048;
  static const uint32 kSharedMemoryOffset = 132;
  static const int32 kInvalidSharedMemoryId = 402;
  static const uint32 kInvalidSharedMemoryOffset = kSharedBufferSize + 1;
  static const uint32 kInitialResult = 0xBDBDBDBDu;
  static const uint8 kInitialMemoryValue = 0xBDu;

  static const uint32 kNewClientId = 501;
  static const uint32 kNewServiceId = 502;
  static const uint32 kInvalidClientId = 601;

  static const int kBackBufferWidth = 128;
  static const int kBackBufferHeight = 64;

  // Template to call glGenXXX functions.
  template <typename T>
  void GenHelper(GLuint client_id) {
    int8 buffer[sizeof(T) + sizeof(client_id)];
    T& cmd = *reinterpret_cast<T*>(&buffer);
    cmd.Init(1, &client_id);
    EXPECT_EQ(error::kNoError,
              ExecuteImmediateCmd(cmd, sizeof(client_id)));
  }

  // This template exists solely so we can specialize it for
  // certain commands.
  template <typename T, int id>
  void SpecializedSetup(bool valid) {
  }

  template <typename T>
  T* GetImmediateAs() {
    return reinterpret_cast<T*>(immediate_buffer_);
  }

  template <typename T, typename Command>
  T GetImmediateDataAs(Command* cmd) {
    return reinterpret_cast<T>(ImmediateDataAddress(cmd));
  }

  void ClearSharedMemory() {
    engine_->ClearSharedMemory();
  }

  virtual void SetUp();
  virtual void TearDown();

  template <typename T>
  error::Error ExecuteCmd(const T& cmd) {
    COMPILE_ASSERT(T::kArgFlags == cmd::kFixed, Cmd_kArgFlags_not_kFixed);
    return decoder_->DoCommand(cmd.kCmdId,
                               ComputeNumEntries(sizeof(cmd)) - 1,
                               &cmd);
  }

  template <typename T>
  error::Error ExecuteImmediateCmd(const T& cmd, size_t data_size) {
    COMPILE_ASSERT(T::kArgFlags == cmd::kAtLeastN, Cmd_kArgFlags_not_kAtLeastN);
    return decoder_->DoCommand(cmd.kCmdId,
                               ComputeNumEntries(sizeof(cmd) + data_size) - 1,
                               &cmd);
  }

  template <typename T>
  T GetSharedMemoryAs() {
    return reinterpret_cast<T>(shared_memory_address_);
  }

  template <typename T>
  T GetSharedMemoryAsWithOffset(uint32 offset) {
    void* ptr = reinterpret_cast<int8*>(shared_memory_address_) + offset;
    return reinterpret_cast<T>(ptr);
  }

  IdAllocator* GetIdAllocator(GLuint namespace_id) {
    return group_->GetIdAllocator(namespace_id);
  }

  BufferManager::BufferInfo* GetBufferInfo(GLuint service_id) {
    return group_->buffer_manager()->GetBufferInfo(service_id);
  }

  FramebufferManager::FramebufferInfo* GetFramebufferInfo(GLuint service_id) {
    return group_->framebuffer_manager()->GetFramebufferInfo(service_id);
  }

  RenderbufferManager::RenderbufferInfo* GetRenderbufferInfo(
      GLuint service_id) {
    return group_->renderbuffer_manager()->GetRenderbufferInfo(service_id);
  }

  TextureManager::TextureInfo* GetTextureInfo(GLuint service_id) {
    return group_->texture_manager()->GetTextureInfo(service_id);
  }

  ShaderManager::ShaderInfo* GetShaderInfo(GLuint service_id) {
    return group_->shader_manager()->GetShaderInfo(service_id);
  }

  ProgramManager::ProgramInfo* GetProgramInfo(GLuint service_id) {
    return group_->program_manager()->GetProgramInfo(service_id);
  }

  void DoCreateProgram(GLuint client_id, GLuint service_id);
  void DoCreateShader(GLenum shader_type, GLuint client_id, GLuint service_id);

  void SetBucketAsCString(uint32 bucket_id, const char* str);

  void InitDecoder(const char* extensions);

  const ContextGroup& group() const {
    return *group_.get();
  }

  struct AttribInfo {
    const char* name;
    GLint size;
    GLenum type;
    GLint location;
  };

  struct UniformInfo {
    const char* name;
    GLint size;
    GLenum type;
    GLint location;
  };

  void SetupShader(
      AttribInfo* attribs, size_t num_attribs,
      UniformInfo* uniforms, size_t num_uniforms,
      GLuint client_id, GLuint service_id,
      GLuint vertex_shader_client_id, GLuint vertex_shader_service_id,
      GLuint fragment_shader_client_id, GLuint fragment_shader_service_id);

  // Setups up a shader for testing glUniform.
  void SetupShaderForUniform();

  // Note that the error is returned as GLint instead of GLenum.
  // This is because there is a mismatch in the types of GLenum and
  // the error values GL_NO_ERROR, GL_INVALID_ENUM, etc. GLenum is
  // typedef'd as unsigned int while the error values are defined as
  // integers. This is problematic for template functions such as
  // EXPECT_EQ that expect both types to be the same.
  GLint GetGLError();

  void DoBindBuffer(GLenum target, GLuint client_id, GLuint service_id);
  void DoBindFramebuffer(GLenum target, GLuint client_id, GLuint service_id);
  void DoBindRenderbuffer(GLenum target, GLuint client_id, GLuint service_id);
  void DoBindTexture(GLenum target, GLuint client_id, GLuint service_id);

  bool DoIsBuffer(GLuint client_id);
  bool DoIsFramebuffer(GLuint client_id);
  bool DoIsProgram(GLuint client_id);
  bool DoIsRenderbuffer(GLuint client_id);
  bool DoIsShader(GLuint client_id);
  bool DoIsTexture(GLuint client_id);

  void DoDeleteBuffer(GLuint client_id, GLuint service_id);
  void DoDeleteFramebuffer(GLuint client_id, GLuint service_id);
  void DoDeleteProgram(GLuint client_id, GLuint service_id);
  void DoDeleteRenderbuffer(GLuint client_id, GLuint service_id);
  void DoDeleteShader(GLuint client_id, GLuint service_id);
  void DoDeleteTexture(GLuint client_id, GLuint service_id);

  void DoTexImage2D(GLenum target, GLint level, GLenum internal_format,
                    GLsizei width, GLsizei height, GLint border,
                    GLenum format, GLenum type,
                    uint32 shared_memory_id, uint32 shared_memory_offset);
  void DoVertexAttribPointer(
      GLuint index, GLint size, GLenum type, GLsizei stride, GLuint offset);

  void SetupExpectationsForFramebufferAttachment(
    GLuint clear_bits,
    GLclampf restore_red,
    GLclampf restore_green,
    GLclampf restore_blue,
    GLclampf restore_alpha,
    GLuint restore_color_mask,
    GLuint restore_stencil,
    GLuint restore_stencil_front_mask,
    GLuint restore_stencil_back_mask,
    GLclampf restore_depth,
    GLboolean restore_depth_mask,
    bool restore_scissor_test);

  GLvoid* BufferOffset(unsigned i) {
    return static_cast<int8 *>(NULL)+(i);
  }

  template <typename Command, typename Result>
  bool IsObjectHelper(GLuint client_id) {
    Result* result = static_cast<Result*>(shared_memory_address_);
    Command cmd;
    cmd.Init(client_id, kSharedMemoryId, kSharedMemoryOffset);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    bool isObject = static_cast<bool>(*result);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    return isObject;
  }

  // Use StrictMock to make 100% sure we know how GL will be called.
  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  gfx::StubGLContext* context_;
  scoped_ptr<GLES2Decoder> decoder_;

  GLuint client_buffer_id_;
  GLuint client_framebuffer_id_;
  GLuint client_program_id_;
  GLuint client_renderbuffer_id_;
  GLuint client_shader_id_;
  GLuint client_texture_id_;
  GLuint client_element_buffer_id_;

  uint32 shared_memory_id_;
  uint32 shared_memory_offset_;
  void* shared_memory_address_;

  int8 immediate_buffer_[256];

 private:
  class MockCommandBufferEngine : public CommandBufferEngine {
   public:
    MockCommandBufferEngine() {
      data_.reset(new int8[kSharedBufferSize]);
      ClearSharedMemory();
      valid_buffer_.ptr = data_.get();
      valid_buffer_.size = kSharedBufferSize;
    }

    virtual ~MockCommandBufferEngine() {
    }

    Buffer GetSharedMemoryBuffer(int32 shm_id) {
      return shm_id == kSharedMemoryId ? valid_buffer_ : invalid_buffer_;
    }

    void ClearSharedMemory() {
      memset(data_.get(), kInitialMemoryValue, kSharedBufferSize);
    }

    void set_token(int32 token) {
      DCHECK(false);
    }

    // Overridden from CommandBufferEngine.
    virtual bool SetGetOffset(int32 offset) {
      DCHECK(false);
      return false;
    }

    // Overridden from CommandBufferEngine.
    virtual int32 GetGetOffset() {
      DCHECK(false);
      return 0;
    }

   private:
    scoped_array<int8> data_;
    Buffer valid_buffer_;
    Buffer invalid_buffer_;
  };

  scoped_ptr< ::testing::StrictMock<MockCommandBufferEngine> > engine_;
  ContextGroup::Ref group_;
};

class GLES2DecoderWithShaderTestBase : public GLES2DecoderTestBase {
 public:
  GLES2DecoderWithShaderTestBase()
      : GLES2DecoderTestBase(),
        client_vertex_shader_id_(121),
        client_fragment_shader_id_(122) {
  }

  static const GLuint kServiceVertexShaderId = 321;
  static const GLuint kServiceFragmentShaderId = 322;

  static const GLsizei kNumVertices = 100;
  static const GLsizei kNumIndices = 10;
  static const int kValidIndexRangeStart = 1;
  static const int kValidIndexRangeCount = 7;
  static const int kInvalidIndexRangeStart = 0;
  static const int kInvalidIndexRangeCount = 7;
  static const int kOutOfRangeIndexRangeEnd = 10;
  static const GLuint kMaxValidIndex = 7;

  static const GLint kMaxAttribLength = 10;
  static const char* kAttrib1Name;
  static const char* kAttrib2Name;
  static const char* kAttrib3Name;
  static const GLint kAttrib1Size = 1;
  static const GLint kAttrib2Size = 1;
  static const GLint kAttrib3Size = 1;
  static const GLint kAttrib1Location = 0;
  static const GLint kAttrib2Location = 1;
  static const GLint kAttrib3Location = 2;
  static const GLenum kAttrib1Type = GL_FLOAT_VEC4;
  static const GLenum kAttrib2Type = GL_FLOAT_VEC2;
  static const GLenum kAttrib3Type = GL_FLOAT_VEC3;
  static const GLint kInvalidAttribLocation = 30;
  static const GLint kBadAttribIndex = kNumVertexAttribs;

  static const GLint kMaxUniformLength = 12;
  static const char* kUniform1Name;
  static const char* kUniform2Name;
  static const char* kUniform3Name;
  static const GLint kUniform1Size = 1;
  static const GLint kUniform2Size = 3;
  static const GLint kUniform3Size = 2;
  static const GLint kUniform1Location = 3;
  static const GLint kUniform2Location = 10;
  static const GLint kUniform2ElementLocation = 12;
  static const GLint kUniform3Location = 20;
  static const GLenum kUniform1Type = GL_SAMPLER_2D;
  static const GLenum kUniform2Type = GL_INT_VEC2;
  static const GLenum kUniform3Type = GL_FLOAT_VEC3;
  static const GLint kInvalidUniformLocation = 30;
  static const GLint kBadUniformIndex = 1000;

 protected:
  virtual void SetUp();
  virtual void TearDown();

  void SetupTexture();

  void DoEnableVertexAttribArray(GLint index);

  void DoBufferData(GLenum target, GLsizei size);

  void DoBufferSubData(
      GLenum target, GLint offset, GLsizei size, const void* data);

  void SetupVertexBuffer();

  void SetupIndexBuffer();

  void DeleteVertexBuffer();

  void DeleteIndexBuffer();

  GLuint client_vertex_shader_id_;
  GLuint client_fragment_shader_id_;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_BASE_H_
