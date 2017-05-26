// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/tests/gl_manager.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/command_buffer_direct.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_ref_counted_memory.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

#if defined(OS_MACOSX)
#include "ui/gfx/mac/io_surface.h"
#include "ui/gl/gl_image_io_surface.h"
#endif

namespace gpu {
namespace {

void InitializeGpuPreferencesForTestingFromCommandLine(
    const base::CommandLine& command_line,
    GpuPreferences* preferences) {
  // Only initialize specific GpuPreferences members used for testing.
  preferences->use_passthrough_cmd_decoder =
      command_line.HasSwitch(switches::kUsePassthroughCmdDecoder);
}

class GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  GpuMemoryBufferImpl(base::RefCountedBytes* bytes,
                      const gfx::Size& size,
                      gfx::BufferFormat format)
      : mapped_(false), bytes_(bytes), size_(size), format_(format) {}

  static GpuMemoryBufferImpl* FromClientBuffer(ClientBuffer buffer) {
    return reinterpret_cast<GpuMemoryBufferImpl*>(buffer);
  }

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override {
    DCHECK(!mapped_);
    mapped_ = true;
    return true;
  }
  void* memory(size_t plane) override {
    DCHECK(mapped_);
    DCHECK_LT(plane, gfx::NumberOfPlanesForBufferFormat(format_));
    return reinterpret_cast<uint8_t*>(&bytes_->data().front()) +
           gfx::BufferOffsetForBufferFormat(size_, format_, plane);
  }
  void Unmap() override {
    DCHECK(mapped_);
    mapped_ = false;
  }
  gfx::Size GetSize() const override { return size_; }
  gfx::BufferFormat GetFormat() const override { return format_; }
  int stride(size_t plane) const override {
    DCHECK_LT(plane, gfx::NumberOfPlanesForBufferFormat(format_));
    return gfx::RowSizeForBufferFormat(size_.width(), format_, plane);
  }
  gfx::GpuMemoryBufferId GetId() const override {
    NOTREACHED();
    return gfx::GpuMemoryBufferId(0);
  }
  gfx::GpuMemoryBufferHandle GetHandle() const override {
    NOTREACHED();
    return gfx::GpuMemoryBufferHandle();
  }
  ClientBuffer AsClientBuffer() override {
    return reinterpret_cast<ClientBuffer>(this);
  }

  base::RefCountedBytes* bytes() { return bytes_.get(); }

 private:
  bool mapped_;
  scoped_refptr<base::RefCountedBytes> bytes_;
  const gfx::Size size_;
  gfx::BufferFormat format_;
};

#if defined(OS_MACOSX)
class IOSurfaceGpuMemoryBuffer : public gfx::GpuMemoryBuffer {
 public:
  IOSurfaceGpuMemoryBuffer(const gfx::Size& size, gfx::BufferFormat format)
      : mapped_(false), size_(size), format_(format) {
    iosurface_ = gfx::CreateIOSurface(size, gfx::BufferFormat::BGRA_8888);
  }

  ~IOSurfaceGpuMemoryBuffer() override {
    CFRelease(iosurface_);
  }

  static IOSurfaceGpuMemoryBuffer* FromClientBuffer(ClientBuffer buffer) {
    return reinterpret_cast<IOSurfaceGpuMemoryBuffer*>(buffer);
  }

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map() override {
    DCHECK(!mapped_);
    mapped_ = true;
    return true;
  }
  void* memory(size_t plane) override {
    DCHECK(mapped_);
    DCHECK_LT(plane, gfx::NumberOfPlanesForBufferFormat(format_));
    return IOSurfaceGetBaseAddressOfPlane(iosurface_, plane);
  }
  void Unmap() override {
    DCHECK(mapped_);
    mapped_ = false;
  }
  gfx::Size GetSize() const override { return size_; }
  gfx::BufferFormat GetFormat() const override { return format_; }
  int stride(size_t plane) const override {
    DCHECK_LT(plane, gfx::NumberOfPlanesForBufferFormat(format_));
    return IOSurfaceGetWidthOfPlane(iosurface_, plane);
  }
  gfx::GpuMemoryBufferId GetId() const override {
    NOTREACHED();
    return gfx::GpuMemoryBufferId(0);
  }
  gfx::GpuMemoryBufferHandle GetHandle() const override {
    NOTREACHED();
    return gfx::GpuMemoryBufferHandle();
  }
  ClientBuffer AsClientBuffer() override {
    return reinterpret_cast<ClientBuffer>(this);
  }

