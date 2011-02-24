// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_url_handler.h"
#include "chrome/common/dom_storage_common.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/renderer_host/test_backing_store.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/password_form.h"

using webkit_glue::PasswordForm;

void InitNavigateParams(ViewHostMsg_FrameNavigate_Params* params,
                        int page_id,
                        const GURL& url,
                        PageTransition::Type transition) {
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
}

TestRenderViewHost::TestRenderViewHost(SiteInstance* instance,
                                       RenderViewHostDelegate* delegate,
                                       int routing_id)
    : RenderViewHost(instance, delegate, routing_id,
                     kInvalidSessionStorageNamespaceId),
      render_view_created_(false),
      delete_counter_(NULL) {
  // For normal RenderViewHosts, this is freed when |Shutdown()| is called.
  // For TestRenderViewHost, the view is explicitly deleted in the destructor
  // below, because TestRenderWidgetHostView::Destroy() doesn't |delete this|.
  set_view(new TestRenderWidgetHostView(this));
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
  process()->ViewCreated();
  return true;
}

bool TestRenderViewHost::IsRenderViewLive() const {
  return render_view_created_;
}

bool TestRenderViewHost::TestOnMessageReceived(const IPC::Message& msg) {
  return OnMessageReceived(msg);
}

void TestRenderViewHost::SendNavigate(int page_id, const GURL& url) {
  SendNavigateWithTransition(page_id, url, PageTransition::LINK);
}

void TestRenderViewHost::SendNavigateWithTransition(
    int page_id, const GURL& url, PageTransition::Type transition) {
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
  params.contents_mime_type = std::string();
  params.is_post = false;
  params.is_content_filtered = false;
  params.was_within_same_page = false;
  params.http_status_code = 0;
  params.socket_address.set_host("2001:db8::1");
  params.socket_address.set_port(80);

  ViewHostMsg_FrameNavigate msg(1, params);
  OnMsgNavigate(msg);
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

gfx::NativeView TestRenderWidgetHostView::GetNativeView() {
  return NULL;
}

bool TestRenderWidgetHostView::HasFocus() {
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

#if defined(OS_MACOSX)

void TestRenderWidgetHostView::ShowPopupWithItems(
    gfx::Rect bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    const std::vector<WebMenuItem>& items,
    bool right_aligned) {
}

gfx::Rect TestRenderWidgetHostView::GetViewCocoaBounds() const {
  return gfx::Rect();
}

gfx::Rect TestRenderWidgetHostView::GetRootWindowRect() {
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
    int gpu_host_id,
    uint64 swap_buffers_count) {
}

void TestRenderWidgetHostView::GpuRenderingStateDidChange() {
}
#elif defined(OS_WIN)
void TestRenderWidgetHostView::WillWmDestroy() {
}

void TestRenderWidgetHostView::ShowCompositorHostWindow(bool show) {
}
#endif

gfx::PluginWindowHandle TestRenderWidgetHostView::AcquireCompositingSurface() {
  return gfx::kNullPluginWindow;
}

bool TestRenderWidgetHostView::ContainsNativeView(
    gfx::NativeView native_view) const {
  return false;
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
  return contents_->controller();
}

TestTabContents* RenderViewHostTestHarness::contents() {
  return contents_.get();
}

TestRenderViewHost* RenderViewHostTestHarness::rvh() {
  return static_cast<TestRenderViewHost*>(contents_->render_view_host());
}

TestRenderViewHost* RenderViewHostTestHarness::pending_rvh() {
  return static_cast<TestRenderViewHost*>(
      contents_->render_manager()->pending_render_view_host());
}

TestRenderViewHost* RenderViewHostTestHarness::active_rvh() {
  return pending_rvh() ? pending_rvh() : rvh();
}

TestingProfile* RenderViewHostTestHarness::profile() {
  return profile_.get();
}

MockRenderProcessHost* RenderViewHostTestHarness::process() {
  if (pending_rvh())
    return static_cast<MockRenderProcessHost*>(pending_rvh()->process());
  return static_cast<MockRenderProcessHost*>(rvh()->process());
}

void RenderViewHostTestHarness::DeleteContents() {
  contents_.reset();
}

TestTabContents* RenderViewHostTestHarness::CreateTestTabContents() {
  // See comment above profile_ decl for why we check for NULL here.
  if (!profile_.get())
    profile_.reset(new TestingProfile());

  // This will be deleted when the TabContents goes away.
  SiteInstance* instance = SiteInstance::CreateSiteInstance(profile_.get());

  return new TestTabContents(profile_.get(), instance);
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
  contents_.reset(CreateTestTabContents());
}

void RenderViewHostTestHarness::TearDown() {
  contents_.reset();

  // Make sure that we flush any messages related to TabContents destruction
  // before we destroy the profile.
  MessageLoop::current()->RunAllPending();

  // Release the profile on the UI thread.
  message_loop_.DeleteSoon(FROM_HERE, profile_.release());
  message_loop_.RunAllPending();
}
