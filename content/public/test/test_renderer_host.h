// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_RENDERER_HOST_H_
#define CONTENT_PUBLIC_TEST_TEST_RENDERER_HOST_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
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

namespace display {
class Screen;
}

namespace ui {
class ScopedOleInitializer;
}

namespace content {

class BrowserContext;
class ContentBrowserSanityChecker;
class MockRenderProcessHost;
class MockRenderProcessHostFactory;
class NavigationController;
class RenderProcessHostFactory;
class TestRenderFrameHostFactory;
class TestRenderViewHostFactory;
class WebContents;
struct WebPreferences;

// An interface and utility for driving tests of RenderFrameHost.
class RenderFrameHostTester {
 public:
  // Retrieves the RenderFrameHostTester that drives the specified
  // RenderFrameHost. The RenderFrameHost must have been created while
  // RenderFrameHost testing was enabled; use a
  // RenderViewHostTestEnabler instance (see below) to do this.
  static RenderFrameHostTester* For(RenderFrameHost* host);

  // Calls the RenderFrameHost's private OnMessageReceived function with the
  // given message.
  static bool TestOnMessageReceived(RenderFrameHost* rfh,
                                    const IPC::Message& msg);

  static void CommitPendingLoad(NavigationController* controller);

  virtual ~RenderFrameHostTester() {}

  // Simulates initialization of the RenderFrame object in the renderer process
  // and ensures internal state of RenderFrameHost is ready for simulating
  // RenderFrame originated IPCs.
  virtual void InitializeRenderFrameIfNeeded() = 0;

  // Gives tests access to RenderFrameHostImpl::OnCreateChild. The returned
  // RenderFrameHost is owned by the parent RenderFrameHost.
  virtual RenderFrameHost* AppendChild(const std::string& frame_name) = 0;

  // Gives tests access to RenderFrameHostImpl::OnDetach. Destroys |this|.
  virtual void Detach() = 0;

  // Simulates a navigation stopping in the RenderFrameHost.
  virtual void SimulateNavigationStop() = 0;

  // Calls OnDidCommitProvisionalLoad on the RenderFrameHost with the given
  // information with various sets of parameters. These are helper functions for
  // simulating the most common types of loads.
  //
  // Guidance for calling these:
  // - nav_entry_id should be 0 if simulating a renderer-initiated navigation;
  //   if simulating a browser-initiated one, pass the GetUniqueID() value of
  //   the NavigationController's PendingEntry.
  // - did_create_new_entry should be true if simulating a navigation that
  //   created a new navigation entry; false for history navigations, reloads,
  //   and other navigations that don't affect the history list.
  virtual void SendFailedNavigate(int nav_entry_id,
                                  bool did_create_new_entry,
                                  const GURL& url) = 0;
  virtual void SendNavigateWithTransition(int nav_entry_id,
                                          bool did_create_new_entry,
                                          const GURL& url,
                                          ui::PageTransition transition) = 0;

  // If set, future loads will have |mime_type| set as the mime type.
  // If not set, the mime type will default to "text/html".
  virtual void SetContentsMimeType(const std::string& mime_type) = 0;

  // Calls OnBeforeUnloadACK on this RenderFrameHost with the given parameter.
  virtual void SendBeforeUnloadACK(bool proceed) = 0;

  // Simulates the SwapOut_ACK that fires if you commit a cross-site
  // navigation without making any network requests.
  virtual void SimulateSwapOutACK() = 0;

  // Simulate a renderer-initiated navigation up until commit.
  // DEPRECATED: Use NavigationSimulator::NavigateAndCommitFromDocument().
  virtual void NavigateAndCommitRendererInitiated(bool did_create_new_entry,
                                                  const GURL& url) = 0;

  // Set the feature policy header for the RenderFrameHost for test. Currently
  // this is limited to setting a whitelist for a single feature. This function
  // can be generalized as needed. Setting a header policy should only be done
  // once per navigation of the RFH.
  virtual void SimulateFeaturePolicyHeader(
      blink::WebFeaturePolicyFeature feature,
      const std::vector<url::Origin>& whitelist) = 0;
};

// An interface and utility for driving tests of RenderViewHost.
class RenderViewHostTester {
 public:
  // Retrieves the RenderViewHostTester that drives the specified
  // RenderViewHost.  The RenderViewHost must have been created while
  // RenderViewHost testing was enabled; use a
  // RenderViewHostTestEnabler instance (see below) to do this.
  static RenderViewHostTester* For(RenderViewHost* host);