  IOSurfaceRef iosurface() { return iosurface_; }

 private:
  bool mapped_;
  IOSurfaceRef iosurface_;
  const gfx::Size size_;
  gfx::BufferFormat format_;
};
#endif  // defined(OS_MACOSX)

class CommandBufferCheckLostContext : public CommandBufferDirect {
 public:
  CommandBufferCheckLostContext(TransferBufferManager* transfer_buffer_manager,
                                AsyncAPIInterface* handler,
                                const MakeCurrentCallback& callback,
                                SyncPointManager* sync_point_manager,
                                bool context_lost_allowed)
      : CommandBufferDirect(transfer_buffer_manager,
                            handler,
                            callback,
                            sync_point_manager),
        context_lost_allowed_(context_lost_allowed) {}

  ~CommandBufferCheckLostContext() override {}

  void Flush(int32_t put_offset) override {
    CommandBufferDirect::Flush(put_offset);

    ::gpu::CommandBuffer::State state = GetLastState();
    if (!context_lost_allowed_) {
      ASSERT_EQ(::gpu::error::kNoError, state.error);
    }
  }

 private:
  bool context_lost_allowed_;
};

}  // namespace

int GLManager::use_count_;
scoped_refptr<gl::GLShareGroup>* GLManager::base_share_group_;
scoped_refptr<gl::GLSurface>* GLManager::base_surface_;
scoped_refptr<gl::GLContext>* GLManager::base_context_;

GLManager::Options::Options() = default;

GLManager::GLManager() : discardable_manager_(new ServiceDiscardableManager()) {
  SetupBaseContext();
}

GLManager::~GLManager() {
  --use_count_;
  if (!use_count_) {
    if (base_share_group_) {
      delete base_context_;
      base_context_ = NULL;
    }
    if (base_surface_) {
      delete base_surface_;
      base_surface_ = NULL;
    }
    if (base_context_) {
      delete base_context_;
      base_context_ = NULL;
    }
  }
}

std::unique_ptr<gfx::GpuMemoryBuffer> GLManager::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format) {
#if defined(OS_MACOSX)
  if (use_iosurface_memory_buffers_) {
    return base::WrapUnique<gfx::GpuMemoryBuffer>(
        new IOSurfaceGpuMemoryBuffer(size, format));
  }
#endif  // defined(OS_MACOSX)
  std::vector<uint8_t> data(gfx::BufferSizeForBufferFormat(size, format), 0);
  scoped_refptr<base::RefCountedBytes> bytes(new base::RefCountedBytes(data));
  return base::WrapUnique<gfx::GpuMemoryBuffer>(
      new GpuMemoryBufferImpl(bytes.get(), size, format));
}

void GLManager::Initialize(const GLManager::Options& options) {
  InitializeWithCommandLine(options, *base::CommandLine::ForCurrentProcess());
}

