// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_RENDERER_HOST_H_
#define CONTENT_PUBLIC_TEST_TEST_RENDERER_HOST_H_

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

#if defined(USE_AURA)
#include "ui/aura/test/aura_test_helper.h"
#endif

namespace aura {
namespace test {
class AuraTestHelper;
}
}

namespace ui {
class ScopedOleInitializer;
}

namespace content {

class BrowserContext;
class MockRenderProcessHost;
class MockRenderProcessHostFactory;
class NavigationController;
class RenderProcessHostFactory;
class RenderViewHostDelegate;
class TestRenderFrameHostFactory;
class TestRenderViewHostFactory;
class WebContents;

// An interface and utility for driving tests of RenderFrameHost.
class RenderFrameHostTester {
 public:
  // Retrieves the RenderFrameHostTester that drives the specified
  // RenderFrameHost. The RenderFrameHost must have been created while
  // RenderFrameHost testing was enabled; use a
  // RenderViewHostTestEnabler instance (see below) to do this.
  static RenderFrameHostTester* For(RenderFrameHost* host);

  virtual ~RenderFrameHostTester() {}

  // Gives tests access to RenderFrameHostImpl::OnCreateChild. The returned
  // RenderFrameHost is owned by the parent RenderFrameHost.
  virtual RenderFrameHost* AppendChild(const std::string& frame_name) = 0;

  // Calls OnMsgNavigate on the RenderViewHost with the given information,
  // including a custom PageTransition.  Sets the rest of the
  // parameters in the message to the "typical" values. This is a helper
  // function for simulating the most common types of loads.
  virtual void SendNavigateWithTransition(int page_id,
                                          const GURL& url,
                                          ui::PageTransition transition) = 0;
};

// An interface and utility for driving tests of RenderViewHost.
class RenderViewHostTester {
 public:
  // Retrieves the RenderViewHostTester that drives the specified
  // RenderViewHost.  The RenderViewHost must have been created while
  // RenderViewHost testing was enabled; use a
  // RenderViewHostTestEnabler instance (see below) to do this.
  static RenderViewHostTester* For(RenderViewHost* host);

  // If the given WebContentsImpl has a pending RVH, returns it, otherwise NULL.
  static RenderViewHost* GetPendingForController(
      NavigationController* controller);

  // This removes the need to expose
  // RenderViewHostImpl::is_swapped_out() outside of content.
  //
  // This is safe to call on any RenderViewHost, not just ones
  // constructed while a RenderViewHostTestEnabler is in play.
  static bool IsRenderViewHostSwappedOut(RenderViewHost* rvh);

  // Calls the RenderViewHosts' private OnMessageReceived function with the
  // given message.
  static bool TestOnMessageReceived(RenderViewHost* rvh,
                                    const IPC::Message& msg);

  // Returns whether the underlying web-page has any touch-event handlers.
  static bool HasTouchEventHandler(RenderViewHost* rvh);

  virtual ~RenderViewHostTester() {}

  // Gives tests access to RenderViewHostImpl::CreateRenderView.
  virtual bool CreateRenderView(const base::string16& frame_name,
                                int opener_route_id,
                                int proxy_routing_id,
                                int32 max_page_id,
                                bool created_with_opener) = 0;

  // Calls OnMsgNavigate on the RenderViewHost with the given information,
  // setting the rest of the parameters in the message to the "typical" values.
  // This is a helper function for simulating the most common types of loads.
  virtual void SendNavigate(int page_id, const GURL& url) = 0;
  virtual void SendFailedNavigate(int page_id, const GURL& url) = 0;

  // Calls OnMsgNavigate on the RenderViewHost with the given information,
  // including a custom PageTransition.  Sets the rest of the
  // parameters in the message to the "typical" values. This is a helper
  // function for simulating the most common types of loads.
  virtual void SendNavigateWithTransition(int page_id, const GURL& url,
                                          ui::PageTransition transition) = 0;

  // Calls OnBeforeUnloadACK on the main RenderFrameHost with the given
  // parameter.
  virtual void SendBeforeUnloadACK(bool proceed) = 0;

  // If set, future loads will have |mime_type| set as the mime type.
  // If not set, the mime type will default to "text/html".
  virtual void SetContentsMimeType(const std::string& mime_type) = 0;

