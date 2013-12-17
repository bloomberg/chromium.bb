// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_render_view_host.h"

#include "base/memory/scoped_ptr.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/page_state.h"
#include "content/test/test_backing_store.h"
#include "content/test/test_web_contents.h"
#include "media/base/video_frame.h"
#include "ui/gfx/rect.h"
#include "webkit/common/webpreferences.h"

namespace content {

namespace {

const int64 kFrameId = 13UL;

}  // namespace


void InitNavigateParams(ViewHostMsg_FrameNavigate_Params* params,
                        int page_id,
                        const GURL& url,
                        PageTransition transition) {
  params->page_id = page_id;
  params->url = url;
  params->referrer = Referrer();
  params->transition = transition;
  params->redirects = std::vector<GURL>();
  params->should_update_history = false;
  params->searchable_form_url = GURL();
  params->searchable_form_encoding = std::string();
  params->security_info = std::string();
  params->gesture = NavigationGestureUser;
  params->was_within_same_page = false;
  params->is_post = false;
  params->page_state = PageState::CreateFromURL(url);
}

TestRenderWidgetHostView::TestRenderWidgetHostView(RenderWidgetHost* rwh)
    : rwh_(RenderWidgetHostImpl::From(rwh)),
      is_showing_(false),
      did_swap_compositor_frame_(false) {
  rwh_->SetView(this);
}

TestRenderWidgetHostView::~TestRenderWidgetHostView() {
}

RenderWidgetHost* TestRenderWidgetHostView::GetRenderWidgetHost() const {
  return NULL;
}

gfx::NativeView TestRenderWidgetHostView::GetNativeView() const {
  return NULL;
}

gfx::NativeViewId TestRenderWidgetHostView::GetNativeViewId() const {
  return 0;
}

gfx::NativeViewAccessible TestRenderWidgetHostView::GetNativeViewAccessible() {
  return NULL;
}

bool TestRenderWidgetHostView::HasFocus() const {
  return true;
}

bool TestRenderWidgetHostView::IsSurfaceAvailableForCopy() const {
  return true;
}

void TestRenderWidgetHostView::Show() {
  is_showing_ = true;
}

void TestRenderWidgetHostView::Hide() {
  is_showing_ = false;
}

bool TestRenderWidgetHostView::IsShowing() {
  return is_showing_;
}

void TestRenderWidgetHostView::RenderProcessGone(base::TerminationStatus status,
                                                 int error_code) {
  delete this;
}

void TestRenderWidgetHostView::Destroy() { delete this; }

gfx::Rect TestRenderWidgetHostView::GetViewBounds() const {
  return gfx::Rect();
}

BackingStore* TestRenderWidgetHostView::AllocBackingStore(
    const gfx::Size& size) {
  return new TestBackingStore(rwh_, size);
}

void TestRenderWidgetHostView::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const base::Callback<void(bool, const SkBitmap&)>& callback) {
  callback.Run(false, SkBitmap());
}

void TestRenderWidgetHostView::CopyFromCompositingSurfaceToVideoFrame(
    const gfx::Rect& src_subrect,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(bool)>& callback) {
  callback.Run(false);
}

bool TestRenderWidgetHostView::CanCopyToVideoFrame() const {
  return false;
}

void TestRenderWidgetHostView::OnAcceleratedCompositingStateChange() {
}

void TestRenderWidgetHostView::AcceleratedSurfaceInitialized(int host_id,
                                                             int route_id) {
}

void TestRenderWidgetHostView::AcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
    int gpu_host_id) {
}

void TestRenderWidgetHostView::AcceleratedSurfacePostSubBuffer(
    const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
    int gpu_host_id) {
}

void TestRenderWidgetHostView::AcceleratedSurfaceSuspend() {
}

bool TestRenderWidgetHostView::HasAcceleratedSurface(
      const gfx::Size& desired_size) {
  return false;
}

#if defined(OS_MACOSX)

void TestRenderWidgetHostView::AboutToWaitForBackingStoreMsg() {
}

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

bool TestRenderWidgetHostView::PostProcessEventForPluginIme(
    const NativeWebKeyboardEvent& event) {
  return false;
}

#elif defined(OS_WIN) && !defined(USE_AURA)
void TestRenderWidgetHostView::WillWmDestroy() {
}
#endif

