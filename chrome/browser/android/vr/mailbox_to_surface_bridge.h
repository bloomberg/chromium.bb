// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_MAILBOX_TO_SURFACE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_VR_MAILBOX_TO_SURFACE_BRIDGE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_fence.h"

namespace gl {
class ScopedJavaSurface;
class SurfaceTexture;
}  // namespace gl

namespace gfx {
struct GpuMemoryBufferHandle;
}

namespace gpu {
class ContextSupport;
struct Mailbox;
struct MailboxHolder;
struct SyncToken;
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace viz {
class ContextProvider;
}

namespace vr {

class MailboxToSurfaceBridge {
 public:
  explicit MailboxToSurfaceBridge(base::OnceClosure on_initialized);
  ~MailboxToSurfaceBridge();

  // Returns true if the GPU process connection is established and ready to use.
  // Equivalent to waiting for on_initialized to be called.
  bool IsConnected();

  // Checks if a workaround from "gpu/config/gpu_driver_bug_workaround_type.h"
  // is active. Requires initialization to be complete.
  bool IsGpuWorkaroundEnabled(int32_t workaround);

  void CreateSurface(gl::SurfaceTexture*);

  void ResizeSurface(int width, int height);

  // Returns true if swapped successfully. This can fail if the GL
  // context isn't ready for use yet, in that case the caller
  // won't get a new frame on the SurfaceTexture.
  bool CopyMailboxToSurfaceAndSwap(const gpu::MailboxHolder& mailbox);

  void GenSyncToken(gpu::SyncToken* out_sync_token);

  // Copies a GpuFence from the local context to the GPU process,
  // and issues a server wait for it.
  void WaitForClientGpuFence(gfx::GpuFence*);

  // Creates a GpuFence in the GPU process after the supplied sync_token
  // completes, and copies it for use in the local context. This is
  // asynchronous, the callback receives the GpuFence once it's available.
  void CreateGpuFence(
      const gpu::SyncToken& sync_token,
      base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback);

  void GenerateMailbox(gpu::Mailbox& out_mailbox);

  // Creates a texture and binds it to the mailbox. Returns its
  // texture ID in the command buffer context. (Don't use that
  // in the local GL context, it's not valid there.)
  uint32_t CreateMailboxTexture(const gpu::Mailbox& mailbox);

  // Unbinds the texture from the mailbox and destroys it.
  void DestroyMailboxTexture(const gpu::Mailbox& mailbox, uint32_t texture_id);

  // Creates a GLImage from the handle's GpuMemoryBuffer and binds it to
  // the supplied texture_id in the GPU process. Returns the image ID in the
  // command buffer context.
  uint32_t BindSharedBufferImage(const gfx::GpuMemoryBufferHandle&,
                                 const gfx::Size& size,
                                 gfx::BufferFormat format,
                                 gfx::BufferUsage usage,
                                 uint32_t texture_id);

  void UnbindSharedBuffer(uint32_t image_id, uint32_t texture_id);

 private:
  void OnContextAvailable(std::unique_ptr<gl::ScopedJavaSurface> surface,
                          scoped_refptr<viz::ContextProvider>);
  void InitializeRenderer();
  void DestroyContext();
  void DrawQuad(unsigned int textureHandle);

  scoped_refptr<viz::ContextProvider> context_provider_;
  gpu::gles2::GLES2Interface* gl_ = nullptr;
  gpu::ContextSupport* context_support_ = nullptr;
  int surface_handle_ = 0;
  base::OnceClosure on_initialized_;

  // Saved state for a pending resize, the dimensions are only
  // valid if needs_resize_ is true.
  bool needs_resize_ = false;
  int resize_width_;
  int resize_height_;

  // Must be last.
  base::WeakPtrFactory<MailboxToSurfaceBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MailboxToSurfaceBridge);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_MAILBOX_TO_SURFACE_BRIDGE_H_
