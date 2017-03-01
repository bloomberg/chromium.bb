// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_H_

#include <stdint.h>

#include <vector>

#include "base/gtest_prod_util.h"
#include "cc/output/copy_output_result.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/compositor_frame_sink_support_client.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/compositor/owned_mailbox.h"
#include "content/browser/renderer_host/delegated_frame_evictor.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/render_process_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace base {
class TickClock;
}

namespace cc {
class CompositorFrameSinkSupport;
class LocalSurfaceIdAllocator;
}

namespace media {
class VideoFrame;
}

namespace display_compositor {
class ReadbackYUVInterface;
}

namespace content {

class DelegatedFrameHost;
class RenderWidgetHostViewFrameSubscriber;
class ResizeLock;

// The DelegatedFrameHostClient is the interface from the DelegatedFrameHost,
// which manages delegated frames, and the ui::Compositor being used to
// display them.
class CONTENT_EXPORT DelegatedFrameHostClient {
 public:
  virtual ~DelegatedFrameHostClient() {}

  virtual ui::Layer* DelegatedFrameHostGetLayer() const = 0;
  virtual bool DelegatedFrameHostIsVisible() const = 0;

  // Returns the color that the resize gutters should be drawn with. Takes the
  // suggested color from the current page background.
  virtual SkColor DelegatedFrameHostGetGutterColor(SkColor color) const = 0;
  virtual gfx::Size DelegatedFrameHostDesiredSizeInDIP() const = 0;

  virtual bool DelegatedFrameCanCreateResizeLock() const = 0;
  virtual std::unique_ptr<ResizeLock> DelegatedFrameHostCreateResizeLock(
      bool defer_compositor_lock) = 0;
  virtual void DelegatedFrameHostResizeLockWasReleased() = 0;

  virtual void DelegatedFrameHostSendReclaimCompositorResources(
      int compositor_frame_sink_id,
      bool is_swap_ack,
      const cc::ReturnedResourceArray& resources) = 0;

  virtual void SetBeginFrameSource(cc::BeginFrameSource* source) = 0;
  virtual bool IsAutoResizeEnabled() const = 0;
};

// The DelegatedFrameHost is used to host all of the RenderWidgetHostView state
// and functionality that is associated with delegated frames being sent from
// the RenderWidget. The DelegatedFrameHost will push these changes through to
// the ui::Compositor associated with its DelegatedFrameHostClient.
class CONTENT_EXPORT DelegatedFrameHost
    : public ui::CompositorObserver,
      public ui::CompositorVSyncManager::Observer,
      public ui::ContextFactoryObserver,
      public DelegatedFrameEvictorClient,
      public NON_EXPORTED_BASE(cc::CompositorFrameSinkSupportClient),
      public cc::ExternalBeginFrameSourceClient,
      public base::SupportsWeakPtr<DelegatedFrameHost> {
 public:
  DelegatedFrameHost(const cc::FrameSinkId& frame_sink_id,
                     DelegatedFrameHostClient* client);
  ~DelegatedFrameHost() override;

  // ui::CompositorObserver implementation.
  void OnCompositingDidCommit(ui::Compositor* compositor) override;
  void OnCompositingStarted(ui::Compositor* compositor,
                            base::TimeTicks start_time) override;
  void OnCompositingEnded(ui::Compositor* compositor) override;
  void OnCompositingLockStateChanged(ui::Compositor* compositor) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // ui::CompositorVSyncManager::Observer implementation.
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval) override;

  // ImageTransportFactoryObserver implementation.
  void OnLostResources() override;

  // DelegatedFrameEvictorClient implementation.
  void EvictDelegatedFrame() override;

  // cc::CompositorFrameSinkSupportClient implementation.
  void DidReceiveCompositorFrameAck() override;
  void OnBeginFrame(const cc::BeginFrameArgs& args) override;
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override;
  void WillDrawSurface(const cc::LocalSurfaceId& id,
                       const gfx::Rect& damage_rect) override;

  // Public interface exposed to RenderWidgetHostView.