gfx::Rect TestRenderWidgetHostView::GetBoundsInRootWindow() {
  return gfx::Rect();
}

#if defined(TOOLKIT_GTK)
GdkEventButton* TestRenderWidgetHostView::GetLastMouseDown() {
  return NULL;
}

gfx::NativeView TestRenderWidgetHostView::BuildInputMethodsGtkMenu() {
  return NULL;
}
#endif  // defined(TOOLKIT_GTK)

void TestRenderWidgetHostView::OnSwapCompositorFrame(
    uint32 output_surface_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  did_swap_compositor_frame_ = true;
}


gfx::GLSurfaceHandle TestRenderWidgetHostView::GetCompositingSurface() {
  return gfx::GLSurfaceHandle();
}

#if defined(OS_WIN) && !defined(USE_AURA)
void TestRenderWidgetHostView::SetClickthroughRegion(SkRegion* region) {
}
#endif

bool TestRenderWidgetHostView::LockMouse() {
  return false;
}

void TestRenderWidgetHostView::UnlockMouse() {
}

#if defined(OS_WIN) && defined(USE_AURA)
void TestRenderWidgetHostView::SetParentNativeViewAccessible(
    gfx::NativeViewAccessible accessible_parent) {
}

gfx::NativeViewId TestRenderWidgetHostView::GetParentForWindowlessPlugin()
    const {
  return 0;
}
#endif

TestRenderViewHost::TestRenderViewHost(
    SiteInstance* instance,
    RenderViewHostDelegate* delegate,
    RenderFrameHostDelegate* frame_delegate,
    RenderWidgetHostDelegate* widget_delegate,
    int routing_id,
    int main_frame_routing_id,
    bool swapped_out)
    : RenderViewHostImpl(instance,
                         delegate,
                         frame_delegate,
                         widget_delegate,
                         routing_id,
                         main_frame_routing_id,
                         swapped_out,
                         false /* hidden */),
      render_view_created_(false),
      delete_counter_(NULL),
      simulate_fetch_via_proxy_(false),
      simulate_history_list_was_cleared_(false),
      contents_mime_type_("text/html"),
      opener_route_id_(MSG_ROUTING_NONE) {
  // TestRenderWidgetHostView installs itself into this->view_ in its
  // constructor, and deletes itself when TestRenderWidgetHostView::Destroy() is
  // called.
  new TestRenderWidgetHostView(this);

  main_frame_id_ = kFrameId;
}

TestRenderViewHost::~TestRenderViewHost() {
  if (delete_counter_)
    ++*delete_counter_;
}

bool TestRenderViewHost::CreateRenderView(
    const base::string16& frame_name,
    int opener_route_id,
    int32 max_page_id) {
  DCHECK(!render_view_created_);
  render_view_created_ = true;
  opener_route_id_ = opener_route_id;
  return true;
}

bool TestRenderViewHost::IsRenderViewLive() const {
  return render_view_created_;
}

void TestRenderViewHost::SendNavigate(int page_id, const GURL& url) {
  SendNavigateWithTransition(page_id, url, PAGE_TRANSITION_LINK);
}

void TestRenderViewHost::SendFailedNavigate(int page_id, const GURL& url) {
  SendNavigateWithTransitionAndResponseCode(
      page_id, url, PAGE_TRANSITION_LINK, 500);
}

void TestRenderViewHost::SendNavigateWithTransition(
    int page_id, const GURL& url, PageTransition transition) {
  SendNavigateWithTransitionAndResponseCode(page_id, url, transition, 200);
}

void TestRenderViewHost::SendNavigateWithOriginalRequestURL(
    int page_id, const GURL& url, const GURL& original_request_url) {
  main_render_frame_host()->OnDidStartProvisionalLoadForFrame(
      kFrameId, -1, true, url);
  SendNavigateWithParameters(page_id, url, PAGE_TRANSITION_LINK,
                             original_request_url, 200, 0);
}

void TestRenderViewHost::SendNavigateWithFile(
    int page_id, const GURL& url, const base::FilePath& file_path) {
  SendNavigateWithParameters(page_id, url, PAGE_TRANSITION_LINK,
                             url, 200, &file_path);
}

