// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/raster_decoder.h"

#include <limits>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/raster_cmd_format.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/raster_decoder_context_state.h"
#include "gpu/command_buffer/service/raster_decoder_unittest_base.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_image_stub.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;

using namespace gpu::raster::cmds;

namespace gpu {
namespace raster {

class RasterDecoderTest : public RasterDecoderTestBase {
 public:
  RasterDecoderTest() = default;
};

INSTANTIATE_TEST_CASE_P(Service, RasterDecoderTest, ::testing::Bool());
INSTANTIATE_TEST_CASE_P(Service,
                        RasterDecoderManualInitTest,
                        ::testing::Bool());

const GLsync kGlSync = reinterpret_cast<GLsync>(0xdeadbeef);

TEST_P(RasterDecoderTest, BeginEndQueryEXTCommandsCompletedCHROMIUM) {
  GenHelper<GenQueriesEXTImmediate>(kNewClientId);

  BeginQueryEXT begin_cmd;
  begin_cmd.Init(GL_COMMANDS_COMPLETED_CHROMIUM, kNewClientId,
                 shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  QueryManager* query_manager = decoder_->GetQueryManager();
  ASSERT_TRUE(query_manager != nullptr);
  QueryManager::Query* query = query_manager->GetQuery(kNewClientId);
  ASSERT_TRUE(query != nullptr);
  EXPECT_FALSE(query->IsPending());

  EXPECT_CALL(*gl_, Flush()).RetiresOnSaturation();
  EXPECT_CALL(*gl_, FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0))
      .WillOnce(Return(kGlSync))
      .RetiresOnSaturation();
#if DCHECK_IS_ON()
  EXPECT_CALL(*gl_, IsSync(kGlSync))
      .WillOnce(Return(GL_TRUE))
      .RetiresOnSaturation();
#endif

  EndQueryEXT end_cmd;
  end_cmd.Init(GL_COMMANDS_COMPLETED_CHROMIUM, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(end_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(query->IsPending());

#if DCHECK_IS_ON()
  EXPECT_CALL(*gl_, IsSync(kGlSync))
      .WillOnce(Return(GL_TRUE))
      .RetiresOnSaturation();
#endif
  EXPECT_CALL(*gl_, ClientWaitSync(kGlSync, _, _))
      .WillOnce(Return(GL_TIMEOUT_EXPIRED))
      .RetiresOnSaturation();
  query_manager->ProcessPendingQueries(false);

  EXPECT_TRUE(query->IsPending());

#if DCHECK_IS_ON()
  EXPECT_CALL(*gl_, IsSync(kGlSync))
      .WillOnce(Return(GL_TRUE))
      .RetiresOnSaturation();
#endif
  EXPECT_CALL(*gl_, ClientWaitSync(kGlSync, _, _))
      .WillOnce(Return(GL_ALREADY_SIGNALED))
      .RetiresOnSaturation();
  query_manager->ProcessPendingQueries(false);

  EXPECT_FALSE(query->IsPending());

#if DCHECK_IS_ON()
  EXPECT_CALL(*gl_, IsSync(kGlSync))
      .WillOnce(Return(GL_TRUE))
      .RetiresOnSaturation();
#endif
  EXPECT_CALL(*gl_, DeleteSync(kGlSync)).Times(1).RetiresOnSaturation();
  ResetDecoder();
}

TEST_P(RasterDecoderTest, BeginEndQueryEXTCommandsIssuedCHROMIUM) {
  BeginQueryEXT begin_cmd;

  GenHelper<GenQueriesEXTImmediate>(kNewClientId);

  // Test valid parameters work.
  begin_cmd.Init(GL_COMMANDS_ISSUED_CHROMIUM, kNewClientId, shared_memory_id_,
                 kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  QueryManager* query_manager = decoder_->GetQueryManager();
  ASSERT_TRUE(query_manager != nullptr);
  QueryManager::Query* query = query_manager->GetQuery(kNewClientId);
  ASSERT_TRUE(query != nullptr);
  EXPECT_FALSE(query->IsPending());

  // Test end succeeds
  EndQueryEXT end_cmd;
  end_cmd.Init(GL_COMMANDS_ISSUED_CHROMIUM, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(end_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_FALSE(query->IsPending());
}

TEST_P(RasterDecoderTest, CopyTexSubImage2DTwiceClearsUnclearedTexture) {
  // Create uninitialized source texture.
  GLuint source_texture_id = kNewClientId;
  CreateFakeTexture(source_texture_id, kNewServiceId,
                    viz::ResourceFormat::RGBA_8888, /*width=*/2, /*height=*/2,
                    /*cleared=*/false);

  // This will initialize the top half of destination.
  {
    // Source is undefined, so first call to CopySubTexture will clear the
    // source.
    SetupClearTextureExpectations(kNewServiceId, kServiceTextureId,
                                  GL_TEXTURE_2D, GL_TEXTURE_2D, 0, GL_RGBA,
                                  GL_UNSIGNED_BYTE, 0, 0, 2, 2, 0);
    SetScopedTextureBinderExpectations(GL_TEXTURE_2D);
    CopySubTexture cmd;
    cmd.Init(source_texture_id, client_texture_id_, 0, 0, 0, 0, 2, 1);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  // This will initialize bottom right corner of the destination.
  // CopySubTexture will clear the bottom half of the destination because a
  // single rectangle is insufficient to keep track of the initialized area.
  {
    SetupClearTextureExpectations(kServiceTextureId, kServiceTextureId,
                                  GL_TEXTURE_2D, GL_TEXTURE_2D, 0, GL_RGBA,
                                  GL_UNSIGNED_BYTE, 0, 1, 2, 1, 0);
    SetScopedTextureBinderExpectations(GL_TEXTURE_2D);
    CopySubTexture cmd;
    cmd.Init(source_texture_id, client_texture_id_, 1, 1, 0, 0, 1, 1);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  gles2::TextureManager* manager = group().texture_manager();
  gles2::TextureRef* texture_ref = manager->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  gles2::Texture* texture = texture_ref->texture();
  EXPECT_TRUE(texture->SafeToRenderFrom());
}

TEST_P(RasterDecoderManualInitTest, CopyTexSubImage2DValidateColorFormat) {
  InitState init;
  init.gl_version = "3.0";
  init.extensions.push_back("GL_EXT_texture_rg");
  InitDecoder(init);

  // Create dest texture.
  GLuint dest_texture_id = kNewClientId;
  CreateFakeTexture(dest_texture_id, kNewServiceId, viz::ResourceFormat::RED_8,
                    /*width=*/2, /*height=*/2, /*cleared=*/true);

  SetScopedTextureBinderExpectations(GL_TEXTURE_2D);
  CopySubTexture copy_cmd;
  copy_cmd.Init(client_texture_id_, dest_texture_id, 0, 0, 0, 0, 2, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(copy_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(RasterDecoderTest, YieldAfterEndRasterCHROMIUM) {
#if defined(OS_MACOSX)
  EXPECT_CALL(*gl_, Flush()).RetiresOnSaturation();
#endif
  GetDecoder()->SetUpForRasterCHROMIUMForTest();
  cmds::EndRasterCHROMIUM end_raster_cmd;
  end_raster_cmd.Init();
  EXPECT_EQ(error::kDeferLaterCommands, ExecuteCmd(end_raster_cmd));
}

class RasterDecoderOOPTest : public testing::Test, DecoderClient {
 public:
  RasterDecoderOOPTest() : shader_translator_cache_(gpu_preferences_) {}

  void SetUp() override {
    gl::GLSurfaceTestSupport::InitializeOneOff();
    gpu::GpuDriverBugWorkarounds workarounds;

    scoped_refptr<gl::GLShareGroup> share_group = new gl::GLShareGroup();
    scoped_refptr<gl::GLSurface> surface =
        gl::init::CreateOffscreenGLSurface(gfx::Size());
    scoped_refptr<gl::GLContext> context = gl::init::CreateGLContext(
        share_group.get(), surface.get(), gl::GLContextAttribs());
    ASSERT_TRUE(context->MakeCurrent(surface.get()));

    context_state_ = new raster::RasterDecoderContextState(
        std::move(share_group), std::move(surface), std::move(context),
        false /* use_virtualized_gl_contexts */);
    context_state_->InitializeGrContext(workarounds, nullptr);

    GpuFeatureInfo gpu_feature_info;
    gpu_feature_info.status_values[GPU_FEATURE_TYPE_OOP_RASTERIZATION] =
        kGpuFeatureStatusEnabled;
    scoped_refptr<gles2::FeatureInfo> feature_info =
        new gles2::FeatureInfo(workarounds, gpu_feature_info);
    group_ = new gles2::ContextGroup(
        gpu_preferences_, false, &mailbox_manager_,
        nullptr /* memory_tracker */, &shader_translator_cache_,
        &framebuffer_completeness_cache_, feature_info,
        false /* bind_generates_resource */, &image_manager_,
        nullptr /* image_factory */, nullptr /* progress_reporter */,
        gpu_feature_info, &discardable_manager_,
        nullptr /* passthrough_discardable_manager */, &shared_image_manager_);
  }
  void TearDown() override {
    context_state_ = nullptr;
    gl::init::ShutdownGL(false);
  }

  // DecoderClient implementation.
  void OnConsoleMessage(int32_t id, const std::string& message) override {}
  void CacheShader(const std::string& key, const std::string& shader) override {
  }
  void OnFenceSyncRelease(uint64_t release) override {}
  bool OnWaitSyncToken(const gpu::SyncToken&) override { return false; }
  void OnDescheduleUntilFinished() override {}
  void OnRescheduleAfterFinished() override {}
  void OnSwapBuffers(uint64_t swap_id, uint32_t flags) override {}
  void ScheduleGrContextCleanup() override {}

  std::unique_ptr<RasterDecoder> CreateDecoder() {
    auto decoder = base::WrapUnique(
        RasterDecoder::Create(this, &command_buffer_service_, &outputter_,
                              group_.get(), context_state_));
    ContextCreationAttribs attribs;
    attribs.enable_oop_rasterization = true;
    attribs.enable_raster_interface = true;
    CHECK_EQ(
        decoder->Initialize(context_state_->surface, context_state_->context,
                            true, gles2::DisallowedFeatures(), attribs),
        ContextResult::kSuccess);
    return decoder;
  }

  template <typename T>
  error::Error ExecuteCmd(RasterDecoder* decoder, const T& cmd) {
    static_assert(T::kArgFlags == cmd::kFixed,
                  "T::kArgFlags should equal cmd::kFixed");
    int entries_processed = 0;
    return decoder->DoCommands(1, (const void*)&cmd,
                               ComputeNumEntries(sizeof(cmd)),
                               &entries_processed);
  }

 protected:
  gles2::TraceOutputter outputter_;
  FakeCommandBufferServiceBase command_buffer_service_;
  scoped_refptr<RasterDecoderContextState> context_state_;

  GpuPreferences gpu_preferences_;
  gles2::MailboxManagerImpl mailbox_manager_;
  gles2::ShaderTranslatorCache shader_translator_cache_;
  gles2::FramebufferCompletenessCache framebuffer_completeness_cache_;
  gles2::ImageManager image_manager_;
  ServiceDiscardableManager discardable_manager_;
  SharedImageManager shared_image_manager_;
  scoped_refptr<gles2::ContextGroup> group_;
};

TEST_F(RasterDecoderOOPTest, StateRestoreAcrossDecoders) {
  // First decoder receives a skia command requiring context state reset.
  auto decoder1 = CreateDecoder();
  EXPECT_FALSE(context_state_->need_context_state_reset);
  decoder1->SetUpForRasterCHROMIUMForTest();
  cmds::EndRasterCHROMIUM end_raster_cmd;
  end_raster_cmd.Init();
  EXPECT_FALSE(error::IsError(ExecuteCmd(decoder1.get(), end_raster_cmd)));
  EXPECT_TRUE(context_state_->need_context_state_reset);

  // Another decoder receives a command which does not require consistent state,
  // it should be processed without state restoration.
  auto decoder2 = CreateDecoder();
  decoder2->SetUpForRasterCHROMIUMForTest();
  EXPECT_FALSE(error::IsError(ExecuteCmd(decoder2.get(), end_raster_cmd)));
  EXPECT_TRUE(context_state_->need_context_state_reset);

  // Now process a command which requires consistent state.
  LoseContextCHROMIUM lose_context_cmd;
  lose_context_cmd.Init(GL_GUILTY_CONTEXT_RESET_ARB,
                        GL_INNOCENT_CONTEXT_RESET_ARB);
  EXPECT_FALSE(error::IsError(ExecuteCmd(decoder2.get(), lose_context_cmd)));
  EXPECT_FALSE(context_state_->need_context_state_reset);

  decoder1->Destroy(true);
  context_state_->context->MakeCurrent(context_state_->surface.get());
  decoder2->Destroy(true);

  // Make sure the context is preserved across decoders.
  EXPECT_FALSE(context_state_->gr_context->abandoned());
}

}  // namespace raster
}  // namespace gpu
