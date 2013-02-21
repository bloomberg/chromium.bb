// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_TEST_RENDER_VIEW_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_TEST_RENDER_VIEW_HOST_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/test/test_renderer_host.h"
#include "ui/gfx/vector2d_f.h"

// This file provides a testing framework for mocking out the RenderProcessHost
// layer. It allows you to test RenderViewHost, WebContentsImpl,
// NavigationController, and other layers above that without running an actual
// renderer process.
//
// To use, derive your test base class from RenderViewHostImplTestHarness.

struct ViewHostMsg_FrameNavigate_Params;

namespace gfx {
class Rect;
}

namespace content {

class SiteInstance;
class TestWebContents;

// Utility function to initialize ViewHostMsg_NavigateParams_Params
// with given |page_id|, |url| and |transition_type|.
void InitNavigateParams(ViewHostMsg_FrameNavigate_Params* params,
                        int page_id,
                        const GURL& url,
                        PageTransition transition_type);

// TestRenderViewHostView ------------------------------------------------------

// Subclass the RenderViewHost's view so that we can call Show(), etc.,
// without having side-effects.
class TestRenderWidgetHostView : public RenderWidgetHostViewBase {
 public:
  explicit TestRenderWidgetHostView(RenderWidgetHost* rwh);
  virtual ~TestRenderWidgetHostView();

  // RenderWidgetHostView implementation.
  virtual void InitAsChild(gfx::NativeView parent_view) OVERRIDE {}
  virtual RenderWidgetHost* GetRenderWidgetHost() const OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE {}
  virtual void SetBounds(const gfx::Rect& rect) OVERRIDE {}
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeViewId GetNativeViewId() const OVERRIDE;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() OVERRIDE;
  virtual bool HasFocus() const OVERRIDE;
  virtual bool IsSurfaceAvailableForCopy() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsShowing() OVERRIDE;
  virtual gfx::Rect GetViewBounds() const OVERRIDE;
#if defined(OS_MACOSX)
  virtual void SetActive(bool active) OVERRIDE;
  virtual void SetTakesFocusOnlyOnMouseDown(bool flag) OVERRIDE {}
  virtual void SetWindowVisibility(bool visible) OVERRIDE {}
  virtual void WindowFrameChanged() OVERRIDE {}
  virtual void ShowDefinitionForSelection() OVERRIDE {}
  virtual bool SupportsSpeech() const OVERRIDE;
  virtual void SpeakSelection() OVERRIDE;
  virtual bool IsSpeaking() const OVERRIDE;
  virtual void StopSpeaking() OVERRIDE;
#endif  // defined(OS_MACOSX)
#if defined(TOOLKIT_GTK)
  virtual GdkEventButton* GetLastMouseDown() OVERRIDE;
  virtual gfx::NativeView BuildInputMethodsGtkMenu() OVERRIDE;
#endif  // defined(TOOLKIT_GTK)

