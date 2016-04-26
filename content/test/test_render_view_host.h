// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDER_VIEW_HOST_H_
#define CONTENT_TEST_TEST_RENDER_VIEW_HOST_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/layout.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/vector2d_f.h"

// This file provides a testing framework for mocking out the RenderProcessHost
// layer. It allows you to test RenderViewHost, WebContentsImpl,
// NavigationController, and other layers above that without running an actual
// renderer process.
//
// To use, derive your test base class from RenderViewHostImplTestHarness.

struct FrameHostMsg_DidCommitProvisionalLoad_Params;

namespace gfx {
class Rect;
}

namespace content {

class SiteInstance;
class TestRenderFrameHost;
class TestWebContents;
struct FrameReplicationState;
struct TextInputState;

// Utility function to initialize FrameHostMsg_DidCommitProvisionalLoad_Params
// with given parameters.
void InitNavigateParams(FrameHostMsg_DidCommitProvisionalLoad_Params* params,
                        int page_id,
                        int nav_entry_id,
                        bool did_create_new_entry,
                        const GURL& url,
                        ui::PageTransition transition_type);

// TestRenderWidgetHostView ----------------------------------------------------

// Subclass the RenderViewHost's view so that we can call Show(), etc.,
// without having side-effects.
class TestRenderWidgetHostView : public RenderWidgetHostViewBase {
 public:
  explicit TestRenderWidgetHostView(RenderWidgetHost* rwh);
  ~TestRenderWidgetHostView() override;

  // RenderWidgetHostView implementation.
  void InitAsChild(gfx::NativeView parent_view) override {}
  RenderWidgetHost* GetRenderWidgetHost() const override;
  void SetSize(const gfx::Size& size) override {}
  void SetBounds(const gfx::Rect& rect) override {}
  gfx::Vector2dF GetLastScrollOffset() const override;
  gfx::NativeView GetNativeView() const override;
  gfx::NativeViewId GetNativeViewId() const override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  ui::TextInputClient* GetTextInputClient() override;
  bool HasFocus() const override;
  bool IsSurfaceAvailableForCopy() const override;
  void Show() override;
  void Hide() override;
  bool IsShowing() override;
  void WasUnOccluded() override;
  void WasOccluded() override;
  gfx::Rect GetViewBounds() const override;
#if defined(OS_MACOSX)
  void SetActive(bool active) override;
  void ShowDefinitionForSelection() override {}
  bool SupportsSpeech() const override;
  void SpeakSelection() override;
  bool IsSpeaking() const override;
  void StopSpeaking() override;
#endif  // defined(OS_MACOSX)
  void OnSwapCompositorFrame(
      uint32_t output_surface_id,
      std::unique_ptr<cc::CompositorFrame> frame) override;
  void ClearCompositorFrame() override {}

