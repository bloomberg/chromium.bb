// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/command_executor.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_stub_with_extensions.h"
#include "ui/gl/gl_image_ref_counted_memory.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_stub_api.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace gpu {
namespace {

const size_t kCommandBufferSize = 16384;
const size_t kTransferBufferSize = 16384;
const size_t kSmallTransferBufferSize = 16;
const size_t kTinyTransferBufferSize = 3;

static const char kExtensions[] =
    "GL_AMD_compressed_ATC_texture "
    "GL_ANGLE_texture_compression_dxt3 "
    "GL_ANGLE_texture_compression_dxt5 "
    "GL_ANGLE_texture_usage "
    "GL_APPLE_ycbcr_422 "
    "GL_ARB_texture_rectangle "
    "GL_EXT_blend_func_extended "
    "GL_EXT_color_buffer_float "
    "GL_EXT_disjoint_timer_query "
    "GL_EXT_draw_buffers "
    "GL_EXT_frag_depth "
    "GL_EXT_multisample_compatibility "
    "GL_EXT_multisampled_render_to_texture "
    "GL_EXT_occlusion_query_boolean "
    "GL_EXT_read_format_bgra "
    "GL_EXT_shader_texture_lod "
    "GL_EXT_texture_compression_s3tc "
    "GL_EXT_texture_filter_anisotropic "
    "GL_IMG_texture_compression_pvrtc "
    "GL_KHR_blend_equation_advanced "
    "GL_KHR_blend_equation_advanced_coherent "
    "GL_KHR_texture_compression_astc_ldr "
    "GL_NV_EGL_stream_consumer_external "
    "GL_NV_framebuffer_mixed_samples "
    "GL_NV_path_rendering "
    "GL_OES_compressed_ETC1_RGB8_texture "
    "GL_OES_depth24 "
    "GL_OES_EGL_image_external "
    "GL_OES_rgb8_rgba8 "
    "GL_OES_texture_float "
    "GL_OES_texture_float_linear "
    "GL_OES_texture_half_float "
    "GL_OES_texture_half_float_linear";

class CommandBufferSetup {
 public:
  CommandBufferSetup()
      : atexit_manager_(),
        sync_point_manager_(new SyncPointManager(false)),
        sync_point_order_data_(SyncPointOrderData::Create()),
        mailbox_manager_(new gles2::MailboxManagerImpl),
        share_group_(new gl::GLShareGroup),
        surface_(new gl::GLSurfaceStub),
        context_(new gl::GLContextStub(share_group_.get())),
        command_buffer_id_(CommandBufferId::FromUnsafeValue(1)) {
    logging::SetMinLogLevel(logging::LOG_FATAL);
    base::CommandLine::Init(0, NULL);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableUnsafeES3APIs);
    gpu_preferences_.enable_unsafe_es3_apis = true;

    gl::GLSurfaceTestSupport::InitializeOneOffWithMockBindings();

    api_.set_version("OpenGL ES 3.1");
    api_.set_extensions(kExtensions);
    gl::SetStubGLApi(&api_);

    sync_point_client_ = sync_point_manager_->CreateSyncPointClient(
        sync_point_order_data_, CommandBufferNamespace::IN_PROCESS,
        command_buffer_id_);

    translator_cache_ = new gles2::ShaderTranslatorCache(gpu_preferences_);
    completeness_cache_ = new gles2::FramebufferCompletenessCache;
  }

  void InitDecoder() {
    context_->MakeCurrent(surface_.get());
    scoped_refptr<gles2::FeatureInfo> feature_info =
        new gles2::FeatureInfo();
    scoped_refptr<gles2::ContextGroup> context_group = new gles2::ContextGroup(
        gpu_preferences_, mailbox_manager_.get(), nullptr, translator_cache_,
        completeness_cache_, feature_info, true /* bind_generates_resource */,
        nullptr /* image_factory */, nullptr /* progress_reporter */);
    command_buffer_.reset(
        new CommandBufferService(context_group->transfer_buffer_manager()));
    command_buffer_->SetPutOffsetChangeCallback(
        base::Bind(&CommandBufferSetup::PumpCommands, base::Unretained(this)));
    command_buffer_->SetGetBufferChangeCallback(base::Bind(
        &CommandBufferSetup::GetBufferChanged, base::Unretained(this)));
    InitializeInitialCommandBuffer();

    decoder_.reset(gles2::GLES2Decoder::Create(context_group.get()));
    executor_.reset(new CommandExecutor(command_buffer_.get(), decoder_.get(),
                                        decoder_.get()));
    decoder_->set_engine(executor_.get());
    decoder_->SetFenceSyncReleaseCallback(base::Bind(
        &CommandBufferSetup::OnFenceSyncRelease, base::Unretained(this)));
    decoder_->SetWaitFenceSyncCallback(base::Bind(
        &CommandBufferSetup::OnWaitFenceSync, base::Unretained(this)));
    decoder_->GetLogger()->set_log_synthesized_gl_errors(false);

    gles2::ContextCreationAttribHelper attrib_helper;
    attrib_helper.offscreen_framebuffer_size = gfx::Size(16, 16);
    attrib_helper.red_size = 8;
    attrib_helper.green_size = 8;
    attrib_helper.blue_size = 8;
    attrib_helper.alpha_size = 8;
    attrib_helper.depth_size = 0;
    attrib_helper.stencil_size = 0;
    attrib_helper.context_type = gles2::CONTEXT_TYPE_OPENGLES3;

    bool result =
        decoder_->Initialize(surface_.get(), context_.get(), true,
                             gles2::DisallowedFeatures(), attrib_helper);
    CHECK(result);
    decoder_->set_max_bucket_size(8 << 20);
    context_group->buffer_manager()->set_max_buffer_size(8 << 20);
    if (!vertex_translator_) {
      // Keep a reference to the translators, which keeps them in the cache even
      // after the decoder is reset. They are expensive to initialize, but they
      // don't keep state.
      vertex_translator_ = decoder_->GetTranslator(GL_VERTEX_SHADER);
      fragment_translator_ = decoder_->GetTranslator(GL_FRAGMENT_SHADER);
    }
  }