  // RenderWidgetHostViewPort implementation.
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) OVERRIDE {}
  virtual void InitAsFullscreen(
      RenderWidgetHostView* reference_host_view) OVERRIDE {}
  virtual void WasShown() OVERRIDE {}
  virtual void WasHidden() OVERRIDE {}
  virtual void MovePluginWindows(
      const gfx::Vector2d& scroll_offset,
      const std::vector<webkit::npapi::WebPluginGeometry>& moves) OVERRIDE {}
  virtual void Focus() OVERRIDE {}
  virtual void Blur() OVERRIDE {}
  virtual void SetIsLoading(bool is_loading) OVERRIDE {}
  virtual void UpdateCursor(const WebCursor& cursor) OVERRIDE {}
  virtual void TextInputStateChanged(
      const ViewHostMsg_TextInputState_Params& params) OVERRIDE {}
  virtual void ImeCancelComposition() OVERRIDE {}
  virtual void ImeCompositionRangeChanged(
      const ui::Range& range,
      const std::vector<gfx::Rect>& character_bounds) OVERRIDE {}
  virtual void DidUpdateBackingStore(
      const gfx::Rect& scroll_rect,
      const gfx::Vector2d& scroll_delta,
      const std::vector<gfx::Rect>& rects) OVERRIDE {}
  virtual void RenderViewGone(base::TerminationStatus status,
                              int error_code) OVERRIDE;
  virtual void WillDestroyRenderWidget(RenderWidgetHost* rwh) { }
  virtual void Destroy() OVERRIDE {}
  virtual void SetTooltipText(const string16& tooltip_text) OVERRIDE {}
  virtual void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) OVERRIDE {}
  virtual void ScrollOffsetChanged() OVERRIDE {}
  virtual BackingStore* AllocBackingStore(const gfx::Size& size) OVERRIDE;
  virtual void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback) OVERRIDE;
  virtual void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) OVERRIDE;
  virtual bool CanCopyToVideoFrame() const OVERRIDE;
  virtual void OnAcceleratedCompositingStateChange() OVERRIDE;
  virtual void AcceleratedSurfaceBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfacePostSubBuffer(
      const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfaceSuspend() OVERRIDE;
  virtual void AcceleratedSurfaceRelease() OVERRIDE {}
  virtual bool HasAcceleratedSurface(const gfx::Size& desired_size) OVERRIDE;
#if defined(OS_MACOSX)
  virtual void AboutToWaitForBackingStoreMsg() OVERRIDE;
  virtual bool PostProcessEventForPluginIme(
      const NativeWebKeyboardEvent& event) OVERRIDE;
#elif defined(OS_ANDROID)
  virtual void ShowDisambiguationPopup(
      const gfx::Rect& target_rect,
      const SkBitmap& zoomed_bitmap) OVERRIDE {}
  virtual void UpdateFrameInfo(const gfx::Vector2d& scroll_offset,
                               float page_scale_factor,
                               float min_page_scale_factor,
                               float max_page_scale_factor,
                               const gfx::Size& content_size,
                               const gfx::Vector2dF& controls_offset,
                               const gfx::Vector2dF& content_offset) OVERRIDE {}
  virtual void HasTouchEventHandlers(bool need_touch_events) OVERRIDE {}
#elif defined(OS_WIN) && !defined(USE_AURA)
  virtual void WillWmDestroy() OVERRIDE;
#endif
  virtual void GetScreenInfo(WebKit::WebScreenInfo* results) OVERRIDE {}
  virtual gfx::Rect GetBoundsInRootWindow() OVERRIDE;
  virtual void SetHasHorizontalScrollbar(
      bool has_horizontal_scrollbar) OVERRIDE { }
  virtual void SetScrollOffsetPinning(
      bool is_pinned_to_left, bool is_pinned_to_right) OVERRIDE { }
  virtual void OnAccessibilityNotifications(
      const std::vector<AccessibilityHostMsg_NotificationParams>&
          params) OVERRIDE {}
  virtual gfx::GLSurfaceHandle GetCompositingSurface() OVERRIDE;
#if defined(OS_WIN) && !defined(USE_AURA)
  virtual void SetClickthroughRegion(SkRegion* region) OVERRIDE;
#endif
  virtual bool LockMouse() OVERRIDE;
  virtual void UnlockMouse() OVERRIDE;

  bool is_showing() const { return is_showing_; }

 protected:
  RenderWidgetHostImpl* rwh_;

 private:
  bool is_showing_;
};

#if defined(COMPILER_MSVC)
// See comment for same warning on RenderViewHostImpl.
#pragma warning(push)
#pragma warning(disable: 4250)
#endif

// TestRenderViewHost ----------------------------------------------------------

