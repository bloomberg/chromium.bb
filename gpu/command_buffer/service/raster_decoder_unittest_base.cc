// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/raster_decoder_unittest_base.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/raster_cmd_format.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/command_buffer/service/vertex_attrib_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

using ::gl::MockGLInterface;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::InSequence;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

using namespace gpu::gles2;

namespace gpu {
namespace raster {

RasterDecoderTestBase::RasterDecoderTestBase()
    : surface_(NULL),
      context_(NULL),
      memory_tracker_(NULL),
      client_texture_id_(106),
      shared_memory_id_(0),
      shared_memory_offset_(0),
      shared_memory_address_(nullptr),
      shared_memory_base_(nullptr),
      ignore_cached_state_for_test_(GetParam()),
      shader_translator_cache_(gpu_preferences_) {
  memset(immediate_buffer_, 0xEE, sizeof(immediate_buffer_));
}

RasterDecoderTestBase::~RasterDecoderTestBase() = default;

void RasterDecoderTestBase::OnConsoleMessage(int32_t id,
                                             const std::string& message) {}
void RasterDecoderTestBase::CacheShader(const std::string& key,
                                        const std::string& shader) {}
void RasterDecoderTestBase::OnFenceSyncRelease(uint64_t release) {}
bool RasterDecoderTestBase::OnWaitSyncToken(const gpu::SyncToken&) {
  return false;
}
void RasterDecoderTestBase::OnDescheduleUntilFinished() {}
void RasterDecoderTestBase::OnRescheduleAfterFinished() {}

void RasterDecoderTestBase::SetUp() {
  InitDecoderWithWorkarounds();
}

void RasterDecoderTestBase::InitDecoderWithWorkarounds() {
  const std::string extensions("GL_EXT_framebuffer_object ");
  const std::string gl_version("2.1");
  const bool bind_generates_resource(false);
  const bool lose_context_when_out_of_memory(false);
  const ContextType context_type(CONTEXT_TYPE_OPENGLES2);

  // For easier substring/extension matching
  gl::SetGLGetProcAddressProc(gl::MockGLInterface::GetGLProcAddress);
  gl::GLSurfaceTestSupport::InitializeOneOffWithMockBindings();

  gl_.reset(new StrictMock<MockGLInterface>());
  ::gl::MockGLInterface::SetGLInterface(gl_.get());

  gpu::GpuDriverBugWorkarounds workarounds;
  scoped_refptr<FeatureInfo> feature_info = new FeatureInfo(workarounds);

  group_ = scoped_refptr<ContextGroup>(new ContextGroup(
      gpu_preferences_, false, &mailbox_manager_, memory_tracker_,
      &shader_translator_cache_, &framebuffer_completeness_cache_, feature_info,
      bind_generates_resource, &image_manager_, nullptr /* image_factory */,
      nullptr /* progress_reporter */, GpuFeatureInfo(),
      &discardable_manager_));

  InSequence sequence;

  surface_ = new gl::GLSurfaceStub;

  // Context needs to be created before initializing ContextGroup, which willxo
  // in turn initialize FeatureInfo, which needs a context to determine
  // extension support.
  context_ = new StrictMock<GLContextMock>();
  context_->SetExtensionsString(extensions.c_str());
  context_->SetGLVersionString(gl_version.c_str());

  context_->GLContextStub::MakeCurrent(surface_.get());

  TestHelper::SetupContextGroupInitExpectations(
      gl_.get(), DisallowedFeatures(), extensions.c_str(), gl_version.c_str(),
      context_type, bind_generates_resource);

  // We initialize the ContextGroup with a MockRasterDecoder so that
  // we can use the ContextGroup to figure out how the real RasterDecoder
  // will initialize itself.
  command_buffer_service_.reset(new FakeCommandBufferServiceBase());
  mock_decoder_.reset(new MockRasterDecoder(command_buffer_service_.get()));

  EXPECT_EQ(group_->Initialize(mock_decoder_.get(), context_type,
                               DisallowedFeatures()),
            gpu::ContextResult::kSuccess);

  scoped_refptr<gpu::Buffer> buffer =
      command_buffer_service_->CreateTransferBufferHelper(kSharedBufferSize,
                                                          &shared_memory_id_);
  shared_memory_offset_ = kSharedMemoryOffset;
  shared_memory_address_ =
      reinterpret_cast<int8_t*>(buffer->memory()) + shared_memory_offset_;
  shared_memory_base_ = buffer->memory();
  ClearSharedMemory();

  ContextCreationAttribs attribs;
  attribs.lose_context_when_out_of_memory = lose_context_when_out_of_memory;
  attribs.context_type = context_type;

  bool use_default_textures = bind_generates_resource;
  for (GLint tt = 0; tt < TestHelper::kNumTextureUnits; ++tt) {
    EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE0 + tt))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D,
                                  use_default_textures
                                      ? TestHelper::kServiceDefaultTexture2dId
                                      : 0))
        .Times(1)
        .RetiresOnSaturation();
  }
  EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE0)).Times(1).RetiresOnSaturation();

  decoder_.reset(RasterDecoder::Create(this, command_buffer_service_.get(),
                                       &outputter_, group_.get()));
  decoder_->SetIgnoreCachedStateForTest(ignore_cached_state_for_test_);
  decoder_->GetLogger()->set_log_synthesized_gl_errors(false);
  ASSERT_EQ(decoder_->Initialize(surface_, context_, true, DisallowedFeatures(),
                                 attribs),
            gpu::ContextResult::kSuccess);

  EXPECT_CALL(*context_, MakeCurrent(surface_.get())).WillOnce(Return(true));
  if (context_->WasAllocatedUsingRobustnessExtension()) {
    EXPECT_CALL(*gl_, GetGraphicsResetStatusARB())
        .WillOnce(Return(GL_NO_ERROR));
  }
  decoder_->MakeCurrent();
  decoder_->BeginDecoding();

  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenTexturesImmediate>(client_texture_id_);

  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

