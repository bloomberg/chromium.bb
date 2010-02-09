// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "base/string_util.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/gl_mock.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::gles2::MockGLInterface;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::MatcherCast;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace gpu {
namespace gles2 {

namespace {

bool IsInvalidPrefix(const char* name) {
  static const char kInvalidPrefix[] = { 'g', 'l', '_' };
  return memcmp(name, kInvalidPrefix, sizeof(kInvalidPrefix)) == 0;
}

}  // anonymous namespace

class GLES2DecoderTest : public testing::Test {
 public:
  GLES2DecoderTest()
      : client_buffer_id_(100),
        client_framebuffer_id_(101),
        client_program_id_(102),
        client_renderbuffer_id_(103),
        client_shader_id_(104),
        client_texture_id_(105),
        client_element_buffer_id_(106) {
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
  static const GLuint kServiceElementBufferId = 307;

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

  void ClearSharedMemory() {
    engine_->ClearSharedMemory();
  }

  virtual void SetUp() {
    gl_.reset(new StrictMock<MockGLInterface>());
    ::gles2::GLInterface::SetGLInterface(gl_.get());

    EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_VERTEX_ATTRIBS, _))
        .WillOnce(SetArgumentPointee<1>(kNumVertexAttribs))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GetError())
        .WillRepeatedly(Return(GL_NO_ERROR));

    engine_.reset(new StrictMock<MockCommandBufferEngine>());
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
    GenHelper<GenBuffersImmediate>(client_buffer_id_);
    EXPECT_CALL(*gl_, GenFramebuffersEXT(_, _))
        .WillOnce(SetArgumentPointee<1>(kServiceFramebufferId))
        .RetiresOnSaturation();
    GenHelper<GenFramebuffersImmediate>(client_framebuffer_id_);
    EXPECT_CALL(*gl_, GenRenderbuffersEXT(_, _))
        .WillOnce(SetArgumentPointee<1>(kServiceRenderbufferId))
        .RetiresOnSaturation();
    GenHelper<GenRenderbuffersImmediate>(client_renderbuffer_id_);
    EXPECT_CALL(*gl_, GenTextures(_, _))
        .WillOnce(SetArgumentPointee<1>(kServiceTextureId))
        .RetiresOnSaturation();
    GenHelper<GenTexturesImmediate>(client_texture_id_);
    EXPECT_CALL(*gl_, GenBuffersARB(_, _))
        .WillOnce(SetArgumentPointee<1>(kServiceElementBufferId))
        .RetiresOnSaturation();
    GenHelper<GenBuffersImmediate>(client_element_buffer_id_);

    {
      EXPECT_CALL(*gl_, CreateProgram())
          .Times(1)
          .WillOnce(Return(kServiceProgramId))
          .RetiresOnSaturation();
      CreateProgram cmd;
      cmd.Init(client_program_id_);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    }