void GLManager::InitializeWithCommandLine(
    const GLManager::Options& options,
    const base::CommandLine& command_line) {
  const int32_t kCommandBufferSize = 1024 * 1024;
  const size_t kStartTransferBufferSize = 4 * 1024 * 1024;
  const size_t kMinTransferBufferSize = 1 * 256 * 1024;
  const size_t kMaxTransferBufferSize = 16 * 1024 * 1024;

  InitializeGpuPreferencesForTestingFromCommandLine(command_line,
                                                    &gpu_preferences_);

  gles2::MailboxManager* mailbox_manager = NULL;
  if (options.share_mailbox_manager) {
    mailbox_manager = options.share_mailbox_manager->mailbox_manager();
  } else if (options.share_group_manager) {
    mailbox_manager = options.share_group_manager->mailbox_manager();
  }

  gl::GLShareGroup* share_group = NULL;
  if (options.share_group_manager) {
    share_group = options.share_group_manager->share_group();
  } else if (options.share_mailbox_manager) {
    share_group = options.share_mailbox_manager->share_group();
  }

  gles2::ContextGroup* context_group = NULL;
  scoped_refptr<gles2::ShareGroup> client_share_group;
  if (options.share_group_manager) {
    context_group = options.share_group_manager->decoder_->GetContextGroup();
    client_share_group =
      options.share_group_manager->gles2_implementation()->share_group();
  }

  gl::GLContext* real_gl_context = NULL;
  if (options.virtual_manager &&
      !gpu_preferences_.use_passthrough_cmd_decoder) {
    real_gl_context = options.virtual_manager->context();
  }

  mailbox_manager_ =
      mailbox_manager ? mailbox_manager : new gles2::MailboxManagerImpl;
  share_group_ = share_group ? share_group : new gl::GLShareGroup;

  gles2::ContextCreationAttribHelper attribs;
  attribs.red_size = 8;
  attribs.green_size = 8;
  attribs.blue_size = 8;
  attribs.alpha_size = 8;
  attribs.depth_size = 16;
  attribs.stencil_size = 8;
  attribs.context_type = options.context_type;
  attribs.samples = options.multisampled ? 4 : 0;
  attribs.sample_buffers = options.multisampled ? 1 : 0;
  attribs.alpha_size = options.backbuffer_alpha ? 8 : 0;
  attribs.should_use_native_gmb_for_backbuffer =
      options.image_factory != nullptr;
  attribs.offscreen_framebuffer_size = options.size;
  attribs.buffer_preserved = options.preserve_backbuffer;
  attribs.bind_generates_resource = options.bind_generates_resource;

  if (!context_group) {
    GpuDriverBugWorkarounds gpu_driver_bug_workaround(&command_line);
    scoped_refptr<gles2::FeatureInfo> feature_info =
        new gles2::FeatureInfo(command_line, gpu_driver_bug_workaround);
    context_group = new gles2::ContextGroup(
        gpu_preferences_, mailbox_manager_.get(), nullptr,
        new gpu::gles2::ShaderTranslatorCache(gpu_preferences_),
        new gpu::gles2::FramebufferCompletenessCache, feature_info,
        options.bind_generates_resource, options.image_factory, nullptr,
        GpuFeatureInfo(), discardable_manager_.get());
  }

  decoder_.reset(::gpu::gles2::GLES2Decoder::Create(context_group));
  if (options.force_shader_name_hashing) {
    decoder_->SetForceShaderNameHashingForTest(true);
  }
  command_buffer_.reset(new CommandBufferCheckLostContext(
      decoder_->GetContextGroup()->transfer_buffer_manager(), decoder_.get(),
      base::Bind(&gles2::GLES2Decoder::MakeCurrent,
                 base::Unretained(decoder_.get())),
      options.sync_point_manager, options.context_lost_allowed));

  decoder_->set_command_buffer_service(command_buffer_->service());

  surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
  ASSERT_TRUE(surface_.get() != NULL) << "could not create offscreen surface";

  if (base_context_) {
    context_ = scoped_refptr<gl::GLContext>(new gpu::GLContextVirtual(
        share_group_.get(), base_context_->get(), decoder_->AsWeakPtr()));
    ASSERT_TRUE(context_->Initialize(
        surface_.get(),
        GenerateGLContextAttribs(attribs, context_group->gpu_preferences())));
  } else {
    if (real_gl_context) {
      context_ = scoped_refptr<gl::GLContext>(new gpu::GLContextVirtual(
          share_group_.get(), real_gl_context, decoder_->AsWeakPtr()));
      ASSERT_TRUE(context_->Initialize(
          surface_.get(),
          GenerateGLContextAttribs(attribs, context_group->gpu_preferences())));
    } else {
      context_ = gl::init::CreateGLContext(
          share_group_.get(), surface_.get(),
          GenerateGLContextAttribs(attribs, context_group->gpu_preferences()));
    }
  }
  ASSERT_TRUE(context_.get() != NULL) << "could not create GL context";

  ASSERT_TRUE(context_->MakeCurrent(surface_.get()));

  if (!decoder_->Initialize(surface_.get(), context_.get(), true,
                            ::gpu::gles2::DisallowedFeatures(), attribs)) {
    return;
  }

  if (options.sync_point_manager) {
    decoder_->SetFenceSyncReleaseCallback(
        base::Bind(&CommandBufferDirect::OnFenceSyncRelease,
                   base::Unretained(command_buffer_.get())));
    decoder_->SetWaitSyncTokenCallback(
        base::Bind(&CommandBufferDirect::OnWaitSyncToken,
                   base::Unretained(command_buffer_.get())));
  }

  // Create the GLES2 helper, which writes the command buffer protocol.
  gles2_helper_.reset(new gles2::GLES2CmdHelper(command_buffer_.get()));
  ASSERT_TRUE(gles2_helper_->Initialize(kCommandBufferSize));

  // Create a transfer buffer.
  transfer_buffer_.reset(new TransferBuffer(gles2_helper_.get()));

  // Create the object exposing the OpenGL API.
  const bool support_client_side_arrays = true;
  gles2_implementation_.reset(new gles2::GLES2Implementation(
      gles2_helper_.get(), std::move(client_share_group),
      transfer_buffer_.get(), options.bind_generates_resource,
      options.lose_context_when_out_of_memory, support_client_side_arrays,
      this));

  ASSERT_TRUE(gles2_implementation_->Initialize(
      kStartTransferBufferSize, kMinTransferBufferSize, kMaxTransferBufferSize,
      SharedMemoryLimits::kNoLimit))
      << "Could not init GLES2Implementation";

  MakeCurrent();
}