// TODO(brettw) this should use a TestWebContents which should be generalized
// from the WebContentsImpl test. We will probably also need that class' version
// of CreateRenderViewForRenderManager when more complicated tests start using
// this.
//
// Note that users outside of content must use this class by getting
// the separate RenderViewHostTester interface via
// RenderViewHostTester::For(rvh) on the RenderViewHost they want to
// drive tests on.
//
// Users within content may directly static_cast from a
// RenderViewHost* to a TestRenderViewHost*.
//
// The reasons we do it this way rather than extending the parallel
// inheritance hierarchy we have for RenderWidgetHost/RenderViewHost
// vs. RenderWidgetHostImpl/RenderViewHostImpl are:
//
// a) Extending the parallel class hierarchy further would require
// more classes to use virtual inheritance.  This is a complexity that
// is better to avoid, especially when it would be introduced in the
// production code solely to facilitate testing code.
//
// b) While users outside of content only need to drive tests on a
// RenderViewHost, content needs a test version of the full
// RenderViewHostImpl so that it can test all methods on that concrete
// class (e.g. overriding a method such as
// RenderViewHostImpl::CreateRenderView).  This would have complicated
// the dual class hierarchy even further.
//
// The reason we do it this way instead of using composition is
// similar to (b) above, essentially it gets very tricky.  By using
// the split interface we avoid complexity within content and maintain
// reasonable utility for embedders.
class TestRenderViewHost
    : public RenderViewHostImpl,
      public RenderViewHostTester {
 public:
  TestRenderViewHost(SiteInstance* instance,
                     RenderViewHostDelegate* delegate,
                     RenderWidgetHostDelegate* widget_delegate,
                     int routing_id,
                     bool swapped_out);
  virtual ~TestRenderViewHost();

  // RenderViewHostTester implementation.  Note that CreateRenderView
  // is not specified since it is synonymous with the one from
  // RenderViewHostImpl, see below.
  virtual void SendNavigate(int page_id, const GURL& url) OVERRIDE;
  virtual void SendNavigateWithTransition(int page_id, const GURL& url,
                                          PageTransition transition) OVERRIDE;
  virtual void SendShouldCloseACK(bool proceed) OVERRIDE;
  virtual void SetContentsMimeType(const std::string& mime_type) OVERRIDE;
  virtual void SimulateSwapOutACK() OVERRIDE;
  virtual void SimulateWasHidden() OVERRIDE;
  virtual void SimulateWasShown() OVERRIDE;

  // Calls OnNavigate on the RenderViewHost with the given information,
  // including a custom original request URL.  Sets the rest of the
  // parameters in the message to the "typical" values.  This is a helper
  // function for simulating the most common types of loads.
  void SendNavigateWithOriginalRequestURL(
      int page_id, const GURL& url, const GURL& original_request_url);

  // Calls OnNavigate on the RenderViewHost with the given information.
  // Sets the rest of the parameters in the message to the "typical" values.
  // This is a helper function for simulating the most common types of loads.
  void SendNavigateWithParameters(
      int page_id, const GURL& url, PageTransition transition,
      const GURL& original_request_url);

  void TestOnStartDragging(const WebDropData& drop_data);

  // If set, *delete_counter is incremented when this object destructs.
  void set_delete_counter(int* delete_counter) {
    delete_counter_ = delete_counter;
  }

  // Sets whether the RenderView currently exists or not. This controls the
  // return value from IsRenderViewLive, which the rest of the system uses to
  // check whether the RenderView has crashed or not.
  void set_render_view_created(bool created) {
    render_view_created_ = created;
  }

  // Returns whether the RenderViewHost is currently waiting to hear the result
  // of a before unload handler from the renderer.
  bool is_waiting_for_beforeunload_ack() const {
    return is_waiting_for_beforeunload_ack_;
  }

  // Returns whether the RenderViewHost is currently waiting to hear the result
  // of an unload handler from the renderer.
  bool is_waiting_for_unload_ack() const {
    return is_waiting_for_unload_ack_;
  }

  // Sets whether the RenderViewHost is currently swapped out, and thus
  // filtering messages from the renderer.
  void set_is_swapped_out(bool is_swapped_out) {
    is_swapped_out_ = is_swapped_out;
  }

  // If set, navigations will appear to have loaded through a proxy
  // (ViewHostMsg_FrameNavigte_Params::was_fetched_via_proxy).
  // False by default.
  void set_simulate_fetch_via_proxy(bool proxy);

  // RenderViewHost overrides --------------------------------------------------

  virtual bool CreateRenderView(const string16& frame_name,
                                int opener_route_id,
                                int32 max_page_id) OVERRIDE;
  virtual bool IsRenderViewLive() const OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostTest, FilterNavigate);

  // Tracks if the caller thinks if it created the RenderView. This is so we can
  // respond to IsRenderViewLive appropriately.
  bool render_view_created_;

  // See set_delete_counter() above. May be NULL.
  int* delete_counter_;

  // See set_simulate_fetch_via_proxy() above.
  bool simulate_fetch_via_proxy_;

  // See SetContentsMimeType() above.
  std::string contents_mime_type_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderViewHost);
};

#if defined(COMPILER_MSVC)
#pragma warning(pop)
#endif

// Adds methods to get straight at the impl classes.
class RenderViewHostImplTestHarness : public RenderViewHostTestHarness {
 public:
  RenderViewHostImplTestHarness();
  virtual ~RenderViewHostImplTestHarness();

  TestRenderViewHost* test_rvh();
  TestRenderViewHost* pending_test_rvh();
  TestRenderViewHost* active_test_rvh();
  TestWebContents* contents();

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderViewHostImplTestHarness);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_TEST_RENDER_VIEW_HOST_H_