    {
      EXPECT_CALL(*gl_, CreateShader(_))
          .Times(1)
          .WillOnce(Return(kServiceShaderId))
          .RetiresOnSaturation();
      CreateShader cmd;
      cmd.Init(GL_VERTEX_SHADER, client_shader_id_);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    }

    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  virtual void TearDown() {
    decoder_->Destroy();
    decoder_.reset();
    engine_.reset();
    ::gles2::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

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

  uint32 GetServiceId(uint32 client_id) {
    return decoder_->GetServiceIdForTesting(client_id);
  }

  // Note that the error is returned as GLint instead of GLenum.
  // This is because there is a mismatch in the types of GLenum and
  // the error values GL_NO_ERROR, GL_INVALID_ENUM, etc. GLenum is
  // typedef'd as unsigned int while the error values are defined as
  // integers. This is problematic for template functions such as
  // EXPECT_EQ that expect both types to be the same.
  GLint GetGLError() {
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
    GetError cmd;
    cmd.Init(shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    return static_cast<GLint>(*GetSharedMemoryAs<GLenum*>());
  }

  // Use StrictMock to make 100% sure we know how GL will be called.
  scoped_ptr<StrictMock<MockGLInterface> > gl_;
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

  scoped_ptr<StrictMock<MockCommandBufferEngine> > engine_;
};

const GLint GLES2DecoderTest::kNumVertexAttribs;
const GLuint GLES2DecoderTest::kServiceBufferId;
const GLuint GLES2DecoderTest::kServiceFramebufferId;
const GLuint GLES2DecoderTest::kServiceRenderbufferId;
const GLuint GLES2DecoderTest::kServiceTextureId;
const GLuint GLES2DecoderTest::kServiceProgramId;
const GLuint GLES2DecoderTest::kServiceShaderId;
const GLuint GLES2DecoderTest::kServiceElementBufferId;
const int32 GLES2DecoderTest::kSharedMemoryId;
const size_t GLES2DecoderTest::kSharedBufferSize;
const uint32 GLES2DecoderTest::kSharedMemoryOffset;
const int32 GLES2DecoderTest::kInvalidSharedMemoryId;
const uint32 GLES2DecoderTest::kInvalidSharedMemoryOffset;
const uint32 GLES2DecoderTest::kInitialResult;
const uint32 GLES2DecoderTest::kNewClientId;
const uint32 GLES2DecoderTest::kNewServiceId;
const uint32 GLES2DecoderTest::kInvalidClientId;

template <>
void GLES2DecoderTest::SpecializedSetup<LinkProgram, 0>() {
  InSequence dummy;
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_ACTIVE_ATTRIBUTES, _))
      .WillOnce(SetArgumentPointee<2>(0));
  EXPECT_CALL(
      *gl_,
      GetProgramiv(kServiceProgramId, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, _))
      .WillOnce(SetArgumentPointee<2>(0));
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_ACTIVE_UNIFORMS, _))
      .WillOnce(SetArgumentPointee<2>(0));
  EXPECT_CALL(
      *gl_,
      GetProgramiv(kServiceProgramId, GL_ACTIVE_UNIFORM_MAX_LENGTH, _))
      .WillOnce(SetArgumentPointee<2>(0));
};


class GLES2DecoderWithShaderTest : public GLES2DecoderTest {
 public:
  GLES2DecoderWithShaderTest()
      : GLES2DecoderTest() {
  }

  static const GLsizei kNumVertices = 100;
  static const GLsizei kNumIndices = 10;
  static const int kValidIndexRangeStart = 1;
  static const int kValidIndexRangeCount = 7;
  static const int kInvalidIndexRangeStart = 0;
  static const int kInvalidIndexRangeCount = 7;
  static const int kOutOfRangeIndexRangeEnd = 10;

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

  static const GLint kMaxUniformLength = 10;
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
  static const GLenum kUniform1Type = GL_FLOAT_VEC4;
  static const GLenum kUniform2Type = GL_INT_VEC2;
  static const GLenum kUniform3Type = GL_FLOAT_VEC3;
  static const GLint kInvalidUniformLocation = 30;
  static const GLint kBadUniformIndex = 1000;

 protected:
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

  virtual void SetUp() {
    GLES2DecoderTest::SetUp();

    {
      static AttribInfo attribs[] = {
        { kAttrib1Name, kAttrib1Size, kAttrib1Type, kAttrib1Location, },
        { kAttrib2Name, kAttrib2Size, kAttrib2Type, kAttrib2Location, },
        { kAttrib3Name, kAttrib3Size, kAttrib3Type, kAttrib3Location, },
      };
      static UniformInfo uniforms[] = {
        { kUniform1Name, kUniform1Size, GL_FLOAT_VEC4, kUniform1Location, },
        { kUniform2Name, kUniform2Size, GL_INT_VEC2, kUniform2Location, },
        { kUniform3Name, kUniform3Size, GL_FLOAT_VEC3, kUniform3Location, },
      };
      SetupShader(attribs, arraysize(attribs), uniforms, arraysize(uniforms),
                  client_program_id_, kServiceProgramId);
    }

    {
      EXPECT_CALL(*gl_, UseProgram(kServiceProgramId))
          .Times(1)
          .RetiresOnSaturation();
      UseProgram cmd;
      cmd.Init(client_program_id_);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    }
  }

  virtual void TearDown() {
    GLES2DecoderTest::TearDown();
  }

