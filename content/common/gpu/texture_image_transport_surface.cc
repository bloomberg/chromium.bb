// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/texture_image_transport_surface.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "content/common/gpu/gl_scoped_binders.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/sync_point_manager.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "gpu/command_buffer/service/texture_definition.h"

using gpu::gles2::ContextGroup;
using gpu::gles2::MailboxManager;
using gpu::gles2::MailboxName;
using gpu::gles2::TextureDefinition;
using gpu::gles2::TextureManager;

namespace content {

TextureImageTransportSurface::Texture::Texture()
    : service_id(0),
      surface_handle(0) {
}

TextureImageTransportSurface::Texture::~Texture() {
}

TextureImageTransportSurface::TextureImageTransportSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& handle)
      : fbo_id_(0),
        stub_destroyed_(false),
        backbuffer_suggested_allocation_(true),
        frontbuffer_suggested_allocation_(true),
        handle_(handle),
        is_swap_buffers_pending_(false),
        did_unschedule_(false) {
  helper_.reset(new ImageTransportHelper(this,
                                         manager,
                                         stub,
                                         gfx::kNullPluginWindow));
}

TextureImageTransportSurface::~TextureImageTransportSurface() {
  DCHECK(stub_destroyed_);
  Destroy();
}

bool TextureImageTransportSurface::Initialize() {
  mailbox_manager_ =
      helper_->stub()->decoder()->GetContextGroup()->mailbox_manager();

  backbuffer_.surface_handle = 1;

  GpuChannelManager* manager = helper_->manager();
  surface_ = manager->GetDefaultOffscreenSurface();
  if (!surface_.get())
    return false;

  if (!helper_->Initialize())
    return false;

  GpuChannel* parent_channel = manager->LookupChannel(handle_.parent_client_id);
  if (parent_channel) {
    const CommandLine* command_line = CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kUIPrioritizeInGpuProcess))
      helper_->SetPreemptByCounter(parent_channel->MessagesPendingCount());
  }

  return true;
}

void TextureImageTransportSurface::Destroy() {
  if (surface_.get())
    surface_ = NULL;

  helper_->Destroy();
}

bool TextureImageTransportSurface::DeferDraws() {
  // The command buffer hit a draw/clear command that could clobber the
  // texture in use by the UI compositor. If a Swap is pending, abort
  // processing of the command by returning true and unschedule until the Swap
  // Ack arrives.
  DCHECK(!did_unschedule_);
  if (is_swap_buffers_pending_) {
    did_unschedule_ = true;
    helper_->SetScheduled(false);
    return true;
  }
  return false;
}

bool TextureImageTransportSurface::Resize(const gfx::Size&) {
  return true;
}

bool TextureImageTransportSurface::IsOffscreen() {
  return true;
}

bool TextureImageTransportSurface::OnMakeCurrent(gfx::GLContext* context) {
  if (stub_destroyed_) {
    // Early-exit so that we don't recreate the fbo. We still want to return
    // true, so that the context is made current and the GLES2DecoderImpl can
    // release its own resources.
    return true;
  }

  if (!context_.get()) {
    DCHECK(helper_->stub());
    context_ = helper_->stub()->decoder()->GetGLContext();
  }

  if (!fbo_id_) {
    glGenFramebuffersEXT(1, &fbo_id_);
    glBindFramebufferEXT(GL_FRAMEBUFFER, fbo_id_);
    current_size_ = gfx::Size(1, 1);
    helper_->stub()->AddDestructionObserver(this);
  }

  // We could be receiving non-deferred GL commands, that is anything that does
  // not need a framebuffer.
  if (!backbuffer_.service_id && !is_swap_buffers_pending_ &&
      backbuffer_suggested_allocation_) {
    CreateBackTexture();
  }
  return true;
}

unsigned int TextureImageTransportSurface::GetBackingFrameBufferObject() {
  return fbo_id_;
}