  // RenderWidgetHostViewBase implementation.
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& bounds) override {}
  void InitAsFullscreen(RenderWidgetHostView* reference_host_view) override {}
  void Focus() override {}
  void SetIsLoading(bool is_loading) override {}
  void UpdateCursor(const WebCursor& cursor) override {}
  void TextInputStateChanged(const TextInputState& params) override {}
  void ImeCancelComposition() override {}
  void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) override {}
  void RenderProcessGone(base::TerminationStatus status,
                         int error_code) override;
  void Destroy() override;
  void SetTooltipText(const base::string16& tooltip_text) override {}
  void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) override {}
  void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const ReadbackRequestCallback& callback,
      const SkColorType preferred_color_type) override;
  void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(const gfx::Rect&, bool)>& callback) override;
  bool CanCopyToVideoFrame() const override;
  bool HasAcceleratedSurface(const gfx::Size& desired_size) override;
  void LockCompositingSurface() override {}
  void UnlockCompositingSurface() override {}
  void GetScreenInfo(blink::WebScreenInfo* results) override {}
  bool GetScreenColorProfile(std::vector<char>* color_profile) override;
  gfx::Rect GetBoundsInRootWindow() override;
  bool LockMouse() override;
  void UnlockMouse() override;

  bool is_showing() const { return is_showing_; }
  bool is_occluded() const { return is_occluded_; }
  bool did_swap_compositor_frame() const { return did_swap_compositor_frame_; }

 protected:
  RenderWidgetHostImpl* rwh_;

 private:
  bool is_showing_;
  bool is_occluded_;
  bool did_swap_compositor_frame_;
  ui::DummyTextInputClient text_input_client_;
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
                     std::unique_ptr<RenderWidgetHostImpl> widget,
                     RenderViewHostDelegate* delegate,
                     int32_t main_frame_routing_id,
                     bool swapped_out);
  ~TestRenderViewHost() override;

  // RenderViewHostTester implementation.  Note that CreateRenderView
  // is not specified since it is synonymous with the one from
  // RenderViewHostImpl, see below.
  void SimulateWasHidden() override;
  void SimulateWasShown() override;
  WebPreferences TestComputeWebkitPrefs() override;

  void TestOnUpdateStateWithFile(
      int page_id, const base::FilePath& file_path);

  void TestOnStartDragging(const DropData& drop_data);

  // If set, *delete_counter is incremented when this object destructs.
  void set_delete_counter(int* delete_counter) {
    delete_counter_ = delete_counter;
  }

  // The opener frame route id passed to CreateRenderView().
  int opener_frame_route_id() const { return opener_frame_route_id_; }

  // RenderWidgetHost overrides (same value, but in the Mock* type)
  MockRenderProcessHost* GetProcess() const override;

  bool CreateTestRenderView(const base::string16& frame_name,
                            int opener_frame_route_id,
                            int proxy_route_id,
                            int32_t max_page_id,
                            bool window_was_created_with_opener) override;

  // RenderViewHost overrides --------------------------------------------------

  bool CreateRenderView(int opener_frame_route_id,
                        int proxy_route_id,
                        int32_t max_page_id,
                        const FrameReplicationState& replicated_frame_state,
                        bool window_was_created_with_opener) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostTest, FilterNavigate);

  void SendNavigateWithTransitionAndResponseCode(int page_id,
                                                 const GURL& url,
                                                 ui::PageTransition transition,
                                                 int response_code);

  // Calls OnNavigate on the RenderViewHost with the given information.
  // Sets the rest of the parameters in the message to the "typical" values.
  // This is a helper function for simulating the most common types of loads.
  void SendNavigateWithParameters(
      int page_id,
      const GURL& url,
      ui::PageTransition transition,
      const GURL& original_request_url,
      int response_code,
      const base::FilePath* file_path_for_history_item);

  // See set_delete_counter() above. May be NULL.
  int* delete_counter_;

  // See opener_frame_route_id() above.
  int opener_frame_route_id_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderViewHost);
};

#if defined(COMPILER_MSVC)
#pragma warning(pop)
#endif

// Adds methods to get straight at the impl classes.
class RenderViewHostImplTestHarness : public RenderViewHostTestHarness {
 public:
  RenderViewHostImplTestHarness();
  ~RenderViewHostImplTestHarness() override;

  // contents() is equivalent to static_cast<TestWebContents*>(web_contents())
  TestWebContents* contents();

  // RVH/RFH getters are shorthand for oft-used bits of web_contents().

  // test_rvh() is equivalent to any of the following:
  //   contents()->GetMainFrame()->GetRenderViewHost()
  //   contents()->GetRenderViewHost()
  //   static_cast<TestRenderViewHost*>(rvh())
  //
  // Since most functionality will eventually shift from RVH to RFH, you may
  // prefer to use the GetMainFrame() method in tests.
  TestRenderViewHost* test_rvh();

  // pending_test_rvh() is equivalent to all of the following:
  //   contents()->GetPendingMainFrame()->GetRenderViewHost() [if frame exists]
  //   contents()->GetPendingRenderViewHost()
  //   static_cast<TestRenderViewHost*>(pending_rvh())
  //
  // Since most functionality will eventually shift from RVH to RFH, you may
  // prefer to use the GetPendingMainFrame() method in tests.
  TestRenderViewHost* pending_test_rvh();

  // active_test_rvh() is equivalent to:
  //   contents()->GetPendingRenderViewHost() ?
  //        contents()->GetPendingRenderViewHost() :
  //        contents()->GetRenderViewHost();
  TestRenderViewHost* active_test_rvh();

  // main_test_rfh() is equivalent to contents()->GetMainFrame()
  // TODO(nick): Replace all uses with contents()->GetMainFrame()
  TestRenderFrameHost* main_test_rfh();

 private:
  typedef std::unique_ptr<ui::test::ScopedSetSupportedScaleFactors>
      ScopedSetSupportedScaleFactors;
  ScopedSetSupportedScaleFactors scoped_set_supported_scale_factors_;
  DISALLOW_COPY_AND_ASSIGN(RenderViewHostImplTestHarness);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDER_VIEW_HOST_H_