void GLManager::SetupBaseContext() {
  if (use_count_) {
    #if defined(OS_ANDROID)
    base_share_group_ =
        new scoped_refptr<gl::GLShareGroup>(new gl::GLShareGroup);
    gfx::Size size(4, 4);
    base_surface_ = new scoped_refptr<gl::GLSurface>(
        gl::init::CreateOffscreenGLSurface(size));
    base_context_ = new scoped_refptr<gl::GLContext>(gl::init::CreateGLContext(
        base_share_group_->get(), base_surface_->get(),
        gl::GLContextAttribs()));
    #endif
  }
  ++use_count_;
}

void GLManager::MakeCurrent() {
  ::gles2::SetGLContext(gles2_implementation_.get());
}

void GLManager::SetSurface(gl::GLSurface* surface) {
  decoder_->SetSurface(surface);
}

void GLManager::PerformIdleWork() {
  decoder_->PerformIdleWork();
}

void GLManager::SetCommandsPaused(bool paused) {
  command_buffer_->SetCommandsPaused(paused);
}

void GLManager::Destroy() {
  if (gles2_implementation_.get()) {
    MakeCurrent();
    EXPECT_TRUE(glGetError() == GL_NONE);
    gles2_implementation_->Flush();
    gles2_implementation_.reset();
  }
  transfer_buffer_.reset();
  gles2_helper_.reset();
  command_buffer_.reset();
  if (decoder_.get()) {
    bool have_context = decoder_->GetGLContext() &&
                        decoder_->GetGLContext()->MakeCurrent(surface_.get());
    decoder_->Destroy(have_context);
    decoder_.reset();
  }
}

const GpuDriverBugWorkarounds& GLManager::workarounds() const {
  return decoder_->GetContextGroup()->feature_info()->workarounds();
}

