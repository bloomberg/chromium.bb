// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_view_host.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/common/frame_messages.h"
#include "content/common/site_isolation_policy.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/page_state.h"
#include "content/public/common/web_preferences.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "media/base/video_frame.h"
#include "ui/gfx/geometry/rect.h"

namespace {
static uint32_t s_next_surface_id_namespace = 1;
}

namespace content {

void InitNavigateParams(FrameHostMsg_DidCommitProvisionalLoad_Params* params,
                        int page_id,
                        int nav_entry_id,
                        bool did_create_new_entry,
                        const GURL& url,
                        ui::PageTransition transition) {
  params->page_id = page_id;
  params->nav_entry_id = nav_entry_id;
  params->url = url;
  params->referrer = Referrer();
  params->transition = transition;
  params->redirects = std::vector<GURL>();
  params->should_update_history = false;
  params->searchable_form_url = GURL();
  params->searchable_form_encoding = std::string();
  params->did_create_new_entry = did_create_new_entry;
  params->security_info = std::string();
  params->gesture = NavigationGestureUser;
  params->was_within_same_page = false;
  params->method = "GET";
  params->page_state = PageState::CreateFromURL(url);
}

TestRenderWidgetHostView::TestRenderWidgetHostView(RenderWidgetHost* rwh)
    : rwh_(RenderWidgetHostImpl::From(rwh)),
      surface_id_namespace_(s_next_surface_id_namespace++),
      is_showing_(false),
      is_occluded_(false),
      did_swap_compositor_frame_(false) {
  rwh_->SetView(this);
}

TestRenderWidgetHostView::~TestRenderWidgetHostView() {
}

RenderWidgetHost* TestRenderWidgetHostView::GetRenderWidgetHost() const {
  return rwh_;
}

gfx::Vector2dF TestRenderWidgetHostView::GetLastScrollOffset() const {
  return gfx::Vector2dF();
}

gfx::NativeView TestRenderWidgetHostView::GetNativeView() const {
  return nullptr;
}

gfx::NativeViewAccessible TestRenderWidgetHostView::GetNativeViewAccessible() {
  return nullptr;
}

ui::TextInputClient* TestRenderWidgetHostView::GetTextInputClient() {
  return &text_input_client_;
}

bool TestRenderWidgetHostView::HasFocus() const {
  return true;
}

bool TestRenderWidgetHostView::IsSurfaceAvailableForCopy() const {
  return true;
}

void TestRenderWidgetHostView::Show() {
  is_showing_ = true;
  is_occluded_ = false;
}

void TestRenderWidgetHostView::Hide() {
  is_showing_ = false;
}

bool TestRenderWidgetHostView::IsShowing() {
  return is_showing_;
}

void TestRenderWidgetHostView::WasUnOccluded() {
  is_occluded_ = false;
}

void TestRenderWidgetHostView::WasOccluded() {
  is_occluded_ = true;
}

void TestRenderWidgetHostView::RenderProcessGone(base::TerminationStatus status,
                                                 int error_code) {
  delete this;
}

void TestRenderWidgetHostView::Destroy() { delete this; }

gfx::Rect TestRenderWidgetHostView::GetViewBounds() const {
  return gfx::Rect();
}

void TestRenderWidgetHostView::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const ReadbackRequestCallback& callback,
    const SkColorType preferred_color_type) {
  callback.Run(SkBitmap(), content::READBACK_FAILED);
}

void TestRenderWidgetHostView::CopyFromCompositingSurfaceToVideoFrame(
    const gfx::Rect& src_subrect,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(const gfx::Rect&, bool)>& callback) {
  callback.Run(gfx::Rect(), false);
}

bool TestRenderWidgetHostView::CanCopyToVideoFrame() const {
  return false;
}

bool TestRenderWidgetHostView::HasAcceleratedSurface(
      const gfx::Size& desired_size) {
  return false;
}

#if defined(OS_MACOSX)

void TestRenderWidgetHostView::SetActive(bool active) {
  // <viettrungluu@gmail.com>: Do I need to do anything here?
}

bool TestRenderWidgetHostView::SupportsSpeech() const {
  return false;
}

void TestRenderWidgetHostView::SpeakSelection() {
}

bool TestRenderWidgetHostView::IsSpeaking() const {
  return false;
}

void TestRenderWidgetHostView::StopSpeaking() {
}

#endif

bool TestRenderWidgetHostView::GetScreenColorProfile(
    std::vector<char>* color_profile) {
  DCHECK(color_profile->empty());
  return false;
}

gfx::Rect TestRenderWidgetHostView::GetBoundsInRootWindow() {
  return gfx::Rect();
}

void TestRenderWidgetHostView::OnSwapCompositorFrame(
    uint32_t output_surface_id,
    std::unique_ptr<cc::CompositorFrame> frame) {
  did_swap_compositor_frame_ = true;
}