  // Calls the RenderViewHosts' private OnMessageReceived function with the
  // given message.
  static bool TestOnMessageReceived(RenderViewHost* rvh,
                                    const IPC::Message& msg);

  // Returns whether the underlying web-page has any touch-event handlers.
  static bool HasTouchEventHandler(RenderViewHost* rvh);

  virtual ~RenderViewHostTester() {}

  // Gives tests access to RenderViewHostImpl::CreateRenderView.
  virtual bool CreateTestRenderView(const base::string16& frame_name,
                                    int opener_frame_route_id,
                                    int proxy_routing_id,
                                    bool created_with_opener) = 0;

  // Makes the WasHidden/WasShown calls to the RenderWidget that
  // tell it it has been hidden or restored from having been hidden.
  virtual void SimulateWasHidden() = 0;
  virtual void SimulateWasShown() = 0;

  // Promote ComputeWebkitPrefs to public.
  virtual WebPreferences TestComputeWebkitPrefs() = 0;
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

#if defined(OS_ANDROID)
  std::unique_ptr<display::Screen> screen_;
#endif
  std::unique_ptr<base::MessageLoop> message_loop_;
  std::unique_ptr<MockRenderProcessHostFactory> rph_factory_;
  std::unique_ptr<TestRenderViewHostFactory> rvh_factory_;
  std::unique_ptr<TestRenderFrameHostFactory> rfh_factory_;
};

// RenderViewHostTestHarness ---------------------------------------------------
class RenderViewHostTestHarness : public testing::Test {
 public:
  RenderViewHostTestHarness();
  ~RenderViewHostTestHarness() override;

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
  void SetUp() override;
  void TearDown() override;

  // Derived classes should override this method to use a custom BrowserContext.
  // It is invoked by SetUp after threads were started.
  // RenderViewHostTestHarness will take ownership of the returned
  // BrowserContext.
  virtual BrowserContext* CreateBrowserContext();

  // Configures which TestBrowserThreads inside |thread_bundle| are backed by
  // real threads. Must be called before SetUp().
  void SetThreadBundleOptions(int options) {
    DCHECK(!thread_bundle_);
    thread_bundle_options_ = options;
  }

  base::test::ScopedTaskEnvironment* scoped_task_environment() {
    return scoped_task_environment_.get();
  }

  TestBrowserThreadBundle* thread_bundle() { return thread_bundle_.get(); }

#if defined(USE_AURA)
  aura::Window* root_window() { return aura_test_helper_->root_window(); }
#endif

  // Replaces the RPH being used.
  void SetRenderProcessHostFactory(RenderProcessHostFactory* factory);

 private:
  friend class AudioRendererHostTest;

  // DEPRECATED: New tests should not use this method.
  // Multithreaded tests that have been ported to TaskScheduler must use a
  // scoped task environment. Most legacy tests are compatible with a
  // TestBrowserThreadBundle inside a scoped task environment. This method is
  // for tests that specifically rely on TestBrowserThreadBundle in the absence
  // of a scoped task environment.
  void DisableScopedTaskEnvironment() { use_scoped_task_environment_ = false; }

  bool use_scoped_task_environment_;
  std::unique_ptr<base::test::ScopedTaskEnvironment> scoped_task_environment_;

  int thread_bundle_options_;
  std::unique_ptr<TestBrowserThreadBundle> thread_bundle_;

  std::unique_ptr<ContentBrowserSanityChecker> sanity_checker_;

  std::unique_ptr<BrowserContext> browser_context_;

  std::unique_ptr<WebContents> contents_;
#if defined(OS_WIN)
  std::unique_ptr<ui::ScopedOleInitializer> ole_initializer_;
#endif
#if defined(USE_AURA)
  std::unique_ptr<aura::test::AuraTestHelper> aura_test_helper_;
#endif
  std::unique_ptr<RenderViewHostTestEnabler> rvh_test_enabler_;
  RenderProcessHostFactory* factory_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostTestHarness);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_RENDERER_HOST_H_
