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
#include "ui/display/display.h"

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
  virtual void DidReceiveFirstFrameAfterNavigation() = 0;
  virtual void DestroyCompositorForShutdown() = 0;
};

// This class owns a DelegatedFrameHost, and will dynamically attach and
// detach it from a ui::Compositor as needed. The ui::Compositor will be
// detached from the DelegatedFrameHost when the following conditions are
// all met:
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
      const display::Display& initial_display,
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
  void OnDidNotProduceFrame(const viz::BeginFrameAck& ack);
  void SetBackgroundColor(SkColor background_color);
  void UpdateVSyncParameters(const base::TimeTicks& timebase,
                             const base::TimeDelta& interval);
  void SetNeedsBeginFrames(bool needs_begin_frames);
  void SetWantsAnimateOnlyBeginFrames();

  // Update the renderer's SurfaceId to reflect the current dimensions of the
  // NSView. This will allocate a new SurfaceId if needed. This will return
  // true if any properties that need to be communicated to the
  // RenderWidgetHostImpl have changed.
  bool UpdateNSViewAndDisplay(const gfx::Size& new_size_dip,
                              const display::Display& new_display);

  // Update the renderer's SurfaceId to reflect |size_dip| in anticipation of
  // the NSView resizing during auto-resize.
  void UpdateForAutoResize(const gfx::Size& size_dip);

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

  viz::FrameSinkId GetRootFrameSinkId();

  const gfx::Size& GetRendererSize() const { return dfh_size_dip_; }
  void GetRendererScreenInfo(ScreenInfo* screen_info) const;
  const viz::LocalSurfaceId& GetRendererLocalSurfaceId() const;

  // Indicate that the recyclable compositor should be destroyed, and no future
  // compositors should be recycled.
  static void DisableRecyclingForShutdown();

  // DelegatedFrameHostClient implementation.
  ui::Layer* DelegatedFrameHostGetLayer() const override;
  bool DelegatedFrameHostIsVisible() const override;
  SkColor DelegatedFrameHostGetGutterColor() const override;
  bool DelegatedFrameCanCreateResizeLock() const override;
  std::unique_ptr<CompositorResizeLock> DelegatedFrameHostCreateResizeLock()
      override;
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnBeginFrame(base::TimeTicks frame_time) override;
  bool IsAutoResizeEnabled() const override;
  void OnFrameTokenChanged(uint32_t frame_token) override;
  void DidReceiveFirstFrameAfterNavigation() override;

  base::WeakPtr<BrowserCompositorMac> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Returns nullptr if no compositor is attached.
  ui::Compositor* CompositorForTesting() const;

  void DidNavigate();

  void BeginPauseForFrame(bool auto_resize_enabled);
  void EndPauseForFrame();
  bool ShouldContinueToPauseForFrame() const;

 private:
  // The state of |delegated_frame_host_| and |recyclable_compositor_| to
  // manage being visible, hidden, or occluded.
  enum State {
    // Effects:
    // - |recyclable_compositor_| exists and is attached to
    //   |delegated_frame_host_|.
    // Happens when:
    // - |render_widet_host_| is in the visible state.
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
  bool render_widget_host_is_hidden_ = true;
  bool ns_view_attached_to_window_ = false;

  BrowserCompositorMacClient* client_ = nullptr;
  ui::AcceleratedWidgetMacNSView* accelerated_widget_mac_ns_view_ = nullptr;
  std::unique_ptr<RecyclableCompositorMac> recyclable_compositor_;

  std::unique_ptr<DelegatedFrameHost> delegated_frame_host_;
  std::unique_ptr<ui::Layer> root_layer_;

  SkColor background_color_ = SK_ColorWHITE;
  viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink_ =
      nullptr;

  // The viz::ParentLocalSurfaceIdAllocator for the delegated frame host
  // dispenses viz::LocalSurfaceIds that are renderered into by the renderer
  // process.
  viz::ParentLocalSurfaceIdAllocator dfh_local_surface_id_allocator_;
  gfx::Size dfh_size_pixels_;
  gfx::Size dfh_size_dip_;
  display::Display dfh_display_;

  // The viz::ParentLocalSurfaceIdAllocator for the ui::Compositor dispenses
  // viz::LocalSurfaceIds that are renderered into by the ui::Compositor.
  viz::ParentLocalSurfaceIdAllocator compositor_local_surface_id_allocator_;
  gfx::Size compositor_size_pixels_;
  float compositor_scale_factor_ = 1.f;

  // Used to disable screen updates while resizing (because frames are drawn in
  // the GPU process, they can end up appearing on-screen before our window
  // resizes).
  enum class RepaintState {
    // No repaint in progress.
    None,
    // Synchronously waiting for a new frame.
    Paused,
    // Screen updates are disabled while a new frame is swapped in.
    ScreenUpdatesDisabled,
  } repaint_state_ = RepaintState::None;
  bool repaint_auto_resize_enabled_ = false;

  base::WeakPtrFactory<BrowserCompositorMac> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BROWSER_COMPOSITOR_VIEW_MAC_H_