bool TestRenderWidgetHostView::LockMouse() {
  return false;
}

void TestRenderWidgetHostView::UnlockMouse() {
}

uint32_t TestRenderWidgetHostView::GetSurfaceIdNamespace() {
  return surface_id_namespace_;
}

TestRenderViewHost::TestRenderViewHost(
    SiteInstance* instance,
    std::unique_ptr<RenderWidgetHostImpl> widget,
    RenderViewHostDelegate* delegate,
    int32_t main_frame_routing_id,
    bool swapped_out)
    : RenderViewHostImpl(instance,
                         std::move(widget),
                         delegate,
                         main_frame_routing_id,
                         swapped_out,
                         false /* has_initialized_audio_host */),
      delete_counter_(nullptr),
      opener_frame_route_id_(MSG_ROUTING_NONE) {
  // TestRenderWidgetHostView installs itself into this->view_ in its
  // constructor, and deletes itself when TestRenderWidgetHostView::Destroy() is
  // called.
  new TestRenderWidgetHostView(GetWidget());
}

TestRenderViewHost::~TestRenderViewHost() {
  if (delete_counter_)
    ++*delete_counter_;
}

bool TestRenderViewHost::CreateTestRenderView(
    const base::string16& frame_name,
    int opener_frame_route_id,
    int proxy_route_id,
    int32_t max_page_id,
    bool window_was_created_with_opener) {
  FrameReplicationState replicated_state;
  replicated_state.name = base::UTF16ToUTF8(frame_name);
  return CreateRenderView(opener_frame_route_id, proxy_route_id, max_page_id,
                          replicated_state, window_was_created_with_opener);
}

bool TestRenderViewHost::CreateRenderView(
    int opener_frame_route_id,
    int proxy_route_id,
    int32_t max_page_id,
    const FrameReplicationState& replicated_frame_state,
    bool window_was_created_with_opener) {
  DCHECK(!IsRenderViewLive());
  GetWidget()->set_renderer_initialized(true);
  DCHECK(IsRenderViewLive());
  opener_frame_route_id_ = opener_frame_route_id;
  RenderFrameHost* main_frame = GetMainFrame();
  if (main_frame)
    static_cast<RenderFrameHostImpl*>(main_frame)->SetRenderFrameCreated(true);

  return true;
}

MockRenderProcessHost* TestRenderViewHost::GetProcess() const {
  return static_cast<MockRenderProcessHost*>(RenderViewHostImpl::GetProcess());
}

void TestRenderViewHost::SimulateWasHidden() {
  GetWidget()->WasHidden();
}

void TestRenderViewHost::SimulateWasShown() {
  GetWidget()->WasShown(ui::LatencyInfo());
}

WebPreferences TestRenderViewHost::TestComputeWebkitPrefs() {
  return ComputeWebkitPrefs();
}

void TestRenderViewHost::TestOnStartDragging(
    const DropData& drop_data) {
  blink::WebDragOperationsMask drag_operation = blink::WebDragOperationEvery;
  DragEventSourceInfo event_info;
  OnStartDragging(drop_data, drag_operation, SkBitmap(), gfx::Vector2d(),
                  event_info);
}

void TestRenderViewHost::TestOnUpdateStateWithFile(
    int page_id,
    const base::FilePath& file_path) {
  PageState state = PageState::CreateForTesting(GURL("http://www.google.com"),
                                                false, "data", &file_path);
  if (SiteIsolationPolicy::UseSubframeNavigationEntries()) {
    static_cast<RenderFrameHostImpl*>(GetMainFrame())->OnUpdateState(state);
  } else {
    OnUpdateState(page_id, state);
  }
}

RenderViewHostImplTestHarness::RenderViewHostImplTestHarness() {
  std::vector<ui::ScaleFactor> scale_factors;
  scale_factors.push_back(ui::SCALE_FACTOR_100P);
  scoped_set_supported_scale_factors_.reset(
      new ui::test::ScopedSetSupportedScaleFactors(scale_factors));
}

RenderViewHostImplTestHarness::~RenderViewHostImplTestHarness() {
}

TestRenderViewHost* RenderViewHostImplTestHarness::test_rvh() {
  return contents()->GetRenderViewHost();
}

TestRenderViewHost* RenderViewHostImplTestHarness::pending_test_rvh() {
  return contents()->GetPendingMainFrame() ?
      contents()->GetPendingMainFrame()->GetRenderViewHost() :
      nullptr;
}

TestRenderViewHost* RenderViewHostImplTestHarness::active_test_rvh() {
  return static_cast<TestRenderViewHost*>(active_rvh());
}

TestRenderFrameHost* RenderViewHostImplTestHarness::main_test_rfh() {
  return contents()->GetMainFrame();
}

TestWebContents* RenderViewHostImplTestHarness::contents() {
  return static_cast<TestWebContents*>(web_contents());
}

}  // namespace content