  void SetupShader(AttribInfo* attribs, size_t num_attribs,
                   UniformInfo* uniforms, size_t num_uniforms,
                   GLuint client_id, GLuint service_id) {
    LinkProgram cmd;
    cmd.Init(client_id);

    {
      InSequence s;
      EXPECT_CALL(*gl_, LinkProgram(service_id))
          .Times(1)
          .RetiresOnSaturation();
      EXPECT_CALL(*gl_, GetError())
          .WillOnce(Return(GL_NO_ERROR))
          .RetiresOnSaturation();
      EXPECT_CALL(*gl_,
          GetProgramiv(service_id, GL_ACTIVE_ATTRIBUTES, _))
          .WillOnce(SetArgumentPointee<2>(num_attribs))
          .RetiresOnSaturation();
      EXPECT_CALL(*gl_,
          GetProgramiv(service_id, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, _))
          .WillOnce(SetArgumentPointee<2>(kMaxAttribLength))
          .RetiresOnSaturation();
      for (size_t ii = 0; ii < num_attribs; ++ii) {
        const AttribInfo& info = attribs[ii];
        EXPECT_CALL(*gl_,
            GetActiveAttrib(service_id, ii,
                            kMaxAttribLength, _, _, _, _))
            .WillOnce(DoAll(
                SetArgumentPointee<3>(strlen(info.name)),
                SetArgumentPointee<4>(info.size),
                SetArgumentPointee<5>(info.type),
                SetArrayArgument<6>(info.name,
                                    info.name + strlen(info.name) + 1)))
            .RetiresOnSaturation();
        if (!IsInvalidPrefix(info.name)) {
          EXPECT_CALL(*gl_, GetAttribLocation(service_id,
                                              StrEq(info.name)))
              .WillOnce(Return(info.location))
              .RetiresOnSaturation();
        }
      }
      EXPECT_CALL(*gl_,
          GetProgramiv(service_id, GL_ACTIVE_UNIFORMS, _))
          .WillOnce(SetArgumentPointee<2>(num_uniforms))
          .RetiresOnSaturation();
      EXPECT_CALL(*gl_,
          GetProgramiv(service_id, GL_ACTIVE_UNIFORM_MAX_LENGTH, _))
          .WillOnce(SetArgumentPointee<2>(kMaxUniformLength))
          .RetiresOnSaturation();
      for (size_t ii = 0; ii < num_uniforms; ++ii) {
        const UniformInfo& info = uniforms[ii];
        EXPECT_CALL(*gl_,
            GetActiveUniform(service_id, ii,
                             kMaxUniformLength, _, _, _, _))
            .WillOnce(DoAll(
                SetArgumentPointee<3>(strlen(info.name)),
                SetArgumentPointee<4>(info.size),
                SetArgumentPointee<5>(info.type),
                SetArrayArgument<6>(info.name,
                                    info.name + strlen(info.name) + 1)))
            .RetiresOnSaturation();
        if (!IsInvalidPrefix(info.name)) {
          EXPECT_CALL(*gl_, GetUniformLocation(service_id,
                                               StrEq(info.name)))
              .WillOnce(Return(info.location))
              .RetiresOnSaturation();
          if (info.size > 1) {
            for (GLsizei jj = 1; jj < info.size; ++jj) {
              std::string element_name(
                  std::string(info.name) + "[" + IntToString(jj) + "]");
              EXPECT_CALL(*gl_, GetUniformLocation(service_id,
                                                   StrEq(element_name)))
                  .WillOnce(Return(info.location + jj))
                  .RetiresOnSaturation();
            }
          }
        }
      }
    }

    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  inline GLvoid* BufferOffset(unsigned i) {
    return static_cast<int8 *>(NULL)+(i);
  }

  void DoEnableVertexAttribArray(GLint index) {
    EXPECT_CALL(*gl_, EnableVertexAttribArray(index))
        .Times(1)
        .RetiresOnSaturation();
    EnableVertexAttribArray cmd;
    cmd.Init(index);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  void DoBindBuffer(GLenum target, GLuint client_id, GLuint service_id) {
    EXPECT_CALL(*gl_, BindBuffer(target, service_id))
        .Times(1)
        .RetiresOnSaturation();
    BindBuffer cmd;
    cmd.Init(target, client_id);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  void DoBufferData(GLenum target, GLsizei size) {
    EXPECT_CALL(*gl_, BufferData(target, size, _, GL_STREAM_DRAW))
        .Times(1)
        .RetiresOnSaturation();
    BufferData cmd;
    cmd.Init(target, size, 0, 0, GL_STREAM_DRAW);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  void DoBufferSubData(
      GLenum target, GLint offset, GLsizei size, const void* data) {
    EXPECT_CALL(*gl_, BufferSubData(target, offset, size,
                                    shared_memory_address_))
        .Times(1)
        .RetiresOnSaturation();
    memcpy(shared_memory_address_, data, size);
    BufferSubData cmd;
    cmd.Init(target, offset, size, shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  void DoDeleteBuffer(GLuint client_id, GLuint service_id) {
    EXPECT_CALL(*gl_, DeleteBuffersARB(1, Pointee(service_id)))
        .Times(1)
        .RetiresOnSaturation();
    DeleteBuffers cmd;
    cmd.Init(1, shared_memory_id_, shared_memory_offset_);
    memcpy(shared_memory_address_, &client_id, sizeof(client_id));
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  void DoDeleteProgram(GLuint client_id, GLuint service_id) {
    EXPECT_CALL(*gl_, DeleteProgram(service_id))
        .Times(1)
        .RetiresOnSaturation();
    DeleteProgram cmd;
    cmd.Init(client_id);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  void DoVertexAttribPointer(
      GLuint index, GLint size, GLenum type, GLsizei stride, GLuint offset) {
    EXPECT_CALL(*gl_,
                VertexAttribPointer(index, size, type, GL_FALSE, stride,
                                    BufferOffset(offset)))
        .Times(1)
        .RetiresOnSaturation();
    VertexAttribPointer cmd;
    cmd.Init(index, size, GL_FLOAT, GL_FALSE, stride, offset);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  void SetupVertexBuffer() {
    DoEnableVertexAttribArray(1);
    DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
    GLfloat f = 0;
    DoBufferData(GL_ARRAY_BUFFER, kNumVertices * 2 * sizeof(f));
  }

  void SetupIndexBuffer() {
    DoBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                 client_element_buffer_id_,
                 kServiceElementBufferId);
    static const GLshort indices[] = {100, 1, 2, 3, 4, 5, 6, 7, 100, 9};
    COMPILE_ASSERT(arraysize(indices) == kNumIndices, Indices_is_not_10);
    DoBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices));
    DoBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(indices), indices);
  }

  void DeleteVertexBuffer() {
    DoDeleteBuffer(client_buffer_id_, kServiceBufferId);
  }

  void DeleteIndexBuffer() {
    DoDeleteBuffer(client_element_buffer_id_, kServiceElementBufferId);
  }
};

const GLint GLES2DecoderWithShaderTest::kMaxAttribLength;
const GLsizei GLES2DecoderWithShaderTest::kNumVertices;
const GLsizei GLES2DecoderWithShaderTest::kNumIndices;
const int GLES2DecoderWithShaderTest::kValidIndexRangeStart;
const int GLES2DecoderWithShaderTest::kValidIndexRangeCount;
const int GLES2DecoderWithShaderTest::kInvalidIndexRangeStart;
const int GLES2DecoderWithShaderTest::kInvalidIndexRangeCount;
const int GLES2DecoderWithShaderTest::kOutOfRangeIndexRangeEnd;
const char* GLES2DecoderWithShaderTest::kAttrib1Name = "attrib1";
const char* GLES2DecoderWithShaderTest::kAttrib2Name = "attrib2";
const char* GLES2DecoderWithShaderTest::kAttrib3Name = "attrib3";
const GLint GLES2DecoderWithShaderTest::kAttrib1Size;
const GLint GLES2DecoderWithShaderTest::kAttrib2Size;
const GLint GLES2DecoderWithShaderTest::kAttrib3Size;
const GLint GLES2DecoderWithShaderTest::kAttrib1Location;
const GLint GLES2DecoderWithShaderTest::kAttrib2Location;
const GLint GLES2DecoderWithShaderTest::kAttrib3Location;
const GLenum GLES2DecoderWithShaderTest::kAttrib1Type;
const GLenum GLES2DecoderWithShaderTest::kAttrib2Type;
const GLenum GLES2DecoderWithShaderTest::kAttrib3Type;
const GLint GLES2DecoderWithShaderTest::kInvalidAttribLocation;
const GLint GLES2DecoderWithShaderTest::kBadAttribIndex;
const GLint GLES2DecoderWithShaderTest::kMaxUniformLength;
const char* GLES2DecoderWithShaderTest::kUniform1Name = "uniform1";
const char* GLES2DecoderWithShaderTest::kUniform2Name = "uniform2";
const char* GLES2DecoderWithShaderTest::kUniform3Name = "uniform3";
const GLint GLES2DecoderWithShaderTest::kUniform1Size;
const GLint GLES2DecoderWithShaderTest::kUniform2Size;
const GLint GLES2DecoderWithShaderTest::kUniform3Size;
const GLint GLES2DecoderWithShaderTest::kUniform1Location;
const GLint GLES2DecoderWithShaderTest::kUniform2Location;
const GLint GLES2DecoderWithShaderTest::kUniform2ElementLocation;
const GLint GLES2DecoderWithShaderTest::kUniform3Location;
const GLenum GLES2DecoderWithShaderTest::kUniform1Type;
const GLenum GLES2DecoderWithShaderTest::kUniform2Type;
const GLenum GLES2DecoderWithShaderTest::kUniform3Type;
const GLint GLES2DecoderWithShaderTest::kInvalidUniformLocation;
const GLint GLES2DecoderWithShaderTest::kBadUniformIndex;

TEST_F(GLES2DecoderWithShaderTest, DrawArraysNoAttributesSucceeds) {
  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysMissingAttributesFails) {
  DoEnableVertexAttribArray(1);

  EXPECT_CALL(*gl_, DrawArrays(_, _, _))
      .Times(0);
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysValidAttributesSucceeds) {
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysDeletedBufferFails) {
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);
  DeleteVertexBuffer();

  EXPECT_CALL(*gl_, DrawArrays(_, _, _))
      .Times(0);
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysDeletedProgramFails) {
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);
  DoDeleteProgram(client_program_id_, kServiceProgramId);

  EXPECT_CALL(*gl_, DrawArrays(_, _, _))
      .Times(0);
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysWithInvalidModeFails) {
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawArrays(_, _, _))
      .Times(0);
  DrawArrays cmd;
  cmd.Init(GL_QUADS, 0, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_POLYGON, 0, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawArraysInvalidCountFails) {
  SetupVertexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  // Try start > 0
  EXPECT_CALL(*gl_, DrawArrays(_, _, _)).Times(0);
  DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 1, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try with count > size
  cmd.Init(GL_TRIANGLES, 0, kNumVertices + 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try with attrib offset > 0
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try with size > 2 (ie, vec3 instead of vec2)
  DoVertexAttribPointer(1, 3, GL_FLOAT, 0, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try with stride > 8 (vec2 + vec2 byte)
  GLfloat f;
  DoVertexAttribPointer(1, 2, GL_FLOAT, sizeof(f) * 2 + 1, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsNoAttributesSucceeds) {
  SetupIndexBuffer();
  EXPECT_CALL(*gl_, DrawElements(GL_TRIANGLES, kValidIndexRangeCount,
                                 GL_UNSIGNED_SHORT,
                                 BufferOffset(kValidIndexRangeStart)))
      .Times(1)
      .RetiresOnSaturation();
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsMissingAttributesFails) {
  SetupIndexBuffer();
  DoEnableVertexAttribArray(1);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _))
      .Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsValidAttributesSucceeds) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawElements(GL_TRIANGLES, kValidIndexRangeCount,
                                 GL_UNSIGNED_SHORT,
                                 BufferOffset(kValidIndexRangeStart)))
      .Times(1)
      .RetiresOnSaturation();
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsDeletedBufferFails) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);
  DeleteIndexBuffer();

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _))
      .Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsDeleteProgramFails) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);
  DoDeleteProgram(client_program_id_, kServiceProgramId);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _))
      .Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsWithInvalidModeFails) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _))
      .Times(0);
  DrawElements cmd;
  cmd.Init(GL_QUADS, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_POLYGON, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, DrawElementsInvalidCountFails) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  // Try start > 0
  EXPECT_CALL(*gl_, DrawElements(_, _, _, _)).Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kNumIndices, GL_UNSIGNED_SHORT, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try with count > size
  cmd.Init(GL_TRIANGLES, kNumIndices + 1, GL_UNSIGNED_SHORT, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

#if 0  // TODO(gman): Turn on this test once buffer validation is in
TEST_F(GLES2DecoderWithShaderTest, DrawElementsOutOfRangeIndicesFails) {
  SetupVertexBuffer();
  SetupIndexBuffer();
  DoVertexAttribPointer(1, 2, GL_FLOAT, 0, 0);

  EXPECT_CALL(*gl_, DrawElements(_, _, _, _)).Times(0);
  DrawElements cmd;
  cmd.Init(GL_TRIANGLES, kInvalidIndexRangeCount, GL_UNSIGNED_SHORT,
           kInvalidIndexRangeStart);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
#endif

TEST_F(GLES2DecoderWithShaderTest, GetVertexAttribPointervSucceeds) {
  const float dummy = 0;
  const GLuint kOffsetToTestFor = sizeof(dummy) * 4;
  const GLuint kIndexToTest = 1;
  GetVertexAttribPointerv::Result* result =
      static_cast<GetVertexAttribPointerv::Result*>(shared_memory_address_);
  const GLuint* result_value = result->GetData();
  // Test that initial value is 0.
  GetVertexAttribPointerv cmd;
  cmd.Init(kIndexToTest, GL_VERTEX_ATTRIB_ARRAY_POINTER,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(sizeof(*result_value), result->size);
  EXPECT_EQ(0u, *result_value);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Set the value and see that we get it.
  SetupVertexBuffer();
  DoVertexAttribPointer(kIndexToTest, 2, GL_FLOAT, 0, kOffsetToTestFor);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(sizeof(*result_value), result->size);
  EXPECT_EQ(kOffsetToTestFor, *result_value);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetVertexAttribPointervBadArgsFails) {
  const GLuint kIndexToTest = 1;
  GetVertexAttribPointerv::Result* result =
      static_cast<GetVertexAttribPointerv::Result*>(shared_memory_address_);
  const GLuint* result_value = result->GetData();
  // Test pname invalid fails.
  GetVertexAttribPointerv cmd;
  cmd.Init(kIndexToTest, GL_VERTEX_ATTRIB_ARRAY_POINTER + 1,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(kInitialResult, *result_value);
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  // Test index out of range fails.
  cmd.Init(kNumVertexAttribs, GL_VERTEX_ATTRIB_ARRAY_POINTER,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(kInitialResult, *result_value);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

  // Test memory id bad fails.
  cmd.Init(kIndexToTest, GL_VERTEX_ATTRIB_ARRAY_POINTER,
           kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));

  // Test memory offset bad fails.
  cmd.Init(kIndexToTest, GL_VERTEX_ATTRIB_ARRAY_POINTER,
           shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformivSucceeds) {
  GetUniformiv::Result* result =
      static_cast<GetUniformiv::Result*>(shared_memory_address_);
  GetUniformiv cmd;
  cmd.Init(client_program_id_, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformiv(kServiceProgramId, kUniform2Location, _))
      .Times(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GLES2Util::GetGLDataTypeSize(kUniform2Type), result->size);
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformivArrayElementSucceeds) {
  GetUniformiv::Result* result =
      static_cast<GetUniformiv::Result*>(shared_memory_address_);
  GetUniformiv cmd;
  cmd.Init(client_program_id_, kUniform2ElementLocation,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_,
              GetUniformiv(kServiceProgramId, kUniform2ElementLocation, _))
      .Times(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GLES2Util::GetGLDataTypeSize(kUniform2Type), result->size);
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformivBadProgramFails) {
  GetUniformiv::Result* result =
      static_cast<GetUniformiv::Result*>(shared_memory_address_);
  GetUniformiv cmd;
  // non-existant program
  cmd.Init(kInvalidClientId, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformiv(_, _, _))
      .Times(0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  // Valid id that is not a program. The GL spec requires a different error for
  // this case.
  result->size = kInitialResult;
  cmd.Init(client_texture_id_, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  // Unlinked program
  EXPECT_CALL(*gl_, CreateProgram())
      .Times(1)
      .WillOnce(Return(kNewServiceId))
      .RetiresOnSaturation();
  CreateProgram cmd2;
  cmd2.Init(kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  result->size = kInitialResult;
  cmd.Init(kNewClientId, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformivBadLocationFails) {
  GetUniformiv::Result* result =
      static_cast<GetUniformiv::Result*>(shared_memory_address_);
  GetUniformiv cmd;
  // invalid location
  cmd.Init(client_program_id_, kInvalidUniformLocation,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformiv(_, _, _))
      .Times(0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformivBadSharedMemoryFails) {
  GetUniformiv cmd;
  cmd.Init(client_program_id_, kUniform2Location,
           kInvalidSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformiv(_, _, _))
      .Times(0);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kUniform2Location,
           kSharedMemoryId, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
};

TEST_F(GLES2DecoderWithShaderTest, GetUniformfvSucceeds) {
  GetUniformfv::Result* result =
      static_cast<GetUniformfv::Result*>(shared_memory_address_);
  GetUniformfv cmd;
  cmd.Init(client_program_id_, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformfv(kServiceProgramId, kUniform2Location, _))
      .Times(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GLES2Util::GetGLDataTypeSize(kUniform2Type), result->size);
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformfvArrayElementSucceeds) {
  GetUniformfv::Result* result =
      static_cast<GetUniformfv::Result*>(shared_memory_address_);
  GetUniformfv cmd;
  cmd.Init(client_program_id_, kUniform2ElementLocation,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_,
              GetUniformfv(kServiceProgramId, kUniform2ElementLocation, _))
      .Times(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GLES2Util::GetGLDataTypeSize(kUniform2Type), result->size);
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformfvBadProgramFails) {
  GetUniformfv::Result* result =
      static_cast<GetUniformfv::Result*>(shared_memory_address_);
  GetUniformfv cmd;
  // non-existant program
  cmd.Init(kInvalidClientId, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformfv(_, _, _))
      .Times(0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  // Valid id that is not a program. The GL spec requires a different error for
  // this case.
  result->size = kInitialResult;
  cmd.Init(client_texture_id_, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  // Unlinked program
  EXPECT_CALL(*gl_, CreateProgram())
      .Times(1)
      .WillOnce(Return(kNewServiceId))
      .RetiresOnSaturation();
  CreateProgram cmd2;
  cmd2.Init(kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  result->size = kInitialResult;
  cmd.Init(kNewClientId, kUniform2Location,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformfvBadLocationFails) {
  GetUniformfv::Result* result =
      static_cast<GetUniformfv::Result*>(shared_memory_address_);
  GetUniformfv cmd;
  // invalid location
  cmd.Init(client_program_id_, kInvalidUniformLocation,
           kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformfv(_, _, _))
      .Times(0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetUniformfvBadSharedMemoryFails) {
  GetUniformfv cmd;
  cmd.Init(client_program_id_, kUniform2Location,
           kInvalidSharedMemoryId, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformfv(_, _, _))
      .Times(0);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kUniform2Location,
           kSharedMemoryId, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
};

TEST_F(GLES2DecoderWithShaderTest, GetAttachedShadersSucceeds) {
  GetAttachedShaders cmd;
  typedef GetAttachedShaders::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetAttachedShaders(kServiceProgramId, 1, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(1),
                      SetArgumentPointee<3>(kServiceShaderId)));
  cmd.Init(client_program_id_, shared_memory_id_, shared_memory_offset_,
           Result::ComputeSize(1));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(1, result->GetNumResults());
  EXPECT_EQ(client_shader_id_, result->GetData()[0]);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetAttachedShadersBadProgramFails) {
  GetAttachedShaders cmd;
  typedef GetAttachedShaders::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetAttachedShaders(_, _, _, _))
      .Times(0);
  cmd.Init(kInvalidClientId, shared_memory_id_, shared_memory_offset_,
           Result::ComputeSize(1));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetAttachedShadersBadSharedMemoryFails) {
  GetAttachedShaders cmd;
  typedef GetAttachedShaders::Result Result;
  cmd.Init(client_program_id_, kInvalidSharedMemoryId, shared_memory_offset_,
           Result::ComputeSize(1));
  EXPECT_CALL(*gl_, GetAttachedShaders(_, _, _, _))
      .Times(0);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, shared_memory_id_, kInvalidSharedMemoryOffset,
           Result::ComputeSize(1));
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetShaderPrecisionFormatSucceeds) {
  GetShaderPrecisionFormat cmd;
  typedef GetShaderPrecisionFormat::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  // NOTE: GL will not be called. There is no equivalent Desktop OpenGL
  // function.
  cmd.Init(GL_VERTEX_SHADER, GL_HIGH_FLOAT,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_NE(0, result->success);
  EXPECT_EQ(-62, result->min_range);
  EXPECT_EQ(62, result->max_range);
  EXPECT_EQ(-16, result->precision);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetShaderPrecisionFormatBadArgsFails) {
  GetShaderPrecisionFormat cmd;
  cmd.Init(GL_TEXTURE_2D, GL_HIGH_FLOAT,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_VERTEX_SHADER, GL_TEXTURE_2D,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest,
       GetShaderPrecisionFormatBadSharedMemoryFails) {
  GetShaderPrecisionFormat cmd;
  cmd.Init(GL_VERTEX_SHADER, GL_HIGH_FLOAT,
           kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(GL_VERTEX_SHADER, GL_TEXTURE_2D,
           shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveUniformSucceeds) {
  const GLuint kUniformIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveUniform cmd;
  typedef GetActiveUniform::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  cmd.Init(client_program_id_, kUniformIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_NE(0, result->success);
  EXPECT_EQ(kUniform2Size, result->size);
  EXPECT_EQ(kUniform2Type, result->type);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != NULL);
  EXPECT_EQ(0, memcmp(bucket->GetData(0, bucket->size()), kUniform2Name,
                      bucket->size()));
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveUniformBadProgramFails) {
  const GLuint kUniformIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveUniform cmd;
  typedef GetActiveUniform::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  cmd.Init(kInvalidClientId, kUniformIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(client_texture_id_, kUniformIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveUniformBadIndexFails) {
  const uint32 kBucketId = 123;
  GetActiveUniform cmd;
  typedef GetActiveUniform::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  cmd.Init(client_program_id_, kBadUniformIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveUniformBadSharedMemoryFails) {
  const GLuint kUniformIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveUniform cmd;
  typedef GetActiveUniform::Result Result;
  cmd.Init(client_program_id_, kUniformIndex, kBucketId,
           kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kUniformIndex, kBucketId,
           shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveAttribSucceeds) {
  const GLuint kAttribIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveAttrib cmd;
  typedef GetActiveAttrib::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  cmd.Init(client_program_id_, kAttribIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_NE(0, result->success);
  EXPECT_EQ(kAttrib2Size, result->size);
  EXPECT_EQ(kAttrib2Type, result->type);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != NULL);
  EXPECT_EQ(0, memcmp(bucket->GetData(0, bucket->size()), kAttrib2Name,
                      bucket->size()));
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveAttribBadProgramFails) {
  const GLuint kAttribIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveAttrib cmd;
  typedef GetActiveAttrib::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  cmd.Init(kInvalidClientId, kAttribIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(client_texture_id_, kAttribIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveAttribBadIndexFails) {
  const uint32 kBucketId = 123;
  GetActiveAttrib cmd;
  typedef GetActiveAttrib::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  cmd.Init(client_program_id_, kBadAttribIndex, kBucketId,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_F(GLES2DecoderWithShaderTest, GetActiveAttribBadSharedMemoryFails) {
  const GLuint kAttribIndex = 1;
  const uint32 kBucketId = 123;
  GetActiveAttrib cmd;
  typedef GetActiveAttrib::Result Result;
  cmd.Init(client_program_id_, kAttribIndex, kBucketId,
           kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kAttribIndex, kBucketId,
           shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

// TODO(gman): BindAttribLocation

// TODO(gman): BindAttribLocationImmediate

// TODO(gman): BufferData

// TODO(gman): BufferDataImmediate

// TODO(gman): BufferSubData

// TODO(gman): BufferSubDataImmediate

// TODO(gman): CompressedTexImage2D

// TODO(gman): CompressedTexImage2DImmediate

// TODO(gman): CompressedTexSubImage2D

// TODO(gman): CompressedTexSubImage2DImmediate

// TODO(gman): DeleteProgram

// TODO(gman): DeleteShader

// TODO(gman): GetAttribLocation

// TODO(gman): GetAttribLocationImmediate

// TODO(gman): GetUniformLocation

// TODO(gman): GetUniformLocationImmediate

// TODO(gman): PixelStorei

// TODO(gman): ReadPixels

// TODO(gman): ShaderSource

// TODO(gman): ShaderSourceImmediate

#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_autogen.h"

}  // namespace gles2
}  // namespace gpu


