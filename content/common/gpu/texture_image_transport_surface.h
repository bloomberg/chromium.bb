// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_TEXTURE_IMAGE_TRANSPORT_SURFACE_H_
#define CONTENT_COMMON_GPU_TEXTURE_IMAGE_TRANSPORT_SURFACE_H_
#pragma once

#include "base/basictypes.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/image_transport_surface.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_surface.h"

class GpuChannelManager;

class TextureImageTransportSurface :
    public ImageTransportSurface,
    public GpuCommandBufferStub::DestructionObserver,
    public gfx::GLSurface {
 public:
  TextureImageTransportSurface(GpuChannelManager* manager,
                               GpuCommandBufferStub* stub,
                               const gfx::GLSurfaceHandle& handle);

  // gfx::GLSurface implementation.
  virtual bool Initialize() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool Resize(const gfx::Size& size) OVERRIDE;
  virtual bool IsOffscreen() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual void* GetHandle() OVERRIDE;
  virtual std::string GetExtensions() OVERRIDE;
  virtual unsigned int GetBackingFrameBufferObject() OVERRIDE;
  virtual bool PostSubBuffer(int x, int y, int width, int height) OVERRIDE;
  virtual bool OnMakeCurrent(gfx::GLContext* context) OVERRIDE;
  virtual void SetBufferAllocation(BufferAllocationState state) OVERRIDE;
  virtual void* GetShareHandle() OVERRIDE;
  virtual void* GetDisplay() OVERRIDE;
  virtual void* GetConfig() OVERRIDE;

 protected:
  // ImageTransportSurface implementation.
  virtual void OnNewSurfaceACK(
      uint64 surface_handle, TransportDIB::Handle shm_handle) OVERRIDE;
  virtual void OnBuffersSwappedACK() OVERRIDE;
  virtual void OnPostSubBufferACK() OVERRIDE;
  virtual void OnResizeViewACK() OVERRIDE;
  virtual void OnResize(gfx::Size size) OVERRIDE;

  // GpuCommandBufferStub::DestructionObserver implementation.
  virtual void OnWillDestroyStub(GpuCommandBufferStub* stub) OVERRIDE;

 private:
  // A texture backing the front/back buffer in the parent stub.
  struct Texture {
    Texture();
    ~Texture();

    // The client-side id in the parent stub.
    uint32 client_id;

    // The currently allocated size.
    gfx::Size size;

    // Whether or not that texture has been sent to the client yet.
    bool sent_to_client;

    // The texture info in the parent stub.
    gpu::gles2::TextureManager::TextureInfo::Ref info;
  };

  virtual ~TextureImageTransportSurface();
  void CreateBackTexture(const gfx::Size& size);
  void ReleaseBackTexture();
  void AttachBackTextureToFBO();
  void ReleaseParentStub();
  int back() const { return 1 - front_; }

  // The framebuffer that represents this surface (service id). Allocated lazily
  // in OnMakeCurrent.
  uint32 fbo_id_;

  // The front and back buffers.
  Texture textures_[2];

  gfx::Rect previous_damage_rect_;

  // Indicates which of the 2 above is the front buffer.
  int front_;

  // Whether or not the command buffer stub has been destroyed.
  bool stub_destroyed_;

  scoped_ptr<ImageTransportHelper> helper_;
  GpuCommandBufferStub* parent_stub_;

  DISALLOW_COPY_AND_ASSIGN(TextureImageTransportSurface);
};

#endif  // CONTENT_COMMON_GPU_TEXTURE_IMAGE_TRANSPORT_SURFACE_H_