void TestRenderViewHost::SendNavigateWithParams(
    ViewHostMsg_FrameNavigate_Params* params) {
  params->frame_id = kFrameId;
  ViewHostMsg_FrameNavigate msg(1, *params);
  OnNavigate(msg);
}

void TestRenderViewHost::SendNavigateWithTransitionAndResponseCode(
    int page_id, const GURL& url, PageTransition transition,
    int response_code) {
  // DidStartProvisionalLoad may delete the pending entry that holds |url|,
  // so we keep a copy of it to use in SendNavigateWithParameters.
  GURL url_copy(url);
  main_render_frame_host()->OnDidStartProvisionalLoadForFrame(
      kFrameId, -1, true, url_copy);
  SendNavigateWithParameters(page_id, url_copy, transition, url_copy,
                             response_code, 0);
}

void TestRenderViewHost::SendNavigateWithParameters(
    int page_id, const GURL& url, PageTransition transition,
    const GURL& original_request_url, int response_code,
    const base::FilePath* file_path_for_history_item) {
  ViewHostMsg_FrameNavigate_Params params;
  params.page_id = page_id;
  params.frame_id = kFrameId;
  params.url = url;
  params.referrer = Referrer();
  params.transition = transition;
  params.redirects = std::vector<GURL>();
  params.should_update_history = true;
  params.searchable_form_url = GURL();
  params.searchable_form_encoding = std::string();
  params.security_info = std::string();
  params.gesture = NavigationGestureUser;
  params.contents_mime_type = contents_mime_type_;
  params.is_post = false;
  params.was_within_same_page = false;
  params.http_status_code = response_code;
  params.socket_address.set_host("2001:db8::1");
  params.socket_address.set_port(80);
  params.was_fetched_via_proxy = simulate_fetch_via_proxy_;
  params.history_list_was_cleared = simulate_history_list_was_cleared_;
  params.original_request_url = original_request_url;

  params.page_state = PageState::CreateForTesting(
      url,
      false,
      file_path_for_history_item ? "data" : NULL,
      file_path_for_history_item);

  ViewHostMsg_FrameNavigate msg(1, params);
  OnNavigate(msg);
}

void TestRenderViewHost::SendShouldCloseACK(bool proceed) {
  base::TimeTicks now = base::TimeTicks::Now();
  OnShouldCloseACK(proceed, now, now);
}

void TestRenderViewHost::SetContentsMimeType(const std::string& mime_type) {
  contents_mime_type_ = mime_type;
}

void TestRenderViewHost::SimulateSwapOutACK() {
  OnSwappedOut(false);
}

void TestRenderViewHost::SimulateWasHidden() {
  WasHidden();
}

void TestRenderViewHost::SimulateWasShown() {
  WasShown();
}

void TestRenderViewHost::TestOnStartDragging(
    const DropData& drop_data) {
  blink::WebDragOperationsMask drag_operation = blink::WebDragOperationEvery;
  DragEventSourceInfo event_info;
  OnStartDragging(drop_data, drag_operation, SkBitmap(), gfx::Vector2d(),
                  event_info);
}

void TestRenderViewHost::TestOnUpdateStateWithFile(
    int process_id,
    const base::FilePath& file_path) {
  OnUpdateState(process_id,
                PageState::CreateForTesting(GURL("http://www.google.com"),
                                            false,
                                            "data",
                                            &file_path));
}

void TestRenderViewHost::set_simulate_fetch_via_proxy(bool proxy) {
  simulate_fetch_via_proxy_ = proxy;
}

void TestRenderViewHost::set_simulate_history_list_was_cleared(bool cleared) {
  simulate_history_list_was_cleared_ = cleared;
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
  return static_cast<TestRenderViewHost*>(rvh());
}

TestRenderViewHost* RenderViewHostImplTestHarness::pending_test_rvh() {
  return static_cast<TestRenderViewHost*>(pending_rvh());
}

TestRenderViewHost* RenderViewHostImplTestHarness::active_test_rvh() {
  return static_cast<TestRenderViewHost*>(active_rvh());
}

TestRenderFrameHost* RenderViewHostImplTestHarness::main_test_rfh() {
  return static_cast<TestRenderFrameHost*>(main_rfh());
}

TestWebContents* RenderViewHostImplTestHarness::contents() {
  return static_cast<TestWebContents*>(web_contents());
}

}  // namespace content