void RasterDecoderTestBase::ResetDecoder() {
  if (!decoder_.get())
    return;
  // All Tests should have read all their GLErrors before getting here.
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  decoder_->EndDecoding();
  decoder_->Destroy(!decoder_->WasContextLost());
  decoder_.reset();
  group_->Destroy(mock_decoder_.get(), false);
  command_buffer_service_.reset();
  ::gl::MockGLInterface::SetGLInterface(NULL);
  gl_.reset();
  gl::init::ShutdownGL(false);
}

void RasterDecoderTestBase::TearDown() {
  ResetDecoder();
}

GLint RasterDecoderTestBase::GetGLError() {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  cmds::GetError cmd;
  cmd.Init(shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  return static_cast<GLint>(*GetSharedMemoryAs<GLenum*>());
}

void RasterDecoderTestBase::SetBucketData(uint32_t bucket_id,
                                          const void* data,
                                          uint32_t data_size) {
  DCHECK(data || data_size == 0);
  cmd::SetBucketSize cmd1;
  cmd1.Init(bucket_id, data_size);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd1));
  if (data) {
    memcpy(shared_memory_address_, data, data_size);
    cmd::SetBucketData cmd2;
    cmd2.Init(bucket_id, 0, data_size, shared_memory_id_, kSharedMemoryOffset);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
    ClearSharedMemory();
  }
}

void RasterDecoderTestBase::SetBucketAsCString(uint32_t bucket_id,
                                               const char* str) {
  SetBucketData(bucket_id, str, str ? (strlen(str) + 1) : 0);
}

void RasterDecoderTestBase::SetBucketAsCStrings(uint32_t bucket_id,
                                                GLsizei count,
                                                const char** str,
                                                GLsizei count_in_header,
                                                char str_end) {
  uint32_t header_size = sizeof(GLint) * (count + 1);
  uint32_t total_size = header_size;
  std::unique_ptr<GLint[]> header(new GLint[count + 1]);
  header[0] = static_cast<GLint>(count_in_header);
  for (GLsizei ii = 0; ii < count; ++ii) {
    header[ii + 1] = str && str[ii] ? strlen(str[ii]) : 0;
    total_size += header[ii + 1] + 1;
  }
  cmd::SetBucketSize cmd1;
  cmd1.Init(bucket_id, total_size);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd1));
  memcpy(shared_memory_address_, header.get(), header_size);
  uint32_t offset = header_size;
  for (GLsizei ii = 0; ii < count; ++ii) {
    if (str && str[ii]) {
      size_t str_len = strlen(str[ii]);
      memcpy(reinterpret_cast<char*>(shared_memory_address_) + offset, str[ii],
             str_len);
      offset += str_len;
    }
    memcpy(reinterpret_cast<char*>(shared_memory_address_) + offset, &str_end,
           1);
    offset += 1;
  }
  cmd::SetBucketData cmd2;
  cmd2.Init(bucket_id, 0, total_size, shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  ClearSharedMemory();
}

void RasterDecoderTestBase::DoBindTexture(GLenum target,
                                          GLuint client_id,
                                          GLuint service_id) {
  EXPECT_CALL(*gl_, BindTexture(target, service_id))
      .Times(1)
      .RetiresOnSaturation();
  if (!group_->feature_info()->gl_version_info().BehavesLikeGLES() &&
      group_->feature_info()->gl_version_info().IsAtLeastGL(3, 2)) {
    EXPECT_CALL(*gl_, TexParameteri(target, GL_DEPTH_TEXTURE_MODE, GL_RED))
        .Times(AtMost(1));
  }
  cmds::BindTexture cmd;
  cmd.Init(target, client_id);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

void RasterDecoderTestBase::DoDeleteTexture(GLuint client_id,
                                            GLuint service_id) {
  {
    InSequence s;

    // Calling DoDeleteTexture will unbind the texture from any texture units
    // it's currently bound to.
    EXPECT_CALL(*gl_, BindTexture(_, 0)).Times(AnyNumber());

    EXPECT_CALL(*gl_, DeleteTextures(1, Pointee(service_id)))
        .Times(1)
        .RetiresOnSaturation();

    GenHelper<cmds::DeleteTexturesImmediate>(client_id);
  }
}


// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/service/raster_decoder_unittest_0_autogen.h"

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLint RasterDecoderTestBase::kMaxTextureSize;
const GLint RasterDecoderTestBase::kNumTextureUnits;

const GLuint RasterDecoderTestBase::kServiceBufferId;
const GLuint RasterDecoderTestBase::kServiceTextureId;

const size_t RasterDecoderTestBase::kSharedBufferSize;
const uint32_t RasterDecoderTestBase::kSharedMemoryOffset;
const int32_t RasterDecoderTestBase::kInvalidSharedMemoryId;
const uint32_t RasterDecoderTestBase::kInvalidSharedMemoryOffset;
const uint32_t RasterDecoderTestBase::kInitialResult;
const uint8_t RasterDecoderTestBase::kInitialMemoryValue;

const uint32_t RasterDecoderTestBase::kNewClientId;
const uint32_t RasterDecoderTestBase::kNewServiceId;
const uint32_t RasterDecoderTestBase::kInvalidClientId;
#endif

}  // namespace raster
}  // namespace gpu