  // Simulates the SwapOut_ACK that fires if you commit a cross-site
  // navigation without making any network requests.
  virtual void SimulateSwapOutACK() = 0;

  // Makes the WasHidden/WasShown calls to the RenderWidget that
  // tell it it has been hidden or restored from having been hidden.
  virtual void SimulateWasHidden() = 0;
  virtual void SimulateWasShown() = 0;
};

// You can instantiate only one class like this at a time.  During its
// lifetime, RenderViewHost and RenderFrameHost objects created may be used via
// RenderViewHostTester and RenderFrameHostTester respectively.
class RenderViewHostTestEnabler {
 public:
  RenderViewHostTestEnabler();
  ~RenderViewHostTestEnabler();

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderViewHostTestEnabler);
  friend class RenderViewHostTestHarness;

  scoped_ptr<MockRenderProcessHostFactory> rph_factory_;
  scoped_ptr<TestRenderViewHostFactory> rvh_factory_;
  scoped_ptr<TestRenderFrameHostFactory> rfh_factory_;
};

// RenderViewHostTestHarness ---------------------------------------------------
class RenderViewHostTestHarness : public testing::Test {
 public:
  RenderViewHostTestHarness();
  virtual ~RenderViewHostTestHarness();

  NavigationController& controller();

  // The contents under test.
  WebContents* web_contents();

  // RVH/RFH getters are shorthand for oft-used bits of web_contents().

  // rvh() is equivalent to either of:
  //   web_contents()->GetMainFrame()->GetRenderViewHost()
  //   web_contents()->GetRenderViewHost()
  RenderViewHost* rvh();

  // pending_rvh() is equivalent to:
  //   WebContentsTester::For(web_contents())->GetPendingRenderViewHost()
  RenderViewHost* pending_rvh();

  // active_rvh() is equivalent to pending_rvh() ? pending_rvh() : rvh()
  RenderViewHost* active_rvh();

  // main_rfh() is equivalent to web_contents()->GetMainFrame()
  RenderFrameHost* main_rfh();

  // pending_main_rfh() is equivalent to:
  //   WebContentsTester::For(web_contents())->GetPendingMainFrame()
  RenderFrameHost* pending_main_rfh();

  BrowserContext* browser_context();
  MockRenderProcessHost* process();

  // Frees the current WebContents for tests that want to test destruction.
  void DeleteContents();

  // Sets the current WebContents for tests that want to alter it. Takes
  // ownership of the WebContents passed.
  void SetContents(WebContents* contents);

  // Creates a new test-enabled WebContents. Ownership passes to the
  // caller.
  WebContents* CreateTestWebContents();

  // Cover for |contents()->NavigateAndCommit(url)|. See
  // WebContentsTester::NavigateAndCommit for details.
  void NavigateAndCommit(const GURL& url);

  // Simulates a reload of the current page.
  void Reload();
  void FailedReload();

 protected:
  // testing::Test
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Derived classes should override this method to use a custom BrowserContext.
  // It is invoked by SetUp after threads were started.
  // RenderViewHostTestHarness will take ownership of the returned
  // BrowserContext.
  virtual BrowserContext* CreateBrowserContext();

  // Configures which TestBrowserThreads inside |thread_bundle| are backed by
  // real threads. Must be called before SetUp().
  void SetThreadBundleOptions(int options) {
    DCHECK(thread_bundle_.get() == NULL);
    thread_bundle_options_ = options;
  }

  TestBrowserThreadBundle* thread_bundle() { return thread_bundle_.get(); }

#if defined(USE_AURA)
  aura::Window* root_window() { return aura_test_helper_->root_window(); }
#endif

  // Replaces the RPH being used.
  void SetRenderProcessHostFactory(RenderProcessHostFactory* factory);

 private:
  scoped_ptr<BrowserContext> browser_context_;

  scoped_ptr<WebContents> contents_;
#if defined(OS_WIN)
  scoped_ptr<ui::ScopedOleInitializer> ole_initializer_;
#endif
#if defined(USE_AURA)
  scoped_ptr<aura::test::AuraTestHelper> aura_test_helper_;
#endif
  RenderViewHostTestEnabler rvh_test_enabler_;

  int thread_bundle_options_;
  scoped_ptr<TestBrowserThreadBundle> thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostTestHarness);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_RENDERER_HOST_H_
