// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_TEXTURE_IMAGE_TRANSPORT_SURFACE_H_
#define CONTENT_COMMON_GPU_TEXTURE_IMAGE_TRANSPORT_SURFACE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/image_transport_surface.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/texture_definition.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace content {
class GpuChannelManager;

class TextureImageTransportSurface
    : public ImageTransportSurface,
      public GpuCommandBufferStub::DestructionObserver,
      public gfx::GLSurface {
 public:
  TextureImageTransportSurface(GpuChannelManager* manager,
                               GpuCommandBufferStub* stub,
                               const gfx::GLSurfaceHandle& handle);

  // gfx::GLSurface implementation.
  virtual bool Initialize() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool DeferDraws() OVERRIDE;
  virtual bool Resize(const gfx::Size& size) OVERRIDE;
  virtual bool IsOffscreen() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual void* GetHandle() OVERRIDE;
  virtual unsigned GetFormat() OVERRIDE;
  virtual std::string GetExtensions() OVERRIDE;
  virtual unsigned int GetBackingFrameBufferObject() OVERRIDE;
  virtual bool PostSubBuffer(int x, int y, int width, int height) OVERRIDE;
  virtual bool OnMakeCurrent(gfx::GLContext* context) OVERRIDE;
  virtual bool SetBackbufferAllocation(bool allocated) OVERRIDE;
  virtual void SetFrontbufferAllocation(bool allocated) OVERRIDE;
  virtual void* GetShareHandle() OVERRIDE;
  virtual void* GetDisplay() OVERRIDE;
  virtual void* GetConfig() OVERRIDE;

 protected:
  // ImageTransportSurface implementation.
  virtual void OnBufferPresented(
      const AcceleratedSurfaceMsg_BufferPresented_Params& params) OVERRIDE;
  virtual void OnResizeViewACK() OVERRIDE;
  virtual void OnResize(gfx::Size size) OVERRIDE;

  // GpuCommandBufferStub::DestructionObserver implementation.
  virtual void OnWillDestroyStub(GpuCommandBufferStub* stub) OVERRIDE;

 private:

  gfx::Size backbuffer_size() const {
    return gfx::Size(backbuffer_->level_infos()[0][0].width,
                     backbuffer_->level_infos()[0][0].height);
  }

  virtual ~TextureImageTransportSurface();
  void CreateBackTexture();
  void AttachBackTextureToFBO();
  void ReleaseBackTexture();
  void BufferPresentedImpl(const std::string& mailbox_name);
  void ProduceTexture();
  void ConsumeTexture();

  static gpu::gles2::TextureDefinition* CreateTextureDefinition(gfx::Size size,
                                                                int service_id);

  // The framebuffer that represents this surface (service id). Allocated lazily
  // in OnMakeCurrent.
  uint32 fbo_id_;

  // The current backbuffer.
  scoped_ptr<gpu::gles2::TextureDefinition> backbuffer_;

  // The mailbox name for the current backbuffer texture. Needs to be unique per
  // GL texture and is invalid while service_id is zero.
  gpu::gles2::MailboxName mailbox_name_;

  // The current size of the GLSurface. Used to disambiguate from the current
  // texture size which might be outdated (since we use two buffers).
  gfx::Size current_size_;

  // Whether or not the command buffer stub has been destroyed.
  bool stub_destroyed_;

  bool backbuffer_suggested_allocation_;
  bool frontbuffer_suggested_allocation_;

  scoped_ptr<ImageTransportHelper> helper_;
  gfx::GLSurfaceHandle handle_;

  // The offscreen surface used to make the context current. However note that
  // the actual rendering is always redirected to an FBO.
  scoped_refptr<gfx::GLSurface> surface_;

  // Holds a reference to the underlying context for cleanup.
  scoped_refptr<gfx::GLContext> context_;

  // Whether a SwapBuffers is pending.
  bool is_swap_buffers_pending_;

  // Whether we unscheduled command buffer because of pending SwapBuffers.
  bool did_unschedule_;

  // Holds a reference to the mailbox manager for cleanup.
  scoped_refptr<gpu::gles2::MailboxManager> mailbox_manager_;

  DISALLOW_COPY_AND_ASSIGN(TextureImageTransportSurface);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_TEXTURE_IMAGE_TRANSPORT_SURFACE_H_
