// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/gles2_conform_support/egl/display.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>
#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/value_state.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/command_buffer/service/valuebuffer_manager.h"
#include "gpu/gles2_conform_support/egl/config.h"
#include "gpu/gles2_conform_support/egl/surface.h"
#include "gpu/gles2_conform_support/egl/test_support.h"

namespace {
const int32_t kCommandBufferSize = 1024 * 1024;
const int32_t kTransferBufferSize = 512 * 1024;
}

namespace egl {
#if defined(COMMAND_BUFFER_GLES_LIB_SUPPORT_ONLY)
// egl::Display is used for comformance tests and command_buffer_gles.  We only
// need the exit manager for the command_buffer_gles library.
// TODO(hendrikw): Find a cleaner solution for this.
namespace {
base::LazyInstance<base::Lock>::Leaky g_exit_manager_lock;
int g_exit_manager_use_count;
base::AtExitManager* g_exit_manager;
void RefAtExitManager() {
  base::AutoLock lock(g_exit_manager_lock.Get());
#if defined(COMPONENT_BUILD)
  if (g_command_buffer_gles_has_atexit_manager) {
    return;
  }
#endif
  if (g_exit_manager_use_count == 0) {
    g_exit_manager = new base::AtExitManager;
  }
  ++g_exit_manager_use_count;
}
void ReleaseAtExitManager() {
  base::AutoLock lock(g_exit_manager_lock.Get());
#if defined(COMPONENT_BUILD)
  if (g_command_buffer_gles_has_atexit_manager) {
    return;
  }
#endif
  --g_exit_manager_use_count;
  if (g_exit_manager_use_count == 0) {
    delete g_exit_manager;
    g_exit_manager = nullptr;
  }
}
}
#endif

Display::Display(EGLNativeDisplayType display_id)
    : display_id_(display_id),
      gpu_driver_bug_workarounds_(base::CommandLine::ForCurrentProcess()),
      is_initialized_(false),
      create_offscreen_(false),
      create_offscreen_width_(0),
      create_offscreen_height_(0),
      next_fence_sync_release_(1) {
#if defined(COMMAND_BUFFER_GLES_LIB_SUPPORT_ONLY)
  RefAtExitManager();
#endif
}

Display::~Display() {
  gles2::Terminate();
#if defined(COMMAND_BUFFER_GLES_LIB_SUPPORT_ONLY)
  ReleaseAtExitManager();
#endif
}

bool Display::Initialize() {
  gles2::Initialize();
  is_initialized_ = true;
  return true;
}

bool Display::IsValidConfig(EGLConfig config) {
  return (config != NULL) && (config == config_.get());
}

bool Display::ChooseConfigs(EGLConfig* configs,
                            EGLint config_size,
                            EGLint* num_config) {
  // TODO(alokp): Find out a way to find all configs. CommandBuffer currently
  // does not support finding or choosing configs.
  *num_config = 1;
  if (configs != NULL) {
    if (config_ == NULL) {
      config_.reset(new Config);
    }
    configs[0] = config_.get();
  }
  return true;
}

bool Display::GetConfigs(EGLConfig* configs,
                         EGLint config_size,
                         EGLint* num_config) {
  // TODO(alokp): Find out a way to find all configs. CommandBuffer currently
  // does not support finding or choosing configs.
  *num_config = 1;
  if (configs != NULL) {
    if (config_ == NULL) {
      config_.reset(new Config);
    }
    configs[0] = config_.get();
  }
  return true;
}

bool Display::GetConfigAttrib(EGLConfig config,
                              EGLint attribute,
                              EGLint* value) {
  const egl::Config* cfg = static_cast<egl::Config*>(config);
  return cfg->GetAttrib(attribute, value);
}

bool Display::IsValidNativeWindow(EGLNativeWindowType win) {
#if defined OS_WIN
  return ::IsWindow(win) != FALSE;
#else
  // TODO(alokp): Validate window handle.
  return true;
#endif  // OS_WIN
}

bool Display::IsValidSurface(EGLSurface surface) {
  return (surface != NULL) && (surface == surface_.get());
}

EGLSurface Display::CreateWindowSurface(EGLConfig config,
                                        EGLNativeWindowType win,
                                        const EGLint* attrib_list) {
  if (surface_ != NULL) {
    // We do not support more than one window surface.
    return EGL_NO_SURFACE;
  }

  {
    gpu::TransferBufferManager* manager =
        new gpu::TransferBufferManager(nullptr);
    transfer_buffer_manager_ = manager;
    manager->Initialize();
  }
  std::unique_ptr<gpu::CommandBufferService> command_buffer(
      new gpu::CommandBufferService(transfer_buffer_manager_.get()));
  if (!command_buffer->Initialize())
    return NULL;

  scoped_refptr<gpu::gles2::FeatureInfo> feature_info(
      new gpu::gles2::FeatureInfo(gpu_driver_bug_workarounds_));
  scoped_refptr<gpu::gles2::ContextGroup> group(new gpu::gles2::ContextGroup(
      gpu_preferences_, NULL, NULL,
      new gpu::gles2::ShaderTranslatorCache(gpu_preferences_),
      new gpu::gles2::FramebufferCompletenessCache, feature_info, NULL, NULL,
      true));

  decoder_.reset(gpu::gles2::GLES2Decoder::Create(group.get()));
  if (!decoder_.get())
    return EGL_NO_SURFACE;

  executor_.reset(
      new gpu::CommandExecutor(command_buffer.get(), decoder_.get(), NULL));

  decoder_->set_engine(executor_.get());
  gfx::Size size(create_offscreen_width_, create_offscreen_height_);
  if (create_offscreen_) {
    gl_surface_ = gfx::GLSurface::CreateOffscreenGLSurface(size);
    create_offscreen_ = false;
    create_offscreen_width_ = 0;
    create_offscreen_height_ = 0;
  } else {
    gl_surface_ = gfx::GLSurface::CreateViewGLSurface(win);
  }
  if (!gl_surface_.get())
    return EGL_NO_SURFACE;

  gl_context_ = gfx::GLContext::CreateGLContext(NULL,
                                                gl_surface_.get(),
                                                gfx::PreferDiscreteGpu);
  if (!gl_context_.get())
    return EGL_NO_SURFACE;

  gl_context_->MakeCurrent(gl_surface_.get());

  EGLint depth_size = 0;
  EGLint alpha_size = 0;
  EGLint stencil_size = 0;
  GetConfigAttrib(config, EGL_DEPTH_SIZE, &depth_size);
  GetConfigAttrib(config, EGL_ALPHA_SIZE, &alpha_size);
  GetConfigAttrib(config, EGL_STENCIL_SIZE, &stencil_size);
  std::vector<int32_t> attribs;
  attribs.push_back(EGL_DEPTH_SIZE);
  attribs.push_back(depth_size);
  attribs.push_back(EGL_ALPHA_SIZE);
  attribs.push_back(alpha_size);
  attribs.push_back(EGL_STENCIL_SIZE);
  attribs.push_back(stencil_size);
  // TODO(gman): Insert attrib_list. Although ES 1.1 says it must be null
  attribs.push_back(EGL_NONE);

  if (!decoder_->Initialize(gl_surface_.get(),
                            gl_context_.get(),
                            gl_surface_->IsOffscreen(),
                            size,
                            gpu::gles2::DisallowedFeatures(),
                            attribs)) {
    return EGL_NO_SURFACE;
  }

  command_buffer->SetPutOffsetChangeCallback(base::Bind(
      &gpu::CommandExecutor::PutChanged, base::Unretained(executor_.get())));
  command_buffer->SetGetBufferChangeCallback(base::Bind(
      &gpu::CommandExecutor::SetGetBuffer, base::Unretained(executor_.get())));

  std::unique_ptr<gpu::gles2::GLES2CmdHelper> cmd_helper(
      new gpu::gles2::GLES2CmdHelper(command_buffer.get()));
  if (!cmd_helper->Initialize(kCommandBufferSize))
    return NULL;

  std::unique_ptr<gpu::TransferBuffer> transfer_buffer(
      new gpu::TransferBuffer(cmd_helper.get()));

  command_buffer_.reset(command_buffer.release());
  transfer_buffer_.reset(transfer_buffer.release());
  gles2_cmd_helper_.reset(cmd_helper.release());
  surface_.reset(new Surface(win));

  return surface_.get();
}

void Display::DestroySurface(EGLSurface surface) {
  DCHECK(IsValidSurface(surface));
  executor_.reset();
  if (decoder_.get()) {
    decoder_->Destroy(true);
  }
  decoder_.reset();
  gl_surface_ = NULL;
  gl_context_ = NULL;
  surface_.reset();
}

void Display::SwapBuffers(EGLSurface surface) {
  DCHECK(IsValidSurface(surface));
  context_->SwapBuffers();
}

bool Display::IsValidContext(EGLContext ctx) {
  return (ctx != NULL) && (ctx == context_.get());
}

EGLContext Display::CreateContext(EGLConfig config,
                                  EGLContext share_ctx,
                                  const EGLint* attrib_list) {
  DCHECK(IsValidConfig(config));
  // TODO(alokp): Add support for shared contexts.
  if (share_ctx != NULL)
    return EGL_NO_CONTEXT;

  DCHECK(command_buffer_ != NULL);
  DCHECK(transfer_buffer_.get());

  bool bind_generates_resources = true;
  bool lose_context_when_out_of_memory = false;
  bool support_client_side_arrays = true;

  context_.reset(
      new gpu::gles2::GLES2Implementation(gles2_cmd_helper_.get(),
                                          NULL,
                                          transfer_buffer_.get(),
                                          bind_generates_resources,
                                          lose_context_when_out_of_memory,
                                          support_client_side_arrays,
                                          this));

  if (!context_->Initialize(
      kTransferBufferSize,
      kTransferBufferSize / 2,
      kTransferBufferSize * 2,
      gpu::gles2::GLES2Implementation::kNoLimit)) {
    return EGL_NO_CONTEXT;
  }

  context_->EnableFeatureCHROMIUM("pepper3d_allow_buffers_on_multiple_targets");
  context_->EnableFeatureCHROMIUM("pepper3d_support_fixed_attribs");

  return context_.get();
}

void Display::DestroyContext(EGLContext ctx) {
  DCHECK(IsValidContext(ctx));
  context_.reset();
  transfer_buffer_.reset();
}

bool Display::MakeCurrent(EGLSurface draw, EGLSurface read, EGLContext ctx) {
  if (ctx == EGL_NO_CONTEXT) {
    gles2::SetGLContext(NULL);
  } else {
    DCHECK(IsValidSurface(draw));
    DCHECK(IsValidSurface(read));
    DCHECK(IsValidContext(ctx));
    gles2::SetGLContext(context_.get());
    gl_context_->MakeCurrent(gl_surface_.get());
  }
  return true;
}

void Display::SetGpuControlClient(gpu::GpuControlClient*) {
  // The client is not currently called, so don't store it.
}

gpu::Capabilities Display::GetCapabilities() {
  return decoder_->GetCapabilities();
}

int32_t Display::CreateImage(ClientBuffer buffer,
                             size_t width,
                             size_t height,
                             unsigned internalformat) {
  NOTIMPLEMENTED();
  return -1;
}

void Display::DestroyImage(int32_t id) {
  NOTIMPLEMENTED();
}

int32_t Display::CreateGpuMemoryBufferImage(size_t width,
                                            size_t height,
                                            unsigned internalformat,
                                            unsigned usage) {
  NOTIMPLEMENTED();
  return -1;
}

void Display::SignalQuery(uint32_t query, const base::Closure& callback) {
  NOTIMPLEMENTED();
}

void Display::SetLock(base::Lock*) {
  NOTIMPLEMENTED();
}

bool Display::IsGpuChannelLost() {
  NOTIMPLEMENTED();
  return false;
}

void Display::EnsureWorkVisible() {
  // This is only relevant for out-of-process command buffers.
}

gpu::CommandBufferNamespace Display::GetNamespaceID() const {
  return gpu::CommandBufferNamespace::IN_PROCESS;
}

gpu::CommandBufferId Display::GetCommandBufferID() const {
  return gpu::CommandBufferId();
}

int32_t Display::GetExtraCommandBufferData() const {
  return 0;
}

uint64_t Display::GenerateFenceSyncRelease() {
  return next_fence_sync_release_++;
}

bool Display::IsFenceSyncRelease(uint64_t release) {
  return release > 0 && release < next_fence_sync_release_;
}

bool Display::IsFenceSyncFlushed(uint64_t release) {
  return IsFenceSyncRelease(release);
}

bool Display::IsFenceSyncFlushReceived(uint64_t release) {
  return IsFenceSyncRelease(release);
}

void Display::SignalSyncToken(const gpu::SyncToken& sync_token,
                              const base::Closure& callback) {
  NOTIMPLEMENTED();
}

bool Display::CanWaitUnverifiedSyncToken(const gpu::SyncToken* sync_token) {
  return false;
}

}  // namespace egl