void GLManager::SetGpuControlClient(GpuControlClient*) {
  // The client is not currently called, so don't store it.
}

Capabilities GLManager::GetCapabilities() {
  return decoder_->GetCapabilities();
}

int32_t GLManager::CreateImage(ClientBuffer buffer,
                               size_t width,
                               size_t height,
                               unsigned internalformat) {
  gfx::Size size(width, height);
  scoped_refptr<gl::GLImage> gl_image;

#if defined(OS_MACOSX)
  if (use_iosurface_memory_buffers_) {
    IOSurfaceGpuMemoryBuffer* gpu_memory_buffer =
        IOSurfaceGpuMemoryBuffer::FromClientBuffer(buffer);
    scoped_refptr<gl::GLImageIOSurface> image(
        new gl::GLImageIOSurface(size, internalformat));
    if (!image->Initialize(gpu_memory_buffer->iosurface(),
                           gfx::GenericSharedMemoryId(1),
                           gfx::BufferFormat::BGRA_8888)) {
      return -1;
    }
    gl_image = image;
  }
#endif  // defined(OS_MACOSX)
  if (!gl_image) {
    GpuMemoryBufferImpl* gpu_memory_buffer =
        GpuMemoryBufferImpl::FromClientBuffer(buffer);

    scoped_refptr<gl::GLImageRefCountedMemory> image(
        new gl::GLImageRefCountedMemory(size, internalformat));
    if (!image->Initialize(gpu_memory_buffer->bytes(),
                           gpu_memory_buffer->GetFormat())) {
      return -1;
    }
    gl_image = image;
  }

  static int32_t next_id = 1;
  int32_t new_id = next_id++;
  gpu::gles2::ImageManager* image_manager = decoder_->GetImageManager();
  DCHECK(image_manager);
  image_manager->AddImage(gl_image.get(), new_id);
  return new_id;
}

void GLManager::DestroyImage(int32_t id) {
  gpu::gles2::ImageManager* image_manager = decoder_->GetImageManager();
  DCHECK(image_manager);
  image_manager->RemoveImage(id);
}

void GLManager::SignalQuery(uint32_t query, const base::Closure& callback) {
  NOTIMPLEMENTED();
}

void GLManager::SetLock(base::Lock*) {
  NOTIMPLEMENTED();
}

void GLManager::EnsureWorkVisible() {
  // This is only relevant for out-of-process command buffers.
}

gpu::CommandBufferNamespace GLManager::GetNamespaceID() const {
  return command_buffer_->GetNamespaceID();
}

CommandBufferId GLManager::GetCommandBufferID() const {
  return command_buffer_->GetCommandBufferID();
}

int32_t GLManager::GetStreamId() const {
  return 0;
}

void GLManager::FlushOrderingBarrierOnStream(int32_t stream_id) {
  // This is only relevant for out-of-process command buffers.
}

uint64_t GLManager::GenerateFenceSyncRelease() {
  return next_fence_sync_release_++;
}

bool GLManager::IsFenceSyncRelease(uint64_t release) {
  return release > 0 && release < next_fence_sync_release_;
}

bool GLManager::IsFenceSyncFlushed(uint64_t release) {
  return IsFenceSyncRelease(release);
}

bool GLManager::IsFenceSyncFlushReceived(uint64_t release) {
  return IsFenceSyncRelease(release);
}

bool GLManager::IsFenceSyncReleased(uint64_t release) {
  return release <= command_buffer_->GetLastState().release_count;
}

void GLManager::SignalSyncToken(const gpu::SyncToken& sync_token,
                                const base::Closure& callback) {
  command_buffer_->SignalSyncToken(sync_token, callback);
}

void GLManager::WaitSyncTokenHint(const gpu::SyncToken& sync_token) {}

bool GLManager::CanWaitUnverifiedSyncToken(const gpu::SyncToken& sync_token) {
  return false;
}

void GLManager::AddLatencyInfo(
    const std::vector<ui::LatencyInfo>& latency_info) {}

}  // namespace gpu