void TextureImageTransportSurface::SetBackbufferAllocation(bool allocation) {
  DCHECK(!is_swap_buffers_pending_);
  if (backbuffer_suggested_allocation_ == allocation)
     return;
  backbuffer_suggested_allocation_ = allocation;

  if (backbuffer_suggested_allocation_) {
    DCHECK(!backbuffer_.service_id);
    CreateBackTexture();
  } else {
    ReleaseBackTexture();
  }
}

void TextureImageTransportSurface::SetFrontbufferAllocation(bool allocation) {
  if (frontbuffer_suggested_allocation_ == allocation)
    return;
  frontbuffer_suggested_allocation_ = allocation;

  if (!frontbuffer_suggested_allocation_) {
    GpuHostMsg_AcceleratedSurfaceRelease_Params params;
    helper_->SendAcceleratedSurfaceRelease(params);
  }
}

void* TextureImageTransportSurface::GetShareHandle() {
  return GetHandle();
}

void* TextureImageTransportSurface::GetDisplay() {
  return surface_.get() ? surface_->GetDisplay() : NULL;
}

void* TextureImageTransportSurface::GetConfig() {
  return surface_.get() ? surface_->GetConfig() : NULL;
}

void TextureImageTransportSurface::OnResize(gfx::Size size) {
  current_size_ = size;
  CreateBackTexture();
}

void TextureImageTransportSurface::OnWillDestroyStub(
    GpuCommandBufferStub* stub) {
  DCHECK(stub == helper_->stub());
  stub->RemoveDestructionObserver(this);

  GpuHostMsg_AcceleratedSurfaceRelease_Params params;
  helper_->SendAcceleratedSurfaceRelease(params);

  ReleaseBackTexture();

  // We are losing the stub owning us, this is our last chance to clean up the
  // resources we allocated in the stub's context.
  if (fbo_id_) {
    glDeleteFramebuffersEXT(1, &fbo_id_);
    CHECK_GL_ERROR();
    fbo_id_ = 0;
  }

  stub_destroyed_ = true;
}

bool TextureImageTransportSurface::SwapBuffers() {
  DCHECK(backbuffer_suggested_allocation_);
  if (!frontbuffer_suggested_allocation_)
    return true;

  glFlush();
  ProduceTexture(backbuffer_);

  // Do not allow destruction while we are still waiting for a swap ACK,
  // so we do not leak a texture in the mailbox.
  AddRef();

  DCHECK(backbuffer_.size == current_size_);
  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.surface_handle = backbuffer_.surface_handle;
  params.size = backbuffer_.size;
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);

  DCHECK(!is_swap_buffers_pending_);
  is_swap_buffers_pending_ = true;
  return true;
}

bool TextureImageTransportSurface::PostSubBuffer(
    int x, int y, int width, int height) {
  DCHECK(backbuffer_suggested_allocation_);
  if (!frontbuffer_suggested_allocation_)
    return true;
  const gfx::Rect new_damage_rect(x, y, width, height);
  DCHECK(gfx::Rect(gfx::Point(), current_size_).Contains(new_damage_rect));

  // An empty damage rect is a successful no-op.
  if (new_damage_rect.IsEmpty())
    return true;

  glFlush();
  ProduceTexture(backbuffer_);

  // Do not allow destruction while we are still waiting for a swap ACK,
  // so we do not leak a texture in the mailbox.
  AddRef();

  DCHECK(current_size_ == backbuffer_.size);

  GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params params;
  params.surface_handle = backbuffer_.surface_handle;
  params.surface_size = backbuffer_.size;
  params.x = x;
  params.y = y;
  params.width = width;
  params.height = height;
  helper_->SendAcceleratedSurfacePostSubBuffer(params);

  DCHECK(!is_swap_buffers_pending_);
  is_swap_buffers_pending_ = true;
  return true;
}

