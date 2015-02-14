// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_OUTPUT_SURFACE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "ipc/ipc_message.h"
#include "ui/gfx/transform.h"

namespace cc {
class ContextProvider;
class CompositorFrameMetadata;
}

namespace IPC {
class Message;
}

namespace content {

class FrameSwapMessageQueue;
class SynchronousCompositorClient;
class SynchronousCompositorExternalBeginFrameSource;
class SynchronousCompositorOutputSurface;
class WebGraphicsContext3DCommandBufferImpl;

// Specialization of the output surface that adapts it to implement the
// content::SynchronousCompositor public API. This class effects an "inversion
// of control" - enabling drawing to be  orchestrated by the embedding
// layer, instead of driven by the compositor internals - hence it holds two
// 'client' pointers (|client_| in the OutputSurface baseclass and
// |delegate_|) which represent the consumers of the two roles in plays.
// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread when BindToClient is called.
class SynchronousCompositorOutputSurface
    : NON_EXPORTED_BASE(public cc::OutputSurface) {
 public:
  explicit SynchronousCompositorOutputSurface(
      int routing_id,
      scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue);
  ~SynchronousCompositorOutputSurface() override;

  // OutputSurface.
  bool BindToClient(cc::OutputSurfaceClient* surface_client) override;
  void Reshape(const gfx::Size& size, float scale_factor) override;
  void SwapBuffers(cc::CompositorFrame* frame) override;

  void SetBeginFrameSource(
      SynchronousCompositorExternalBeginFrameSource* begin_frame_source);

  // Partial SynchronousCompositor API implementation.
  bool InitializeHwDraw(
      scoped_refptr<cc::ContextProvider> onscreen_context_provider,
      scoped_refptr<cc::ContextProvider> worker_context_provider);
  void ReleaseHwDraw();
  scoped_ptr<cc::CompositorFrame> DemandDrawHw(
      gfx::Size surface_size,
      const gfx::Transform& transform,
      gfx::Rect viewport,
      gfx::Rect clip,
      gfx::Rect viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority);
  void ReturnResources(const cc::CompositorFrameAck& frame_ack);
  scoped_ptr<cc::CompositorFrame> DemandDrawSw(SkCanvas* canvas);
  void SetMemoryPolicy(size_t bytes_limit);
  void SetTreeActivationCallback(const base::Closure& callback);
  void GetMessagesToDeliver(ScopedVector<IPC::Message>* messages);

 private:
  class SoftwareDevice;
  friend class SoftwareDevice;

  void InvokeComposite(const gfx::Transform& transform,
                       gfx::Rect viewport,
                       gfx::Rect clip,
                       gfx::Rect viewport_rect_for_tile_priority,
                       gfx::Transform transform_for_tile_priority,
                       bool hardware_draw);
  bool CalledOnValidThread() const;

  const int routing_id_;
  bool registered_;

  gfx::Transform cached_hw_transform_;
  gfx::Rect cached_hw_viewport_;
  gfx::Rect cached_hw_clip_;
  gfx::Rect cached_hw_viewport_rect_for_tile_priority_;
  gfx::Transform cached_hw_transform_for_tile_priority_;

  // Only valid (non-NULL) during a DemandDrawSw() call.
  SkCanvas* current_sw_canvas_;

  cc::ManagedMemoryPolicy memory_policy_;

  cc::OutputSurfaceClient* output_surface_client_;
  scoped_ptr<cc::CompositorFrame> frame_holder_;

  scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue_;

  SynchronousCompositorExternalBeginFrameSource* begin_frame_source_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_OUTPUT_SURFACE_H_
