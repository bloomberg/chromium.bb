
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_url_handler.h"
#include "content/browser/renderer_host/test_backing_store.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "content/common/dom_storage_common.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_client.h"
#include "content/test/test_browser_context.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/password_form.h"

using webkit_glue::PasswordForm;

void InitNavigateParams(ViewHostMsg_FrameNavigate_Params* params,
                        int page_id,
                        const GURL& url,
                        content::PageTransition transition) {
  params->page_id = page_id;
  params->url = url;
  params->referrer = GURL();
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

TestRenderViewHost::TestRenderViewHost(SiteInstance* instance,
                                       RenderViewHostDelegate* delegate,
                                       int routing_id)
    : RenderViewHost(instance, delegate, routing_id,
                     kInvalidSessionStorageNamespaceId),
      render_view_created_(false),
      delete_counter_(NULL),
      simulate_fetch_via_proxy_(false),
      contents_mime_type_("text/html") {
  // For normal RenderViewHosts, this is freed when |Shutdown()| is called.
  // For TestRenderViewHost, the view is explicitly deleted in the destructor
  // below, because TestRenderWidgetHostView::Destroy() doesn't |delete this|.
  SetView(new TestRenderWidgetHostView(this));
}

TestRenderViewHost::~TestRenderViewHost() {
  if (delete_counter_)
    ++*delete_counter_;

  // Since this isn't a traditional view, we have to delete it.
  delete view();
}

bool TestRenderViewHost::CreateRenderView(const string16& frame_name) {
  DCHECK(!render_view_created_);
  render_view_created_ = true;
  return true;
}

bool TestRenderViewHost::IsRenderViewLive() const {
  return render_view_created_;
}

bool TestRenderViewHost::TestOnMessageReceived(const IPC::Message& msg) {
  return OnMessageReceived(msg);
}

void TestRenderViewHost::SendNavigate(int page_id, const GURL& url) {
  SendNavigateWithTransition(page_id, url, content::PAGE_TRANSITION_LINK);
}

void TestRenderViewHost::SendNavigateWithTransition(
    int page_id, const GURL& url, content::PageTransition transition) {
  ViewHostMsg_FrameNavigate_Params params;

  params.page_id = page_id;
  params.url = url;
  params.referrer = GURL();
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

  ViewHostMsg_FrameNavigate msg(1, params);
  OnMsgNavigate(msg);
}

void TestRenderViewHost::SendShouldCloseACK(bool proceed) {
  OnMsgShouldCloseACK(proceed);
}

void TestRenderViewHost::TestOnMsgStartDragging(const WebDropData& drop_data) {
  WebKit::WebDragOperationsMask drag_operation = WebKit::WebDragOperationEvery;
  OnMsgStartDragging(drop_data, drag_operation, SkBitmap(), gfx::Point());
}

void TestRenderViewHost::set_simulate_fetch_via_proxy(bool proxy) {
  simulate_fetch_via_proxy_ = proxy;
}

void TestRenderViewHost::set_contents_mime_type(const std::string& mime_type) {
  contents_mime_type_ = mime_type;
}

TestRenderWidgetHostView::TestRenderWidgetHostView(RenderWidgetHost* rwh)
    : rwh_(rwh),
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

bool TestRenderWidgetHostView::HasFocus() const {
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

void TestRenderWidgetHostView::OnAcceleratedCompositingStateChange() {
}

#if defined(OS_MACOSX)

gfx::Rect TestRenderWidgetHostView::GetViewCocoaBounds() const {
  return gfx::Rect();
}

void TestRenderWidgetHostView::SetActive(bool active) {
  // <viettrungluu@gmail.com>: Do I need to do anything here?
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
  return NULL;
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

void TestRenderWidgetHostView::AcceleratedSurfaceBuffersSwapped(
    gfx::PluginWindowHandle window,
    uint64 surface_id,
    int renderer_id,
    int32 route_id,
    int gpu_host_id) {
}

#elif defined(OS_WIN)
void TestRenderWidgetHostView::WillWmDestroy() {
}

#endif

#if defined(OS_POSIX) || defined(USE_AURA)
gfx::Rect TestRenderWidgetHostView::GetRootWindowBounds() {
  return gfx::Rect();
}
#endif

gfx::PluginWindowHandle TestRenderWidgetHostView::GetCompositingSurface() {
  return gfx::kNullPluginWindow;
}

bool TestRenderWidgetHostView::LockMouse() {
  return false;
}

void TestRenderWidgetHostView::UnlockMouse() {
}

TestRenderViewHostFactory::TestRenderViewHostFactory(
    RenderProcessHostFactory* rph_factory)
    : render_process_host_factory_(rph_factory) {
  RenderViewHostFactory::RegisterFactory(this);
}

TestRenderViewHostFactory::~TestRenderViewHostFactory() {
  RenderViewHostFactory::UnregisterFactory();
}

void TestRenderViewHostFactory::set_render_process_host_factory(
    RenderProcessHostFactory* rph_factory) {
  render_process_host_factory_ = rph_factory;
}

RenderViewHost* TestRenderViewHostFactory::CreateRenderViewHost(
    SiteInstance* instance,
    RenderViewHostDelegate* delegate,
    int routing_id,
    SessionStorageNamespace* session_storage) {
  // See declaration of render_process_host_factory_ below.
  instance->set_render_process_host_factory(render_process_host_factory_);
  return new TestRenderViewHost(instance, delegate, routing_id);
}

RenderViewHostTestHarness::RenderViewHostTestHarness()
    : rph_factory_(),
      rvh_factory_(&rph_factory_),
      contents_(NULL) {
}

RenderViewHostTestHarness::~RenderViewHostTestHarness() {
}

NavigationController& RenderViewHostTestHarness::controller() {
  return contents()->controller();
}

TestTabContents* RenderViewHostTestHarness::contents() {
  return contents_.get();
}

TestRenderViewHost* RenderViewHostTestHarness::rvh() {
  return static_cast<TestRenderViewHost*>(contents()->render_view_host());
}

TestRenderViewHost* RenderViewHostTestHarness::pending_rvh() {
  return static_cast<TestRenderViewHost*>(
      contents()->render_manager_for_testing()->pending_render_view_host());
}

TestRenderViewHost* RenderViewHostTestHarness::active_rvh() {
  return pending_rvh() ? pending_rvh() : rvh();
}

content::BrowserContext* RenderViewHostTestHarness::browser_context() {
  return browser_context_.get();
}

MockRenderProcessHost* RenderViewHostTestHarness::process() {
  if (pending_rvh())
    return static_cast<MockRenderProcessHost*>(pending_rvh()->process());
  return static_cast<MockRenderProcessHost*>(rvh()->process());
}

void RenderViewHostTestHarness::DeleteContents() {
  SetContents(NULL);
}

void RenderViewHostTestHarness::SetContents(TestTabContents* contents) {
  contents_.reset(contents);
}

TestTabContents* RenderViewHostTestHarness::CreateTestTabContents() {
  // See comment above browser_context_ decl for why we check for NULL here.
  if (!browser_context_.get())
    browser_context_.reset(new TestBrowserContext());

  // This will be deleted when the TabContents goes away.
  SiteInstance* instance =
      SiteInstance::CreateSiteInstance(browser_context_.get());

  return new TestTabContents(browser_context_.get(), instance);
}

void RenderViewHostTestHarness::NavigateAndCommit(const GURL& url) {
  contents()->NavigateAndCommit(url);
}

void RenderViewHostTestHarness::Reload() {
  NavigationEntry* entry = controller().GetLastCommittedEntry();
  DCHECK(entry);
  controller().Reload(false);
  rvh()->SendNavigate(entry->page_id(), entry->url());
}

void RenderViewHostTestHarness::SetUp() {
  SetContents(CreateTestTabContents());
}

void RenderViewHostTestHarness::TearDown() {
  SetContents(NULL);

  // Make sure that we flush any messages related to TabContents destruction
  // before we destroy the browser context.
  MessageLoop::current()->RunAllPending();

  // Release the browser context on the UI thread.
  message_loop_.DeleteSoon(FROM_HERE, browser_context_.release());
  message_loop_.RunAllPending();
}
