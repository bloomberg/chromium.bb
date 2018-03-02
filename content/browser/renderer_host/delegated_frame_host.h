// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_H_

#include <stdint.h>

#include <vector>

#include "base/gtest_prod_util.h"
#include "components/viz/client/frame_evictor.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/host/host_frame_sink_client.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_export.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "services/viz/public/interfaces/hit_test/hit_test_region_list.mojom.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace base {
class TickClock;
}

namespace viz {
class CompositorFrameSinkSupport;
}

namespace content {

class DelegatedFrameHost;
class CompositorResizeLock;

// The DelegatedFrameHostClient is the interface from the DelegatedFrameHost,
// which manages delegated frames, and the ui::Compositor being used to
// display them.
class CONTENT_EXPORT DelegatedFrameHostClient {
 public:
  virtual ~DelegatedFrameHostClient() {}

  virtual ui::Layer* DelegatedFrameHostGetLayer() const = 0;
  virtual bool DelegatedFrameHostIsVisible() const = 0;

  // Returns the color that the resize gutters should be drawn with.
  virtual SkColor DelegatedFrameHostGetGutterColor() const = 0;

  virtual bool DelegatedFrameCanCreateResizeLock() const = 0;
  virtual std::unique_ptr<CompositorResizeLock>
  DelegatedFrameHostCreateResizeLock() = 0;

  virtual void OnFirstSurfaceActivation(
      const viz::SurfaceInfo& surface_info) = 0;
  virtual void OnBeginFrame(base::TimeTicks frame_time) = 0;
  virtual bool IsAutoResizeEnabled() const = 0;
  virtual void OnFrameTokenChanged(uint32_t frame_token) = 0;
  virtual void DidReceiveFirstFrameAfterNavigation() = 0;
};

// The DelegatedFrameHost is used to host all of the RenderWidgetHostView state
// and functionality that is associated with delegated frames being sent from
// the RenderWidget. The DelegatedFrameHost will push these changes through to
// the ui::Compositor associated with its DelegatedFrameHostClient.
class CONTENT_EXPORT DelegatedFrameHost
    : public ui::CompositorObserver,
      public ui::ContextFactoryObserver,
      public viz::FrameEvictorClient,
      public viz::mojom::CompositorFrameSinkClient,
      public viz::HostFrameSinkClient {
 public:
  // |should_register_frame_sink_id| flag indicates whether DelegatedFrameHost
  // is responsible for registering the associated FrameSinkId with the
  // compositor or not. This is set only on non-aura platforms, since aura is
  // responsible for doing the appropriate [un]registration.
  DelegatedFrameHost(const viz::FrameSinkId& frame_sink_id,
                     DelegatedFrameHostClient* client,
                     bool enable_surface_synchronization,
                     bool enable_viz,
                     bool should_register_frame_sink_id);
  ~DelegatedFrameHost() override;

  // ui::CompositorObserver implementation.
  void OnCompositingDidCommit(ui::Compositor* compositor) override;
  void OnCompositingStarted(ui::Compositor* compositor,
                            base::TimeTicks start_time) override;
  void OnCompositingEnded(ui::Compositor* compositor) override;
  void OnCompositingLockStateChanged(ui::Compositor* compositor) override;
  void OnCompositingChildResizing(ui::Compositor* compositor) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // ui::ContextFactoryObserver implementation.
  void OnLostResources() override;

  // FrameEvictorClient implementation.
  void EvictDelegatedFrame() override;

  // viz::mojom::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      const std::vector<viz::ReturnedResource>& resources) override;
  void DidPresentCompositorFrame(uint32_t presentation_token,
                                 base::TimeTicks time,
                                 base::TimeDelta refresh,
                                 uint32_t flags) override;
  void DidDiscardCompositorFrame(uint32_t presentation_token) override;
  void OnBeginFrame(const viz::BeginFrameArgs& args) override;
  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override;
  void OnBeginFramePausedChanged(bool paused) override;

  // viz::HostFrameSinkClient implementation.
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnFrameTokenChanged(uint32_t frame_token) override;

  // Public interface exposed to RenderWidgetHostView.

  void DidCreateNewRendererCompositorFrameSink(
      viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink);
  void SubmitCompositorFrame(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      viz::mojom::HitTestRegionListPtr hit_test_region_list);
  void ClearDelegatedFrame();
  void WasHidden();
  // TODO(ccameron): Include device scale factor here.
  void WasShown(const viz::LocalSurfaceId& local_surface_id,
                const gfx::Size& dip_size,
                const ui::LatencyInfo& latency_info);
  void WasResized(const viz::LocalSurfaceId& local_surface_id,
                  const gfx::Size& dip_size,
                  cc::DeadlinePolicy deadline_policy);
  bool HasSavedFrame() const;
  gfx::Size GetRequestedRendererSize() const;
  void SetCompositor(ui::Compositor* compositor);
  void ResetCompositor();
  // Note: |src_subrect| is specified in DIP dimensions while |output_size|
  // expects pixels. If |src_subrect| is empty, the entire surface area is
  // copied.
  void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& output_size,
      base::OnceCallback<void(const SkBitmap&)> callback);
  bool CanCopyFromCompositingSurface() const;
  const viz::FrameSinkId& frame_sink_id() const { return frame_sink_id_; }