  void ResetDecoder() {
    decoder_->Destroy(true);
    decoder_.reset();
    command_buffer_.reset();
  }

  ~CommandBufferSetup() {
    sync_point_client_ = nullptr;
    if (sync_point_order_data_) {
      sync_point_order_data_->Destroy();
      sync_point_order_data_ = nullptr;
    }
  }

  void RunCommandBuffer(const uint8_t* data, size_t size) {
    // The commands are flushed at a uint32_t granularity. If the data is not
    // a full command, we zero-pad it.
    size_t padded_size = (size + 3) & ~3;
    // crbug.com/638836 The -max_len argument is sometimes not respected, so the
    // fuzzer may give us too much data. Bail ASAP in that case.
    if (padded_size > kCommandBufferSize)
      return;

    InitDecoder();
    size_t buffer_size = buffer_->size();
    CHECK_LE(padded_size, buffer_size);
    command_buffer_->SetGetBuffer(buffer_id_);
    auto* memory = static_cast<char*>(buffer_->memory());
    memcpy(memory, data, size);
    if (size < buffer_size)
      memset(memory + size, 0, buffer_size - size);
    command_buffer_->Flush(padded_size / 4);
    ResetDecoder();
  }

 private:
  void PumpCommands() {
    if (!decoder_->MakeCurrent()) {
      command_buffer_->SetContextLostReason(decoder_->GetContextLostReason());
      command_buffer_->SetParseError(::gpu::error::kLostContext);
      return;
    }

    uint32_t order_num = sync_point_order_data_->GenerateUnprocessedOrderNumber(
        sync_point_manager_.get());
    sync_point_order_data_->BeginProcessingOrderNumber(order_num);

    executor_->PutChanged();

    sync_point_order_data_->FinishProcessingOrderNumber(order_num);
  }

  bool GetBufferChanged(int32_t transfer_buffer_id) {
    return executor_->SetGetBuffer(transfer_buffer_id);
  }

  void OnFenceSyncRelease(uint64_t release) {
    CHECK(sync_point_client_);
    sync_point_client_->ReleaseFenceSync(release);
  }

  bool OnWaitFenceSync(CommandBufferNamespace namespace_id,
                       CommandBufferId command_buffer_id,
                       uint64_t release) {
    CHECK(sync_point_client_);
    scoped_refptr<gpu::SyncPointClientState> release_state =
        sync_point_manager_->GetSyncPointClientState(namespace_id,
                                                     command_buffer_id);
    if (!release_state)
      return true;

    if (release_state->IsFenceSyncReleased(release))
      return true;

    executor_->SetScheduled(false);
    return false;
  }

  void CreateTransferBuffer(size_t size, int32_t id) {
    scoped_refptr<Buffer> buffer =
        command_buffer_->CreateTransferBufferWithId(size, id);
    memset(buffer->memory(), 0, size);
  }

  void InitializeInitialCommandBuffer() {
    buffer_id_ = 1;
    buffer_ = command_buffer_->CreateTransferBufferWithId(kCommandBufferSize,
                                                          buffer_id_);
    CHECK(buffer_);
    // Create some transfer buffers. This is somewhat arbitrary, but having a
    // reasonably sized buffer in slot 4 allows us to prime the corpus with data
    // extracted from unit tests.
    CreateTransferBuffer(kTransferBufferSize, 2);
    CreateTransferBuffer(kSmallTransferBufferSize, 3);
    CreateTransferBuffer(kTransferBufferSize, 4);
    CreateTransferBuffer(kTinyTransferBufferSize, 5);
  }

  base::AtExitManager atexit_manager_;

  gl::GLStubApi api_;
  GpuPreferences gpu_preferences_;

  std::unique_ptr<SyncPointManager> sync_point_manager_;
  scoped_refptr<SyncPointOrderData> sync_point_order_data_;
  std::unique_ptr<SyncPointClient> sync_point_client_;
  scoped_refptr<gles2::MailboxManager> mailbox_manager_;
  scoped_refptr<gl::GLShareGroup> share_group_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  const gpu::CommandBufferId command_buffer_id_;

  scoped_refptr<gles2::ShaderTranslatorCache> translator_cache_;
  scoped_refptr<gles2::FramebufferCompletenessCache> completeness_cache_;

  std::unique_ptr<CommandBufferService> command_buffer_;

  std::unique_ptr<gles2::GLES2Decoder> decoder_;
  std::unique_ptr<CommandExecutor> executor_;

  scoped_refptr<gles2::ShaderTranslatorInterface> vertex_translator_;
  scoped_refptr<gles2::ShaderTranslatorInterface> fragment_translator_;

  scoped_refptr<Buffer> buffer_;
  int32_t buffer_id_ = 0;
};

}  // anonymous namespace
}  // namespace gpu


static gpu::CommandBufferSetup& GetSetup() {
  static gpu::CommandBufferSetup setup;
  return setup;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  GetSetup().RunCommandBuffer(data, size);
  return 0;
}
