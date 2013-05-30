// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_OUTPUT_SURFACE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cc/output/output_surface.h"
#include "content/public/renderer/android/synchronous_compositor.h"

namespace content {

class SynchronousCompositorClient;
class SynchronousCompositorOutputSurfaceDelegate;
class WebGraphicsContext3DCommandBufferImpl;

class SynchronousCompositorOutputSurfaceDelegate {
 public:
  virtual void SetContinuousInvalidate(bool enable) = 0;
  virtual void DidCreateSynchronousOutputSurface() = 0;
  virtual void DidDestroySynchronousOutputSurface() = 0;

 protected:
  SynchronousCompositorOutputSurfaceDelegate() {}
  virtual ~SynchronousCompositorOutputSurfaceDelegate() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorOutputSurfaceDelegate);
};

// Specialization of the output surface that adapts it to implement the
// content::SynchronousCompositor public API. This class effects an "inversion
// of control" - enabling drawing to be  orchestrated by the embedding
// layer, instead of driven by the compositor internals - hence it holds two
// 'client' pointers (|client_| in the OutputSurface baseclass and |delegate_|)
// which represent the consumers of the two roles in plays.
// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread when BindToClient is called.
class SynchronousCompositorOutputSurface
    : NON_EXPORTED_BASE(public cc::OutputSurface) {
 public:
  explicit SynchronousCompositorOutputSurface(
      SynchronousCompositorOutputSurfaceDelegate* delegate);
  virtual ~SynchronousCompositorOutputSurface();

  // OutputSurface.
  virtual bool ForcedDrawToSoftwareDevice() const OVERRIDE;
  virtual bool BindToClient(cc::OutputSurfaceClient* surface_client) OVERRIDE;
  virtual void Reshape(gfx::Size size, float scale_factor) OVERRIDE;
  virtual void SendFrameToParentCompositor(cc::CompositorFrame* frame) OVERRIDE;
  virtual void SetNeedsBeginFrame(bool enable) OVERRIDE;
  virtual void SwapBuffers(const ui::LatencyInfo& info) OVERRIDE;

  // Partial SynchronousCompositor API implementation.
  bool IsHwReady();
  bool DemandDrawSw(SkCanvas* canvas);
  bool DemandDrawHw(gfx::Size view_size,
                    const gfx::Transform& transform,
                    gfx::Rect clip);

 private:
  class SoftwareDevice;
  friend class SoftwareDevice;

  void InvokeComposite(const gfx::Transform& transform, gfx::Rect damage_area);
  void UpdateCompositorClientSettings();
  void NotifyCompositorSettingsChanged();
  bool CalledOnValidThread() const;

  SynchronousCompositorOutputSurfaceDelegate* delegate_;
  bool needs_begin_frame_;
  bool did_swap_buffer_;

  // Only valid (non-NULL) during a DemandDrawSw() call.
  SkCanvas* current_sw_canvas_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_OUTPUT_SURFACE_H_