std::string TextureImageTransportSurface::GetExtensions() {
  std::string extensions = gfx::GLSurface::GetExtensions();
  extensions += extensions.empty() ? "" : " ";
  extensions += "GL_CHROMIUM_front_buffer_cached ";
  extensions += "GL_CHROMIUM_post_sub_buffer";
  return extensions;
}

gfx::Size TextureImageTransportSurface::GetSize() {
  gfx::Size size = current_size_;

  // OSMesa expects a non-zero size.
  return gfx::Size(size.width() == 0 ? 1 : size.width(),
                   size.height() == 0 ? 1 : size.height());
}

void* TextureImageTransportSurface::GetHandle() {
  return surface_.get() ? surface_->GetHandle() : NULL;
}

unsigned TextureImageTransportSurface::GetFormat() {
  return surface_.get() ? surface_->GetFormat() : 0;
}

void TextureImageTransportSurface::OnBufferPresented(uint64 surface_handle,
                                                     uint32 sync_point) {
  if (sync_point == 0) {
    BufferPresentedImpl(surface_handle);
  } else {
    helper_->manager()->sync_point_manager()->AddSyncPointCallback(
        sync_point,
        base::Bind(&TextureImageTransportSurface::BufferPresentedImpl,
                   this,
                   surface_handle));
  }

  // Careful, we might get deleted now if we were only waiting for
  // a final swap ACK.
  Release();
}

void TextureImageTransportSurface::BufferPresentedImpl(uint64 surface_handle) {
  DCHECK(!backbuffer_.service_id);
  if (surface_handle) {
    DCHECK(surface_handle == 1 || surface_handle == 2);
    backbuffer_.surface_handle = surface_handle;
    ConsumeTexture(backbuffer_);
  } else {
    // We didn't get back a texture, so allocate 'the other' buffer.
    backbuffer_.surface_handle = (backbuffer_.surface_handle == 1) ? 2 : 1;
    mailbox_name(backbuffer_.surface_handle) = MailboxName();
  }

  if (stub_destroyed_ && backbuffer_.service_id) {
    // TODO(sievers): Remove this after changes to the mailbox to take ownership
    // of the service ids.
    DCHECK(context_.get() && surface_.get());
    if (context_->MakeCurrent(surface_.get()))
      glDeleteTextures(1, &backbuffer_.service_id);

    return;
  }

  DCHECK(is_swap_buffers_pending_);
  is_swap_buffers_pending_ = false;

  // We should not have allowed the backbuffer to be discarded while the ack
  // was pending.
  DCHECK(backbuffer_suggested_allocation_);

  // We're relying on the fact that the parent context is
  // finished with it's context when it inserts the sync point that
  // triggers this callback.
  if (helper_->MakeCurrent()) {
    if (backbuffer_.size != current_size_ || !backbuffer_.service_id)
      CreateBackTexture();
    else
      AttachBackTextureToFBO();
  }

  // Even if MakeCurrent fails, schedule anyway, to trigger the lost context
  // logic.
  if (did_unschedule_) {
    did_unschedule_ = false;
    helper_->SetScheduled(true);
  }
}

void TextureImageTransportSurface::OnResizeViewACK() {
  NOTREACHED();
}

void TextureImageTransportSurface::ReleaseBackTexture() {
  if (!backbuffer_.service_id)
    return;

  glDeleteTextures(1, &backbuffer_.service_id);
  backbuffer_.service_id = 0;
  mailbox_name(backbuffer_.surface_handle) = MailboxName();
  glFlush();
  CHECK_GL_ERROR();
}

