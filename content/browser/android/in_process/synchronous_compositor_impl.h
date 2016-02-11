// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_IMPL_H_
#define CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_IMPL_H_

#include <stddef.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/android/synchronous_compositor_base.h"
#include "content/renderer/android/synchronous_compositor_external_begin_frame_source.h"
#include "content/renderer/android/synchronous_compositor_output_surface.h"
#include "ipc/ipc_message.h"
#include "ui/events/blink/synchronous_input_handler_proxy.h"

namespace cc {
class InputHandler;
}

namespace content {
class InputHandlerManager;
class RenderWidgetHostViewAndroid;
class SynchronousCompositorExternalBeginFrameSource;
struct DidOverscrollParams;

// The purpose of this class is to act as the intermediary between the various
// components that make up the 'synchronous compositor mode' implementation and
// expose their functionality via the SynchronousCompositor interface.
// This class is created on the main thread but most of the APIs are called
// from the Compositor thread.
class SynchronousCompositorImpl
    : public ui::SynchronousInputHandler,
      public SynchronousCompositorBase,
      public SynchronousCompositorExternalBeginFrameSourceClient,
      public SynchronousCompositorOutputSurfaceClient {
 public:
  // For handling upcalls from renderer code; the process id
  // is implicitly that of the in-process renderer.
  static SynchronousCompositorImpl* FromRoutingID(int routing_id);

  ~SynchronousCompositorImpl() override;

  // Called by SynchronousCompositorRegistry.
  void DidInitializeRendererObjects(
      SynchronousCompositorOutputSurface* output_surface,
      SynchronousCompositorExternalBeginFrameSource* begin_frame_source,
      ui::SynchronousInputHandlerProxy* synchronous_input_handler_proxy);
  void DidDestroyRendererObjects();

  // SynchronousCompositorExternalBeginFrameSourceClient overrides.
  void OnNeedsBeginFramesChange(bool needs_begin_frames) override;

  // SynchronousCompositorOutputSurfaceClient overrides.
  void Invalidate() override;
  void SwapBuffers(cc::CompositorFrame* frame) override;

  // SynchronousCompositor overrides.
  scoped_ptr<cc::CompositorFrame> DemandDrawHw(
      const gfx::Size& surface_size,
      const gfx::Transform& transform,
      const gfx::Rect& viewport,
      const gfx::Rect& clip,
      const gfx::Rect& viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority) override;
  bool DemandDrawSw(SkCanvas* canvas) override;
  void ReturnResources(const cc::CompositorFrameAck& frame_ack) override;
  void SetMemoryPolicy(size_t bytes_limit) override;
  void DidChangeRootLayerScrollOffset(
      const gfx::ScrollOffset& root_offset) override;
  void SetIsActive(bool is_active) override;
  void OnComputeScroll(base::TimeTicks animation_time) override;

  // SynchronousCompositorBase overrides.
  void BeginFrame(const cc::BeginFrameArgs& args) override;
  InputEventAckState HandleInputEvent(
      const blink::WebInputEvent& input_event) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void DidBecomeCurrent() override;

  // SynchronousInputHandler
  void SetNeedsSynchronousAnimateInput() override;
  void UpdateRootLayerState(const gfx::ScrollOffset& total_scroll_offset,
                            const gfx::ScrollOffset& max_scroll_offset,
                            const gfx::SizeF& scrollable_size,
                            float page_scale_factor,
                            float min_page_scale_factor,
                            float max_page_scale_factor) override;

  void DidOverscroll(const DidOverscrollParams& params);
  void DidStopFlinging();

 private:
  friend class SynchronousCompositorBase;
  SynchronousCompositorImpl(RenderWidgetHostViewAndroid* rwhva,
                            SynchronousCompositorClient* client);
  void RegisterWithClient();
  void UpdateFrameMetaData(const cc::CompositorFrameMetadata& frame_info);
  void DidActivatePendingTree();
  void DeliverMessages();
  bool CalledOnValidThread() const;
  void UpdateNeedsBeginFrames();

  RenderWidgetHostViewAndroid* const rwhva_;
  const int routing_id_;
  SynchronousCompositorClient* const compositor_client_;
  SynchronousCompositorOutputSurface* output_surface_;
  SynchronousCompositorExternalBeginFrameSource* begin_frame_source_;
  ui::SynchronousInputHandlerProxy* synchronous_input_handler_proxy_;
  bool registered_with_client_;
  bool is_active_;
  bool renderer_needs_begin_frames_;
  bool need_animate_input_;
  scoped_ptr<cc::CompositorFrame> frame_holder_;

  base::WeakPtrFactory<SynchronousCompositorImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_IMPL_H_
