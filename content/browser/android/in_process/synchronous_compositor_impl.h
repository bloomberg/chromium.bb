// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_IMPL_H_
#define CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_IMPL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cc/input/layer_scroll_offset_delegate.h"
#include "content/browser/android/in_process/synchronous_compositor_output_surface.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ipc/ipc_message.h"

namespace cc {
class BeginFrameSource;
class InputHandler;
}

namespace blink {
class WebInputEvent;
}

namespace content {
class InputHandlerManager;
class SynchronousCompositorExternalBeginFrameSource;
struct DidOverscrollParams;

// The purpose of this class is to act as the intermediary between the various
// components that make up the 'synchronous compositor mode' implementation and
// expose their functionality via the SynchronousCompositor interface.
// This class is created on the main thread but most of the APIs are called
// from the Compositor thread.
class SynchronousCompositorImpl
    : public cc::LayerScrollOffsetDelegate,
      public SynchronousCompositor,
      public WebContentsUserData<SynchronousCompositorImpl> {
 public:
  // When used from browser code, use both |process_id| and |routing_id|.
  static SynchronousCompositorImpl* FromID(int process_id, int routing_id);
  // When handling upcalls from renderer code, use this version; the process id
  // is implicitly that of the in-process renderer.
  static SynchronousCompositorImpl* FromRoutingID(int routing_id);

  InputEventAckState HandleInputEvent(const blink::WebInputEvent& input_event);

  // Called by SynchronousCompositorRegistry.
  void DidInitializeRendererObjects(
      SynchronousCompositorOutputSurface* output_surface,
      SynchronousCompositorExternalBeginFrameSource* begin_frame_source);
  void DidDestroyRendererObjects();

  // Called by SynchronousCompositorExternalBeginFrameSource.
  void NeedsBeginFramesChanged() const;

  // SynchronousCompositor
  bool InitializeHwDraw() override;
  void ReleaseHwDraw() override;
  scoped_ptr<cc::CompositorFrame> DemandDrawHw(
      gfx::Size surface_size,
      const gfx::Transform& transform,
      gfx::Rect viewport,
      gfx::Rect clip,
      gfx::Rect viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority) override;
  bool DemandDrawSw(SkCanvas* canvas) override;
  void ReturnResources(const cc::CompositorFrameAck& frame_ack) override;
  void SetMemoryPolicy(size_t bytes_limit) override;
  void DidChangeRootLayerScrollOffset() override;

  // LayerScrollOffsetDelegate
  gfx::ScrollOffset GetTotalScrollOffset() override;
  void UpdateRootLayerState(const gfx::ScrollOffset& total_scroll_offset,
                            const gfx::ScrollOffset& max_scroll_offset,
                            const gfx::SizeF& scrollable_size,
                            float page_scale_factor,
                            float min_page_scale_factor,
                            float max_page_scale_factor) override;
  bool IsExternalFlingActive() const override;

  void SetInputHandler(cc::InputHandler* input_handler);
  void DidOverscroll(const DidOverscrollParams& params);
  void DidStopFlinging();

 private:
  friend class WebContentsUserData<SynchronousCompositorImpl>;
  friend class SynchronousCompositor;
  explicit SynchronousCompositorImpl(WebContents* contents);
  ~SynchronousCompositorImpl() override;

  void SetClient(SynchronousCompositorClient* compositor_client);
  void UpdateFrameMetaData(const cc::CompositorFrameMetadata& frame_info);
  void NotifyDidDestroyCompositorToClient();
  void DidActivatePendingTree();
  void DeliverMessages();
  bool CalledOnValidThread() const;

  SynchronousCompositorClient* compositor_client_;
  SynchronousCompositorOutputSurface* output_surface_;
  SynchronousCompositorExternalBeginFrameSource* begin_frame_source_;
  WebContents* contents_;
  const int routing_id_;
  cc::InputHandler* input_handler_;
  bool invoking_composite_;

  base::WeakPtrFactory<SynchronousCompositorImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_IMPL_H_
