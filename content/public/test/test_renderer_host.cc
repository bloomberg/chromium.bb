// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_renderer_host.h"

#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/navigation_entry_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host_factory.h"
#include "content/test/test_web_contents.h"

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/test/aura_test_helper.h"
#endif

namespace content {

// RenderViewHostTester -------------------------------------------------------

// static
RenderViewHostTester* RenderViewHostTester::For(RenderViewHost* host) {
  return static_cast<TestRenderViewHost*>(host);
}

// static
RenderViewHost* RenderViewHostTester::GetPendingForController(
    NavigationController* controller) {
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      controller->GetWebContents());
  return web_contents->GetRenderManagerForTesting()->pending_render_view_host();
}

// static
bool RenderViewHostTester::IsRenderViewHostSwappedOut(RenderViewHost* rvh) {
  return static_cast<RenderViewHostImpl*>(rvh)->is_swapped_out();
}

// static
bool RenderViewHostTester::TestOnMessageReceived(RenderViewHost* rvh,
                                                 const IPC::Message& msg) {
  return static_cast<RenderViewHostImpl*>(rvh)->OnMessageReceived(msg);
}

// static
bool RenderViewHostTester::HasTouchEventHandler(RenderViewHost* rvh) {
  RenderWidgetHostImpl* host_impl = RenderWidgetHostImpl::From(rvh);
  return host_impl->has_touch_handler();
}


// RenderViewHostTestEnabler --------------------------------------------------

RenderViewHostTestEnabler::RenderViewHostTestEnabler()
    : rph_factory_(new MockRenderProcessHostFactory()),
      rvh_factory_(new TestRenderViewHostFactory(rph_factory_.get())) {
}

RenderViewHostTestEnabler::~RenderViewHostTestEnabler() {
}


// RenderViewHostTestHarness --------------------------------------------------

RenderViewHostTestHarness::RenderViewHostTestHarness() : contents_(NULL) {
}

RenderViewHostTestHarness::~RenderViewHostTestHarness() {
}

NavigationController& RenderViewHostTestHarness::controller() {
  return web_contents()->GetController();
}

WebContents* RenderViewHostTestHarness::web_contents() {
  return contents_.get();
}

RenderViewHost* RenderViewHostTestHarness::rvh() {
  return web_contents()->GetRenderViewHost();
}

RenderViewHost* RenderViewHostTestHarness::pending_rvh() {
  return static_cast<TestWebContents*>(web_contents())->
      GetRenderManagerForTesting()->pending_render_view_host();
}

RenderViewHost* RenderViewHostTestHarness::active_rvh() {
  return pending_rvh() ? pending_rvh() : rvh();
}

BrowserContext* RenderViewHostTestHarness::browser_context() {
  return browser_context_.get();
}

MockRenderProcessHost* RenderViewHostTestHarness::process() {
  return static_cast<MockRenderProcessHost*>(active_rvh()->GetProcess());
}

void RenderViewHostTestHarness::DeleteContents() {
  SetContents(NULL);
}

void RenderViewHostTestHarness::SetContents(WebContents* contents) {
  contents_.reset(contents);
}

WebContents* RenderViewHostTestHarness::CreateTestWebContents() {
  // Make sure we ran SetUp() already.
#if defined(OS_WIN)
  DCHECK(ole_initializer_ != NULL);
#endif
#if defined(USE_AURA)
  DCHECK(aura_test_helper_ != NULL);
#endif

  // See comment above browser_context_ decl for why we check for NULL here.
  if (!browser_context_.get())
    browser_context_.reset(new TestBrowserContext());

  // This will be deleted when the WebContentsImpl goes away.
  SiteInstance* instance = SiteInstance::Create(browser_context_.get());

  return TestWebContents::Create(browser_context_.get(), instance);
}

void RenderViewHostTestHarness::NavigateAndCommit(const GURL& url) {
  static_cast<TestWebContents*>(web_contents())->NavigateAndCommit(url);
}

void RenderViewHostTestHarness::Reload() {
  NavigationEntry* entry = controller().GetLastCommittedEntry();
  DCHECK(entry);
  controller().Reload(false);
  static_cast<TestRenderViewHost*>(
      rvh())->SendNavigate(entry->GetPageID(), entry->GetURL());
}

void RenderViewHostTestHarness::SetUp() {
#if defined(OS_WIN)
  ole_initializer_.reset(new ui::ScopedOleInitializer());
#endif
#if defined(USE_AURA)
  aura_test_helper_.reset(new aura::test::AuraTestHelper(&message_loop_));
  aura_test_helper_->SetUp();
#endif
  SetContents(CreateTestWebContents());
}

void RenderViewHostTestHarness::TearDown() {
  SetContents(NULL);
#if defined(USE_AURA)
  aura_test_helper_->TearDown();
#endif
  // Make sure that we flush any messages related to WebContentsImpl destruction
  // before we destroy the browser context.
  MessageLoop::current()->RunUntilIdle();

  // Delete any RenderProcessHosts before the BrowserContext goes away.
  if (rvh_test_enabler_.rph_factory_.get())
    rvh_test_enabler_.rph_factory_.reset();

  // Release the browser context on the UI thread.
  message_loop_.DeleteSoon(FROM_HERE, browser_context_.release());
  message_loop_.RunUntilIdle();

#if defined(OS_WIN)
  ole_initializer_.reset();
#endif
}

void RenderViewHostTestHarness::SetRenderProcessHostFactory(
    RenderProcessHostFactory* factory) {
    rvh_test_enabler_.rvh_factory_->set_render_process_host_factory(factory);
}

}  // namespace content
