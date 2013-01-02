// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/test_backing_store.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/navigation_controller_impl.h"
#include "content/browser/web_contents/test_web_contents.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/password_form.h"
#include "ui/gfx/rect.h"
#include "webkit/dom_storage/dom_storage_types.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/webpreferences.h"

namespace content {

namespace {
// Normally this is done by the NavigationController, but we'll fake it out
// here for testing.
SessionStorageNamespaceImpl* CreateSessionStorageNamespace(
    SiteInstance* instance) {
  RenderProcessHost* process_host = instance->GetProcess();
  DOMStorageContext* dom_storage_context =
      BrowserContext::GetStoragePartition(process_host->GetBrowserContext(),
                                          instance)->GetDOMStorageContext();
  return new SessionStorageNamespaceImpl(
      static_cast<DOMStorageContextImpl*>(dom_storage_context));
}
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
  params->password_form = PasswordForm();
  params->security_info = std::string();
  params->gesture = NavigationGestureUser;
  params->was_within_same_page = false;
  params->is_post = false;
  params->content_state = webkit_glue::CreateHistoryStateForURL(GURL(url));
}

TestRenderWidgetHostView::TestRenderWidgetHostView(RenderWidgetHost* rwh)
    : rwh_(RenderWidgetHostImpl::From(rwh)),
      is_showing_(false) {
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

void TestRenderWidgetHostView::RenderViewGone(base::TerminationStatus status,
                                              int error_code) {
  delete this;
}

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
    const base::Callback<void(bool)>& callback,
    skia::PlatformBitmap* output) {
  callback.Run(false);
}

void TestRenderWidgetHostView::OnAcceleratedCompositingStateChange() {
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

void TestRenderWidgetHostView::PluginFocusChanged(bool focused,
                                                  int plugin_id) {
}

void TestRenderWidgetHostView::StartPluginIme() {
}

bool TestRenderWidgetHostView::PostProcessEventForPluginIme(
    const NativeWebKeyboardEvent& event) {
  return false;
}

gfx::PluginWindowHandle
TestRenderWidgetHostView::AllocateFakePluginWindowHandle(
    bool opaque,
    bool root) {
  return gfx::kNullPluginWindow;
}

void TestRenderWidgetHostView::DestroyFakePluginWindowHandle(
    gfx::PluginWindowHandle window) {
}

void TestRenderWidgetHostView::AcceleratedSurfaceSetIOSurface(
    gfx::PluginWindowHandle window,
    int32 width,
    int32 height,
    uint64 surface_id) {
}

void TestRenderWidgetHostView::AcceleratedSurfaceSetTransportDIB(
    gfx::PluginWindowHandle window,
    int32 width,
    int32 height,
    TransportDIB::Handle transport_dib) {
}

#elif defined(OS_WIN) && !defined(USE_AURA)
void TestRenderWidgetHostView::WillWmDestroy() {
}
#endif

#if defined(OS_ANDROID)
void TestRenderWidgetHostView::StartContentIntent(const GURL&) {}
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

gfx::GLSurfaceHandle TestRenderWidgetHostView::GetCompositingSurface() {
  return gfx::GLSurfaceHandle();
}

bool TestRenderWidgetHostView::LockMouse() {
  return false;
}

void TestRenderWidgetHostView::UnlockMouse() {
}

TestRenderViewHost::TestRenderViewHost(
    SiteInstance* instance,
    RenderViewHostDelegate* delegate,
    RenderWidgetHostDelegate* widget_delegate,
    int routing_id,
    bool swapped_out)
    : RenderViewHostImpl(instance,
                         delegate,
                         widget_delegate,
                         routing_id,
                         swapped_out,
                         CreateSessionStorageNamespace(instance)),
      render_view_created_(false),
      delete_counter_(NULL),
      simulate_fetch_via_proxy_(false),
      contents_mime_type_("text/html") {
  // For normal RenderViewHosts, this is freed when |Shutdown()| is
  // called.  For TestRenderViewHost, the view is explicitly
  // deleted in the destructor below, because
  // TestRenderWidgetHostView::Destroy() doesn't |delete this|.
  SetView(new TestRenderWidgetHostView(this));
}

TestRenderViewHost::~TestRenderViewHost() {
  if (delete_counter_)
    ++*delete_counter_;

  // Since this isn't a traditional view, we have to delete it.
  delete GetView();
}

bool TestRenderViewHost::CreateRenderView(
    const string16& frame_name,
    int opener_route_id,
    int32 max_page_id) {
  DCHECK(!render_view_created_);
  render_view_created_ = true;
  return true;
}

bool TestRenderViewHost::IsRenderViewLive() const {
  return render_view_created_;
}

void TestRenderViewHost::SendNavigate(int page_id, const GURL& url) {
  SendNavigateWithTransition(page_id, url, PAGE_TRANSITION_LINK);
}

void TestRenderViewHost::SendNavigateWithTransition(
    int page_id, const GURL& url, PageTransition transition) {
  OnDidStartProvisionalLoadForFrame(0, -1, true, url);
  SendNavigateWithParameters(page_id, url, transition, url);
}

void TestRenderViewHost::SendNavigateWithOriginalRequestURL(
    int page_id, const GURL& url, const GURL& original_request_url) {
  OnDidStartProvisionalLoadForFrame(0, -1, true, url);
  SendNavigateWithParameters(page_id, url, PAGE_TRANSITION_LINK,
      original_request_url);
}

void TestRenderViewHost::SendNavigateWithParameters(
    int page_id, const GURL& url, PageTransition transition,
    const GURL& original_request_url) {
  ViewHostMsg_FrameNavigate_Params params;

  params.page_id = page_id;
  params.frame_id = 0;
  params.url = url;
  params.referrer = Referrer();
  params.transition = transition;
  params.redirects = std::vector<GURL>();
  params.should_update_history = true;
  params.searchable_form_url = GURL();
  params.searchable_form_encoding = std::string();
  params.password_form = PasswordForm();
  params.security_info = std::string();
  params.gesture = NavigationGestureUser;
  params.contents_mime_type = contents_mime_type_;
  params.is_post = false;
  params.was_within_same_page = false;
  params.http_status_code = 0;
  params.socket_address.set_host("2001:db8::1");
  params.socket_address.set_port(80);
  params.was_fetched_via_proxy = simulate_fetch_via_proxy_;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url));
  params.original_request_url = original_request_url;

