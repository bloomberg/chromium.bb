// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_MAILBOX_TO_SURFACE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_VR_MAILBOX_TO_SURFACE_BRIDGE_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"

namespace gl {
class ScopedJavaSurface;
class SurfaceTexture;
}  // namespace gl

namespace gpu {
struct MailboxHolder;
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
  bool IsReady();

  // Checks if a workaround from "gpu/config/gpu_driver_bug_workaround_type.h"
  // is active. Requires initialization to be complete.
  bool IsGpuWorkaroundEnabled(int32_t workaround);

  void CreateSurface(gl::SurfaceTexture*);

  void ResizeSurface(int width, int height);

  // Returns true if swapped successfully. This can fail if the GL
  // context isn't ready for use yet, in that case the caller
  // won't get a new frame on the SurfaceTexture.
  bool CopyMailboxToSurfaceAndSwap(const gpu::MailboxHolder& mailbox);

 private:
  void OnContextAvailable(std::unique_ptr<gl::ScopedJavaSurface> surface,
                          scoped_refptr<viz::ContextProvider>);
  void InitializeRenderer();
  void DestroyContext();
  void DrawQuad(unsigned int textureHandle);

  scoped_refptr<viz::ContextProvider> context_provider_;
  gpu::gles2::GLES2Interface* gl_ = nullptr;
  int surface_handle_ = 0;
  base::OnceClosure on_initialized_;

  // Saved state for a pending resize, the dimensions are only
  // valid if needs_resize_ is true.
  bool needs_resize_ = false;
  int resize_width_;
  int resize_height_;

  // Must be last.
  base::WeakPtrFactory<MailboxToSurfaceBridge> weak_ptr_factory_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_MAILBOX_TO_SURFACE_BRIDGE_H_
