// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/tests/gl_manager.h"

#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"

namespace gpu {

int GLManager::use_count_;
scoped_refptr<gfx::GLShareGroup>* GLManager::base_share_group_;
scoped_refptr<gfx::GLSurface>* GLManager::base_surface_;
scoped_refptr<gfx::GLContext>* GLManager::base_context_;

GLManager::Options::Options()
    : size(4, 4),
      share_group_manager(NULL),
      share_mailbox_manager(NULL),
      virtual_manager(NULL),
      bind_generates_resource(false),
      context_lost_allowed(false) {
}

GLManager::GLManager()
    : context_lost_allowed_(false) {
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

void GLManager::Initialize(const GLManager::Options& options) {
  const int32 kCommandBufferSize = 1024 * 1024;
  const size_t kStartTransferBufferSize = 4 * 1024 * 1024;
  const size_t kMinTransferBufferSize = 1 * 256 * 1024;
  const size_t kMaxTransferBufferSize = 16 * 1024 * 1024;
  const bool kShareResources = true;

  context_lost_allowed_ = options.context_lost_allowed;

  gles2::MailboxManager* mailbox_manager = NULL;
  if (options.share_mailbox_manager) {
    mailbox_manager = options.share_mailbox_manager->mailbox_manager();
  } else if (options.share_group_manager) {
    mailbox_manager = options.share_group_manager->mailbox_manager();
  }

  gfx::GLShareGroup* share_group = NULL;
  if (options.share_group_manager) {
    share_group = options.share_group_manager->share_group();
  } else if (options.share_mailbox_manager) {
    share_group = options.share_mailbox_manager->share_group();
  }

  gles2::ContextGroup* context_group = NULL;
  gles2::ShareGroup* client_share_group = NULL;
  if (options.share_group_manager) {
    context_group = options.share_group_manager->decoder_->GetContextGroup();
    client_share_group =
      options.share_group_manager->gles2_implementation()->share_group();
  }

  gfx::GLContext* real_gl_context = NULL;
  if (options.virtual_manager) {
    real_gl_context = options.virtual_manager->context();
  }

  // From <EGL/egl.h>.
  const int32 EGL_ALPHA_SIZE = 0x3021;
  const int32 EGL_BLUE_SIZE = 0x3022;
  const int32 EGL_GREEN_SIZE = 0x3023;
  const int32 EGL_RED_SIZE = 0x3024;
  const int32 EGL_DEPTH_SIZE = 0x3025;
  const int32 EGL_NONE = 0x3038;

  mailbox_manager_ =
      mailbox_manager ? mailbox_manager : new gles2::MailboxManager;
  share_group_ =
      share_group ? share_group : new gfx::GLShareGroup;

  gfx::GpuPreference gpu_preference(gfx::PreferDiscreteGpu);
  const char* allowed_extensions = "*";
  std::vector<int32> attribs;
  attribs.push_back(EGL_RED_SIZE);
  attribs.push_back(8);
  attribs.push_back(EGL_GREEN_SIZE);
  attribs.push_back(8);
  attribs.push_back(EGL_BLUE_SIZE);
  attribs.push_back(8);
  attribs.push_back(EGL_ALPHA_SIZE);
  attribs.push_back(8);
  attribs.push_back(EGL_DEPTH_SIZE);
  attribs.push_back(16);
  attribs.push_back(EGL_NONE);

  if (!context_group) {
    context_group = new gles2::ContextGroup(mailbox_manager_.get(),
                                            NULL,
                                            NULL,
                                            options.bind_generates_resource);
  }

  decoder_.reset(::gpu::gles2::GLES2Decoder::Create(context_group));

  command_buffer_.reset(new CommandBufferService(
      decoder_->GetContextGroup()->transfer_buffer_manager()));
  ASSERT_TRUE(command_buffer_->Initialize())
      << "could not create command buffer service";

  gpu_scheduler_.reset(new GpuScheduler(command_buffer_.get(),
                                        decoder_.get(),
                                        decoder_.get()));

  decoder_->set_engine(gpu_scheduler_.get());

  surface_ = gfx::GLSurface::CreateOffscreenGLSurface(false, options.size);
  ASSERT_TRUE(surface_.get() != NULL) << "could not create offscreen surface";

  if (real_gl_context) {
    context_ = scoped_refptr<gfx::GLContext>(new gpu::GLContextVirtual(
        share_group_.get(), real_gl_context, decoder_->AsWeakPtr()));
    ASSERT_TRUE(context_->Initialize(
        surface_.get(), gfx::PreferIntegratedGpu));
  } else {
    if (base_context_) {
      context_ = scoped_refptr<gfx::GLContext>(new gpu::GLContextVirtual(
          share_group_.get(), base_context_->get(), decoder_->AsWeakPtr()));
      ASSERT_TRUE(context_->Initialize(
          surface_.get(), gfx::PreferIntegratedGpu));
    } else {
      context_ = gfx::GLContext::CreateGLContext(share_group_.get(),
                                                 surface_.get(),
                                                 gpu_preference);
    }
  }
  ASSERT_TRUE(context_.get() != NULL) << "could not create GL context";

  ASSERT_TRUE(context_->MakeCurrent(surface_.get()));

  ASSERT_TRUE(decoder_->Initialize(
      surface_.get(),
      context_.get(),
      true,
      options.size,
      ::gpu::gles2::DisallowedFeatures(),
      allowed_extensions,
      attribs)) << "could not initialize decoder";

  command_buffer_->SetPutOffsetChangeCallback(
      base::Bind(&GLManager::PumpCommands, base::Unretained(this)));
  command_buffer_->SetGetBufferChangeCallback(
      base::Bind(&GLManager::GetBufferChanged, base::Unretained(this)));

  // Create the GLES2 helper, which writes the command buffer protocol.
  gles2_helper_.reset(new gles2::GLES2CmdHelper(command_buffer_.get()));
  ASSERT_TRUE(gles2_helper_->Initialize(kCommandBufferSize));

  // Create a transfer buffer.
  transfer_buffer_.reset(new TransferBuffer(gles2_helper_.get()));

  // Create the object exposing the OpenGL API.
  gles2_implementation_.reset(new gles2::GLES2Implementation(
      gles2_helper_.get(),
      client_share_group,
      transfer_buffer_.get(),
      kShareResources,
      options.bind_generates_resource));

  ASSERT_TRUE(gles2_implementation_->Initialize(
      kStartTransferBufferSize,
      kMinTransferBufferSize,
      kMaxTransferBufferSize)) << "Could not init GLES2Implementation";

  MakeCurrent();
}

void GLManager::SetupBaseContext() {
  if (use_count_) {
    #if defined(OS_ANDROID)
      base_share_group_ = new scoped_refptr<gfx::GLShareGroup>(
          new gfx::GLShareGroup);
      gfx::Size size(4, 4);
      base_surface_ = new scoped_refptr<gfx::GLSurface>(
          gfx::GLSurface::CreateOffscreenGLSurface(false, size));
      gfx::GpuPreference gpu_preference(gfx::PreferDiscreteGpu);
      base_context_ = new scoped_refptr<gfx::GLContext>(
          gfx::GLContext::CreateGLContext(base_share_group_->get(),
                                          base_surface_->get(),
                                          gpu_preference));
    #endif
  }
  ++use_count_;
}

void GLManager::MakeCurrent() {
  ::gles2::SetGLContext(gles2_implementation_.get());
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
    decoder_->MakeCurrent();
    decoder_->Destroy(true);
  }
}

void GLManager::PumpCommands() {
  decoder_->MakeCurrent();
  gpu_scheduler_->PutChanged();
  ::gpu::CommandBuffer::State state = command_buffer_->GetState();
  if (!context_lost_allowed_) {
    ASSERT_EQ(::gpu::error::kNoError, state.error);
  }
}

bool GLManager::GetBufferChanged(int32 transfer_buffer_id) {
  return gpu_scheduler_->SetGetBuffer(transfer_buffer_id);
}

}  // namespace gpu