  ViewHostMsg_FrameNavigate msg(1, params);
  OnNavigate(msg);
}

void TestRenderViewHost::SendShouldCloseACK(bool proceed) {
  OnShouldCloseACK(proceed, base::TimeTicks(), base::TimeTicks());
}

void TestRenderViewHost::SetContentsMimeType(const std::string& mime_type) {
  contents_mime_type_ = mime_type;
}

void TestRenderViewHost::SimulateSwapOutACK() {
  OnSwapOutACK(false);
}

void TestRenderViewHost::SimulateWasHidden() {
  WasHidden();
}

void TestRenderViewHost::SimulateWasShown() {
  WasShown();
}

void TestRenderViewHost::TestOnStartDragging(
    const WebDropData& drop_data) {
  WebKit::WebDragOperationsMask drag_operation = WebKit::WebDragOperationEvery;
  DragEventSourceInfo event_info;
  OnStartDragging(drop_data, drag_operation, SkBitmap(), gfx::Vector2d(),
                  event_info);
}

void TestRenderViewHost::set_simulate_fetch_via_proxy(bool proxy) {
  simulate_fetch_via_proxy_ = proxy;
}

RenderViewHostImplTestHarness::RenderViewHostImplTestHarness() {
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

TestWebContents* RenderViewHostImplTestHarness::contents() {
  return static_cast<TestWebContents*>(web_contents());
}

}  // namespace content
