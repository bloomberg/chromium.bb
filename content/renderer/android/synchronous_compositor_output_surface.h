// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_RENDERER_ANDOIRD_SYNCHRONOUS_COMPOSITOR_OUTPUT_SURFACE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cc/output/output_surface.h"
#include "content/public/renderer/android/synchronous_compositor.h"

namespace content {

class SynchronousCompositorClient;
class WebGraphicsContext3DCommandBufferImpl;

// Specialization of the output surface that adapts it to implement the
// content::SynchronousCompositor public API. This class effects an "inversion
// of control" - enabling drawing to be  orchestrated by the embedding
// layer, instead of driven by the compositor internals - hence it holds two
// 'client' pointers (including |client_| in the OutputSurface baseclass) which
// represent the consumers of the two roles in plays.
// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread when BindToClient is called.
class SynchronousCompositorOutputSurface
    : NON_EXPORTED_BASE(public cc::OutputSurface),
      NON_EXPORTED_BASE(public SynchronousCompositor) {
 public:
  SynchronousCompositorOutputSurface(
      int32 routing_id,
      WebGraphicsContext3DCommandBufferImpl* context);
  virtual ~SynchronousCompositorOutputSurface();

  // OutputSurface.
  virtual bool BindToClient(cc::OutputSurfaceClient* surface_client) OVERRIDE;
  virtual void SendFrameToParentCompositor(cc::CompositorFrame* frame) OVERRIDE;

  // SynchronousCompositor.
  virtual void SetClient(SynchronousCompositorClient* compositor_client)
      OVERRIDE;
  virtual bool DemandDrawSw(SkCanvas* canvas) OVERRIDE;

 private:
  bool CalledOnValidThread() const;

  SynchronousCompositorClient* compositor_client_;
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_OUTPUT_SURFACE_H_