  // Given the SurfaceID of a Surface that is contained within this class'
  // Surface, find the relative transform between the Surfaces and apply it
  // to a point. Returns false if a Surface has not yet been created or if
  // |original_surface| is not embedded within our current Surface.
  bool TransformPointToLocalCoordSpace(const gfx::PointF& point,
                                       const viz::SurfaceId& original_surface,
                                       gfx::PointF* transformed_point);

  // Given a RenderWidgetHostViewBase that renders to a Surface that is
  // contained within this class' Surface, find the relative transform between
  // the Surfaces and apply it to a point. Returns false if a Surface has not
  // yet been created or if |target_view| is not a descendant RWHV from our
  // client.
  bool TransformPointToCoordSpaceForView(const gfx::PointF& point,
                                         RenderWidgetHostViewBase* target_view,
                                         gfx::PointF* transformed_point);

  void SetNeedsBeginFrames(bool needs_begin_frames);
  void SetWantsAnimateOnlyBeginFrames();
  void DidNotProduceFrame(const viz::BeginFrameAck& ack);

  // Returns the surface id for the surface most recently activated by
  // OnFirstSurfaceActivation.
  // TODO(ccameron): GetActiveSurfaceId may be a better name.
  viz::SurfaceId GetCurrentSurfaceId() const {
    return viz::SurfaceId(frame_sink_id_, active_local_surface_id_);
  }
  viz::CompositorFrameSinkSupport* GetCompositorFrameSinkSupportForTesting() {
    return support_.get();
  }

  bool HasPrimarySurface() const;
  bool HasFallbackSurface() const;

  void OnCompositingDidCommitForTesting(ui::Compositor* compositor) {
    OnCompositingDidCommit(compositor);
  }
  bool ReleasedFrontLockActiveForTesting() const {
    return !!released_front_lock_.get();
  }

  gfx::Size CurrentFrameSizeInDipForTesting() const {
    return current_frame_size_in_dip_;
  }

  void DidNavigate();

  bool IsPrimarySurfaceEvicted() const;

 private:
  friend class DelegatedFrameHostClient;
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           SkippedDelegatedFrames);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           DiscardDelegatedFramesWithLocking);

  void LockResources();
  void UnlockResources();

  bool ShouldSkipFrame(const gfx::Size& size_in_dip);

  // Lazily grab a resize lock if the aura window size doesn't match the current
  // frame size, to give time to the renderer.
  void MaybeCreateResizeLock();

  // Checks if the resize lock can be released because we received an new frame.
  void CheckResizeLock();

  SkColor GetGutterColor() const;

  // Update the layers for the resize gutters to the right and bottom of the
  // surface layer.
  void UpdateGutters();

  void CreateCompositorFrameSinkSupport();
  void ResetCompositorFrameSinkSupport();

  const viz::FrameSinkId frame_sink_id_;
  DelegatedFrameHostClient* const client_;
  const bool enable_surface_synchronization_;
  const bool enable_viz_;
  const bool should_register_frame_sink_id_;
  ui::Compositor* compositor_ = nullptr;

  // The surface id that was most recently activated by
  // OnFirstSurfaceActivation.
  viz::LocalSurfaceId active_local_surface_id_;
  // The scale factor of the above surface.
  float active_device_scale_factor_ = 0.f;

  // The local surface id as of the most recent call to WasResized or WasShown.
  // This is the surface that we expect future frames to reference. This will
  // eventually equal the active surface.
  viz::LocalSurfaceId pending_local_surface_id_;
  // The size of the above surface (updated at the same time).
  gfx::Size pending_surface_dip_size_;

  // In non-surface sync, this is the size of the most recently activated
  // surface (which is suitable for calculating gutter size). In surface sync,
  // this is most recent size set in WasResized.
  // TODO(ccameron): The meaning of "current" should be made more clear here.
  gfx::Size current_frame_size_in_dip_;

  // Overridable tick clock used for testing functions using current time.
  base::TickClock* tick_clock_;

  // True after a delegated frame has been skipped, until a frame is not
  // skipped.
  bool skipped_frames_ = false;
  std::vector<ui::LatencyInfo> skipped_latency_info_list_;

  std::unique_ptr<ui::Layer> right_gutter_;
  std::unique_ptr<ui::Layer> bottom_gutter_;

  // This is the last root background color from a swapped frame.
  SkColor background_color_;

  // State for rendering into a Surface.
  std::unique_ptr<viz::CompositorFrameSinkSupport> support_;

  // This lock is the one waiting for a frame of the right size to come back
  // from the renderer/GPU process. It is set from the moment the aura window
  // got resized, to the moment we committed the renderer frame of the same
  // size. It keeps track of the size we expect from the renderer, and locks the
  // compositor, as well as the UI for a short time to give a chance to the
  // renderer of producing a frame of the right size.
  std::unique_ptr<CompositorResizeLock> resize_lock_;
  bool create_resize_lock_after_commit_ = false;
  bool allow_one_renderer_frame_during_resize_lock_ = false;

  // This lock is for waiting for a front surface to become available to draw.
  std::unique_ptr<ui::CompositorLock> released_front_lock_;

  bool needs_begin_frame_ = false;

  viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink_ =
      nullptr;

  std::unique_ptr<viz::FrameEvictor> frame_evictor_;

  uint32_t first_parent_sequence_number_after_navigation_ = 0;
  bool received_frame_after_navigation_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_H_
