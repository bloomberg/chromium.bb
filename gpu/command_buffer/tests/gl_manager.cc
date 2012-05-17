// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"

namespace gpu {

GLManager::GLManager() {
}

GLManager::~GLManager() {
}

void GLManager::Initialize(const gfx::Size& size) {
  Setup(size, NULL, NULL, NULL, NULL);
}

void GLManager::InitializeShared(
    const gfx::Size& size, GLManager* gl_manager) {
  DCHECK(gl_manager);
  Setup(
      size,
      gl_manager->mailbox_manager(),
      gl_manager->share_group(),
      gl_manager->decoder_->GetContextGroup(),
      gl_manager->gles2_implementation()->share_group());
}

void GLManager::InitializeSharedMailbox(
     const gfx::Size& size, GLManager* gl_manager) {
  DCHECK(gl_manager);
  Setup(
      size,
      gl_manager->mailbox_manager(),
      gl_manager->share_group(),
      NULL,
      NULL);
}

void GLManager::Setup(
    const gfx::Size& size,
    gles2::MailboxManager* mailbox_manager,
    gfx::GLShareGroup* share_group,
    gles2::ContextGroup* context_group,
    gles2::ShareGroup* client_share_group) {
  const int32 kCommandBufferSize = 1024 * 1024;
  const size_t kStartTransferBufferSize = 4 * 1024 * 1024;
  const size_t kMinTransferBufferSize = 1 * 256 * 1024;
  const size_t kMaxTransferBufferSize = 16 * 1024 * 1024;
  const bool kBindGeneratesResource = false;
  const bool kShareResources = true;

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

  command_buffer_.reset(new CommandBufferService);
  ASSERT_TRUE(command_buffer_->Initialize())
      << "could not create command buffer service";

  decoder_.reset(::gpu::gles2::GLES2Decoder::Create(
      context_group ? context_group :
          new gles2::ContextGroup(
              mailbox_manager_.get(), kBindGeneratesResource)));

  gpu_scheduler_.reset(new GpuScheduler(command_buffer_.get(),
                                        decoder_.get(),
                                        decoder_.get()));

  decoder_->set_engine(gpu_scheduler_.get());

  surface_ = gfx::GLSurface::CreateOffscreenGLSurface(false, size);
  ASSERT_TRUE(surface_.get() != NULL) << "could not create offscreen surface";

  context_ = gfx::GLContext::CreateGLContext(share_group_.get(),
                                             surface_.get(),
                                             gpu_preference);
  ASSERT_TRUE(context_.get() != NULL) << "could not create GL context";

  ASSERT_TRUE(context_->MakeCurrent(surface_.get()));

  ASSERT_TRUE(decoder_->Initialize(
      surface_.get(),
      context_.get(),
      true,
      size,
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
      kBindGeneratesResource));

  ASSERT_TRUE(gles2_implementation_->Initialize(
      kStartTransferBufferSize,
      kMinTransferBufferSize,
      kMaxTransferBufferSize)) << "Could not init GLES2Implementation";

  MakeCurrent();
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
  ASSERT_EQ(::gpu::error::kNoError, state.error);
}

bool GLManager::GetBufferChanged(int32 transfer_buffer_id) {
  return gpu_scheduler_->SetGetBuffer(transfer_buffer_id);
}

}  // namespace gpu