  void SwapDelegatedFrame(uint32_t compositor_frame_sink_id,
                          cc::CompositorFrame frame);
  void ClearDelegatedFrame();
  void WasHidden();
  void WasShown(const ui::LatencyInfo& latency_info);
  void WasResized();
  bool HasSavedFrame();
  gfx::Size GetRequestedRendererSize() const;
  void SetCompositor(ui::Compositor* compositor);
  void ResetCompositor();
  // Note: |src_subrect| is specified in DIP dimensions while |output_size|
  // expects pixels. If |src_subrect| is empty, the entire surface area is
  // copied.
  void CopyFromCompositingSurface(const gfx::Rect& src_subrect,
                                  const gfx::Size& output_size,
                                  const ReadbackRequestCallback& callback,
                                  const SkColorType preferred_color_type);
  void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      scoped_refptr<media::VideoFrame> target,
      const base::Callback<void(const gfx::Rect&, bool)>& callback);
  bool CanCopyFromCompositingSurface() const;
  void BeginFrameSubscription(
      std::unique_ptr<RenderWidgetHostViewFrameSubscriber> subscriber);
  void EndFrameSubscription();
  bool HasFrameSubscriber() const { return !!frame_subscriber_; }
  cc::FrameSinkId GetFrameSinkId();
  // Returns a null SurfaceId if this DelegatedFrameHost has not yet created
  // a compositor Surface.
  cc::SurfaceId SurfaceIdAtPoint(cc::SurfaceHittestDelegate* delegate,
                                 const gfx::Point& point,
                                 gfx::Point* transformed_point);

  // Given the SurfaceID of a Surface that is contained within this class'
  // Surface, find the relative transform between the Surfaces and apply it
  // to a point. Returns false if a Surface has not yet been created or if
  // |original_surface| is not embedded within our current Surface.
  bool TransformPointToLocalCoordSpace(const gfx::Point& point,
                                       const cc::SurfaceId& original_surface,
                                       gfx::Point* transformed_point);

  // Given a RenderWidgetHostViewBase that renders to a Surface that is
  // contained within this class' Surface, find the relative transform between
  // the Surfaces and apply it to a point. Returns false if a Surface has not
  // yet been created or if |target_view| is not a descendant RWHV from our
  // client.
  bool TransformPointToCoordSpaceForView(const gfx::Point& point,
                                         RenderWidgetHostViewBase* target_view,
                                         gfx::Point* transformed_point);

  // Exposed for tests.
  cc::SurfaceId SurfaceIdForTesting() const {
    return cc::SurfaceId(frame_sink_id_, local_surface_id_);
  }

  const cc::LocalSurfaceId& LocalSurfaceIdForTesting() const {
    return local_surface_id_;
  }

  void OnCompositingDidCommitForTesting(ui::Compositor* compositor) {
    OnCompositingDidCommit(compositor);
  }
  bool ReleasedFrontLockActiveForTesting() const {
    return !!released_front_lock_.get();
  }
  void SetRequestCopyOfOutputCallbackForTesting(
      const base::Callback<void(std::unique_ptr<cc::CopyOutputRequest>)>&
          callback) {
    request_copy_of_output_callback_for_testing_ = callback;
  }

 private:
  friend class DelegatedFrameHostClient;
  friend class RenderWidgetHostViewAuraCopyRequestTest;
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           SkippedDelegatedFrames);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           DiscardDelegatedFramesWithLocking);

  RenderWidgetHostViewFrameSubscriber* frame_subscriber() const {
    return frame_subscriber_.get();
  }
  bool ShouldCreateResizeLock();
  void LockResources();
  void UnlockResources();
  void RequestCopyOfOutput(std::unique_ptr<cc::CopyOutputRequest> request);

  bool ShouldSkipFrame(gfx::Size size_in_dip) const;

  // Lazily grab a resize lock if the aura window size doesn't match the current
  // frame size, to give time to the renderer.
  void MaybeCreateResizeLock();

  // Checks if the resize lock can be released because we received an new frame.
  void CheckResizeLock();

  SkColor GetGutterColor() const;

  // Update the layers for the resize gutters to the right and bottom of the
  // surface layer.
  void UpdateGutters();

  // Called after async thumbnailer task completes.  Scales and crops the result
  // of the copy.
  static void CopyFromCompositingSurfaceHasResultForVideo(
      base::WeakPtr<DelegatedFrameHost> rwhva,
      scoped_refptr<OwnedMailbox> subscriber_texture,
      scoped_refptr<media::VideoFrame> video_frame,
      const base::Callback<void(const gfx::Rect&, bool)>& callback,
      std::unique_ptr<cc::CopyOutputResult> result);
  static void CopyFromCompositingSurfaceFinishedForVideo(
      scoped_refptr<media::VideoFrame> video_frame,
      base::WeakPtr<DelegatedFrameHost> rwhva,
      const base::Callback<void(bool)>& callback,
      scoped_refptr<OwnedMailbox> subscriber_texture,
      std::unique_ptr<cc::SingleReleaseCallback> release_callback,
      bool result);
  static void ReturnSubscriberTexture(
      base::WeakPtr<DelegatedFrameHost> rwhva,
      scoped_refptr<OwnedMailbox> subscriber_texture,
      const gpu::SyncToken& sync_token);

  // Called to consult the current |frame_subscriber_|, to determine and maybe
  // initiate a copy-into-video-frame request.
  void AttemptFrameSubscriberCapture(const gfx::Rect& damage_rect);

  // cc::ExternalBeginFrameSource implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;
  void OnDidFinishFrame(const cc::BeginFrameAck& ack) override;

  void CreateCompositorFrameSinkSupport();
  void ResetCompositorFrameSinkSupport();

  const cc::FrameSinkId frame_sink_id_;
  cc::LocalSurfaceId local_surface_id_;
  DelegatedFrameHostClient* const client_;
  ui::Compositor* compositor_;

  // The vsync manager we are observing for changes, if any.
  scoped_refptr<ui::CompositorVSyncManager> vsync_manager_;

  // The current VSync timebase and interval. These are zero until the first
  // call to SetVSyncParameters().
  base::TimeTicks vsync_timebase_;
  base::TimeDelta vsync_interval_;

  // Overridable tick clock used for testing functions using current time.
  std::unique_ptr<base::TickClock> tick_clock_;

  // With delegated renderer, this is the last output surface, used to
  // disambiguate resources with the same id coming from different output
  // surfaces.
  uint32_t last_compositor_frame_sink_id_;

  // True after a delegated frame has been skipped, until a frame is not
  // skipped.
  bool skipped_frames_;
  std::vector<ui::LatencyInfo> skipped_latency_info_list_;

  std::unique_ptr<ui::Layer> right_gutter_;
  std::unique_ptr<ui::Layer> bottom_gutter_;

  // This is the last root background color from a swapped frame.
  SkColor background_color_;

  // State for rendering into a Surface.
  std::unique_ptr<cc::LocalSurfaceIdAllocator> id_allocator_;
  std::unique_ptr<cc::CompositorFrameSinkSupport> support_;
  gfx::Size current_surface_size_;
  float current_scale_factor_;
  cc::ReturnedResourceArray surface_returned_resources_;

  // This lock is the one waiting for a frame of the right size to come back
  // from the renderer/GPU process. It is set from the moment the aura window
  // got resized, to the moment we committed the renderer frame of the same
  // size. It keeps track of the size we expect from the renderer, and locks the
  // compositor, as well as the UI for a short time to give a chance to the
  // renderer of producing a frame of the right size.
  std::unique_ptr<ResizeLock> resize_lock_;

  // Keeps track of the current frame size.
  gfx::Size current_frame_size_in_dip_;

  // This lock is for waiting for a front surface to become available to draw.
  scoped_refptr<ui::CompositorLock> released_front_lock_;

  enum CanLockCompositorState {
    YES_CAN_LOCK,
    // We locked, so at some point we'll need to kick a frame.
    YES_DID_LOCK,
    // No. A lock timed out, we need to kick a new frame before locking again.
    NO_PENDING_RENDERER_FRAME,
    // No. We've got a frame, but it hasn't been committed.
    NO_PENDING_COMMIT,
  };
  CanLockCompositorState can_lock_compositor_;

  base::TimeTicks last_draw_ended_;

  // Subscriber that listens to frame presentation events.
  std::unique_ptr<RenderWidgetHostViewFrameSubscriber> frame_subscriber_;
  std::vector<scoped_refptr<OwnedMailbox>> idle_frame_subscriber_textures_;

  // Callback used to pass the output request to the layer or to a function
  // specified by a test.
  base::Callback<void(std::unique_ptr<cc::CopyOutputRequest>)>
      request_copy_of_output_callback_for_testing_;

  // YUV readback pipeline.
  std::unique_ptr<display_compositor::ReadbackYUVInterface>
      yuv_readback_pipeline_;

  std::unique_ptr<cc::ExternalBeginFrameSource> begin_frame_source_;

  bool needs_begin_frame_ = false;

  std::unique_ptr<DelegatedFrameEvictor> delegated_frame_evictor_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_H_
