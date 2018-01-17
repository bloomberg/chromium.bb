// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BROWSER_COMPOSITOR_VIEW_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_BROWSER_COMPOSITOR_VIEW_MAC_H_

#include <memory>

#include "base/macros.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"

namespace ui {
class AcceleratedWidgetMacNSView;
}

namespace content {

class RecyclableCompositorMac;

class BrowserCompositorMacClient {
 public:
  virtual SkColor BrowserCompositorMacGetGutterColor() const = 0;
  virtual void BrowserCompositorMacOnBeginFrame() = 0;
  virtual void OnFrameTokenChanged(uint32_t frame_token) = 0;
};

// This class owns a DelegatedFrameHost, and will dynamically attach and
// detach it from a ui::Compositor as needed. The ui::Compositor will be
// detached from the DelegatedFrameHost when the following conditions are
// all met:
// - There are no outstanding copy requests
// - The RenderWidgetHostImpl providing frames to the DelegatedFrameHost
//   is visible.
// - The RenderWidgetHostViewMac that is used to display these frames is
//   attached to the NSView hierarchy of an NSWindow.
class CONTENT_EXPORT BrowserCompositorMac : public DelegatedFrameHostClient {
 public:
  BrowserCompositorMac(
      ui::AcceleratedWidgetMacNSView* accelerated_widget_mac_ns_view,
      BrowserCompositorMacClient* client,
      bool render_widget_host_is_hidden,
      bool ns_view_attached_to_window,
      const viz::FrameSinkId& frame_sink_id);
  ~BrowserCompositorMac() override;

  // These will not return nullptr until Destroy is called.
  DelegatedFrameHost* GetDelegatedFrameHost();

  // Ensure that the currect compositor frame be cleared (even if it is
  // potentially visible).
  void ClearCompositorFrame();

  gfx::AcceleratedWidget GetAcceleratedWidget();
  void DidCreateNewRendererCompositorFrameSink(
      viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink);
  void SubmitCompositorFrame(const viz::LocalSurfaceId& local_surface_id,
                             viz::CompositorFrame frame);
  void OnDidNotProduceFrame(const viz::BeginFrameAck& ack);
  void SetBackgroundColor(SkColor background_color);
  void SetDisplayColorSpace(const gfx::ColorSpace& color_space);
  void WasResized();
  bool HasFrameOfSize(const gfx::Size& desired_size);
  void UpdateVSyncParameters(const base::TimeTicks& timebase,
                             const base::TimeDelta& interval);
  void SetNeedsBeginFrames(bool needs_begin_frames);
  void SetWantsAnimateOnlyBeginFrames();

  // This is used to ensure that the ui::Compositor be attached to the
  // DelegatedFrameHost while the RWHImpl is visible.
  // Note: This should be called before the RWHImpl is made visible and after
  // it has been hidden, in order to ensure that thumbnailer notifications to
  // initiate copies occur before the ui::Compositor be detached.
  void SetRenderWidgetHostIsHidden(bool hidden);

  // This is used to ensure that the ui::Compositor be attached to this
  // NSView while its contents may be visible on-screen, even if the RWHImpl is
  // hidden (e.g, because it is occluded by another window).
  void SetNSViewAttachedToWindow(bool attached);

  // These functions will track the number of outstanding copy requests, and
  // will not allow the ui::Compositor to be detached until all outstanding
  // copies have returned.
  void CopyFromCompositingSurface(const gfx::Rect& src_subrect,
                                  const gfx::Size& dst_size,
                                  const ReadbackRequestCallback& callback,
                                  SkColorType preferred_color_type);
  void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      scoped_refptr<media::VideoFrame> target,
      const base::Callback<void(const gfx::Rect&, bool)>& callback);
  viz::FrameSinkId GetRootFrameSinkId();

  // Indicate that the recyclable compositor should be destroyed, and no future
  // compositors should be recycled.
  static void DisableRecyclingForShutdown();