void TextureImageTransportSurface::CreateBackTexture() {
  // If |is_swap_buffers_pending| we are waiting for our backbuffer
  // in the mailbox, so we shouldn't be reallocating it now.
  DCHECK(!is_swap_buffers_pending_);

  if (backbuffer_.service_id && backbuffer_.size == current_size_)
    return;

  if (!backbuffer_.service_id) {
    MailboxName new_mailbox_name;
    MailboxName& name = mailbox_name(backbuffer_.surface_handle);
    // This slot should be uninitialized.
    DCHECK(!memcmp(&name, &new_mailbox_name, sizeof(MailboxName)));
    mailbox_manager_->GenerateMailboxName(&new_mailbox_name);
    name = new_mailbox_name;
    glGenTextures(1, &backbuffer_.service_id);
  }

  backbuffer_.size = current_size_;

  {
    ScopedTextureBinder texture_binder(backbuffer_.service_id);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
        current_size_.width(), current_size_.height(), 0,
        GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK_GL_ERROR();
  }

  AttachBackTextureToFBO();

  const MailboxName& name = mailbox_name(backbuffer_.surface_handle);

  GpuHostMsg_AcceleratedSurfaceNew_Params params;
  params.width = current_size_.width();
  params.height = current_size_.height();
  params.surface_handle = backbuffer_.surface_handle;
  params.mailbox_name.append(
      reinterpret_cast<const char*>(&name), sizeof(name));
  helper_->SendAcceleratedSurfaceNew(params);
}

void TextureImageTransportSurface::AttachBackTextureToFBO() {
  DCHECK(backbuffer_.service_id);
  ScopedFrameBufferBinder fbo_binder(fbo_id_);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0,
      GL_TEXTURE_2D,
      backbuffer_.service_id,
      0);
  glFlush();
  CHECK_GL_ERROR();

#ifndef NDEBUG
  GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    DLOG(FATAL) << "Framebuffer incomplete: " << status;
  }
#endif
}

void TextureImageTransportSurface::ConsumeTexture(Texture& texture) {
  DCHECK(!texture.service_id);
  DCHECK(texture.surface_handle == 1 || texture.surface_handle == 2);

  scoped_ptr<TextureDefinition> definition(mailbox_manager_->ConsumeTexture(
      GL_TEXTURE_2D, mailbox_name(texture.surface_handle)));
  if (definition.get()) {
    texture.service_id = definition->ReleaseServiceId();
    texture.size = gfx::Size(definition->level_infos()[0][0].width,
                             definition->level_infos()[0][0].height);
  }
}

void TextureImageTransportSurface::ProduceTexture(Texture& texture) {
  DCHECK(texture.service_id);
  DCHECK(texture.surface_handle == 1 || texture.surface_handle == 2);
  TextureManager* texture_manager =
      helper_->stub()->decoder()->GetContextGroup()->texture_manager();
  DCHECK(texture.size.width() > 0 && texture.size.height() > 0);
  TextureDefinition::LevelInfo info(
      GL_TEXTURE_2D, GL_RGBA, texture.size.width(), texture.size.height(), 1,
      0, GL_RGBA, GL_UNSIGNED_BYTE, true);

  TextureDefinition::LevelInfos level_infos;
  level_infos.resize(1);
  level_infos[0].resize(texture_manager->MaxLevelsForTarget(GL_TEXTURE_2D));
  level_infos[0][0] = info;
  scoped_ptr<TextureDefinition> definition(new TextureDefinition(
      GL_TEXTURE_2D,
      texture.service_id,
      GL_LINEAR,
      GL_LINEAR,
      GL_CLAMP_TO_EDGE,
      GL_CLAMP_TO_EDGE,
      GL_NONE,
      true,
      level_infos));
  // Pass NULL as |owner| here to avoid errors from glConsumeTextureCHROMIUM()
  // when the renderer context group goes away before the RWHV handles a pending
  // ACK. We avoid leaking a texture in the mailbox by waiting for the final ACK
  // at which point we consume the correct texture back.
  mailbox_manager_->ProduceTexture(
      GL_TEXTURE_2D,
      mailbox_name(texture.surface_handle),
      definition.release(),
      NULL);
  texture.service_id = 0;
}

}  // namespace content
