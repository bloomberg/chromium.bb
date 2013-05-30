// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_IMPL_H_
#define CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cc/input/layer_scroll_offset_delegate.h"
#include "content/public/renderer/android/synchronous_compositor.h"
#include "content/renderer/android/synchronous_compositor_output_surface.h"

namespace cc {
class OutputSurface;
}

namespace content {

// The purpose of this class is to act as the intermediary between the various
// components that make up the 'synchronous compositor mode' implementation and
// expose their functionality via the SynchronousCompositor interface.
// This class is created on the main thread but most of the APIs are called
// from the Compositor thread.
class SynchronousCompositorImpl
    : public SynchronousCompositor,
      public SynchronousCompositorOutputSurfaceDelegate {
 public:
  explicit SynchronousCompositorImpl(int32 routing_id);
  virtual ~SynchronousCompositorImpl();

  scoped_ptr<cc::OutputSurface> CreateOutputSurface();

  // SynchronousCompositor
  virtual bool IsHwReady() OVERRIDE;
  virtual void SetClient(SynchronousCompositorClient* compositor_client)
      OVERRIDE;
  virtual bool DemandDrawSw(SkCanvas* canvas) OVERRIDE;
  virtual bool DemandDrawHw(
      gfx::Size view_size,
      const gfx::Transform& transform,
      gfx::Rect clip) OVERRIDE;

  // SynchronousCompositorOutputSurfaceDelegate
  virtual void SetContinuousInvalidate(bool enable) OVERRIDE;
  virtual void DidCreateSynchronousOutputSurface() OVERRIDE;
  virtual void DidDestroySynchronousOutputSurface() OVERRIDE;

 private:
  bool CalledOnValidThread() const;

  int routing_id_;
  SynchronousCompositorClient* compositor_client_;
  SynchronousCompositorOutputSurface* output_surface_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_IMPL_H_