  // DelegatedFrameHostClient implementation.
  ui::Layer* DelegatedFrameHostGetLayer() const override;
  bool DelegatedFrameHostIsVisible() const override;
  SkColor DelegatedFrameHostGetGutterColor() const override;
  gfx::Size DelegatedFrameHostDesiredSizeInDIP() const override;
  bool DelegatedFrameCanCreateResizeLock() const override;
  viz::LocalSurfaceId GetLocalSurfaceId() const override;
  std::unique_ptr<CompositorResizeLock> DelegatedFrameHostCreateResizeLock()
      override;
  void OnBeginFrame(base::TimeTicks frame_time) override;
  bool IsAutoResizeEnabled() const override;
  void OnFrameTokenChanged(uint32_t frame_token) override;

  // Returns nullptr if no compositor is attached.
  ui::Compositor* CompositorForTesting() const;

 private:
  // The state of |delegated_frame_host_| and |recyclable_compositor_| to
  // manage being visible, hidden, or occluded.
  enum State {
    // Effects:
    // - |recyclable_compositor_| exists and is attached to
    //   |delegated_frame_host_|.
    // Happens when:
    // - |render_widet_host_| is in the visible state, or there are
    //   outstanding copy requests.
    HasAttachedCompositor,
    // Effects:
    // - |recyclable_compositor_| exists, but |delegated_frame_host_| is
    //   hidden and detached from it.
    // Happens when:
    // - The |render_widget_host_| is hidden, but |cocoa_view_| is still in the
    //   NSWindow hierarchy (e.g, when the window is occluded or offscreen).
    // - Note: In this state, |recyclable_compositor_| and its CALayers are kept
    //   around so that we will have content to show when we are un-occluded. If
    //   we had a way to keep the CALayers attached to the NSView while
    //   detaching the ui::Compositor, then there would be no need for this
    HasDetachedCompositor,
    // Effects:
    // - |recyclable_compositor_| has been recycled and |delegated_frame_host_|
    //   is hidden and detached from it.
    // Happens when:
    // - The |render_widget_host_| hidden or gone, and |cocoa_view_| is not
    //   attached to an NSWindow.
    // - This happens for backgrounded tabs.
    HasNoCompositor,
  };
  State state_ = HasNoCompositor;
  void UpdateState();
  void TransitionToState(State new_state);
  void GetViewProperties(gfx::Size* bounds_in_dip,
                         float* scale_factor,
                         gfx::ColorSpace* color_space) const;

  static void CopyCompleted(
      base::WeakPtr<BrowserCompositorMac> browser_compositor,
      const ReadbackRequestCallback& callback,
      const SkBitmap& bitmap,
      ReadbackResponse response);
  static void CopyToVideoFrameCompleted(
      base::WeakPtr<BrowserCompositorMac> browser_compositor,
      const base::Callback<void(const gfx::Rect&, bool)>& callback,
      const gfx::Rect& rect,
      bool result);
  uint64_t outstanding_copy_count_ = 0;

  bool render_widget_host_is_hidden_ = true;
  bool ns_view_attached_to_window_ = false;

  BrowserCompositorMacClient* client_ = nullptr;
  ui::AcceleratedWidgetMacNSView* accelerated_widget_mac_ns_view_ = nullptr;
  std::unique_ptr<RecyclableCompositorMac> recyclable_compositor_;

  std::unique_ptr<DelegatedFrameHost> delegated_frame_host_;
  std::unique_ptr<ui::Layer> root_layer_;

  SkColor background_color_ = SK_ColorWHITE;
  const bool enable_viz_ = false;
  viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink_ =
      nullptr;
  // The surface for the delegated frame host, rendered into by the renderer
  // process.
  viz::LocalSurfaceId delegated_frame_host_surface_id_;
  // The surface for the ui::Compositor, which will embed
  // |delegated_frame_host_surface_id_| into its tree.
  viz::LocalSurfaceId compositor_surface_id_;
  viz::ParentLocalSurfaceIdAllocator parent_local_surface_id_allocator_;

  base::WeakPtrFactory<BrowserCompositorMac> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BROWSER_COMPOSITOR_VIEW_MAC_H_
