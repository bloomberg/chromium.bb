// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDERER_HOST_H_
#define CONTENT_TEST_TEST_RENDERER_HOST_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/page_transition_types.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_AURA)
namespace aura {
class RootWindow;
namespace test {
class TestStackingClient;
}
}
#endif

namespace content {

class BrowserContext;
class MockRenderProcessHost;
class MockRenderProcessHostFactory;
class NavigationController;
class RenderProcessHostFactory;
class RenderViewHostDelegate;
class TestRenderViewHostFactory;
class WebContents;

// An interface and utility for driving tests of RenderViewHost.
class RenderViewHostTester {
 public:
  // Retrieves the RenderViewHostTester that drives the specified
  // RenderViewHost.  The RenderViewHost must have been created while
  // RenderViewHost testing was enabled; use a
  // RenderViewHostTestEnabler instance (see below) to do this.
  static RenderViewHostTester* For(RenderViewHost* host);

  // This removes the need to expose
  // RenderViewHostImpl::set_send_accessibility_updated_notifications()
  // outside of content.
  static void EnableAccessibilityUpdatedNotifications(RenderViewHost* host);

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

  virtual ~RenderViewHostTester() {}

  // Gives tests access to RenderViewHostImpl::CreateRenderView.
  virtual bool CreateRenderView(const string16& frame_name,
                                int opener_route_id,
                                int32 max_page_id,
                                int embedder_process_id) = 0;

  // Calls OnMsgNavigate on the RenderViewHost with the given information,
  // setting the rest of the parameters in the message to the "typical" values.
  // This is a helper function for simulating the most common types of loads.
  virtual void SendNavigate(int page_id, const GURL& url) = 0;

  // Calls OnMsgNavigate on the RenderViewHost with the given information,
  // including a custom PageTransition.  Sets the rest of the
  // parameters in the message to the "typical" values. This is a helper
  // function for simulating the most common types of loads.
  virtual void SendNavigateWithTransition(int page_id, const GURL& url,
                                          PageTransition transition) = 0;

  // Calls OnMsgShouldCloseACK on the RenderViewHost with the given parameter.
  virtual void SendShouldCloseACK(bool proceed) = 0;

  // If set, future loads will have |mime_type| set as the mime type.
  // If not set, the mime type will default to "text/html".
  virtual void SetContentsMimeType(const std::string& mime_type) = 0;

  // Simulates the SwapOut_ACK that fires if you commit a cross-site
  // navigation without making any network requests.
  virtual void SimulateSwapOutACK() = 0;

  // Makes the WasHidden/WasRestored calls to the RenderWidget that
  // tell it it has been hidden or restored from having been hidden.
  virtual void SimulateWasHidden() = 0;
  virtual void SimulateWasRestored() = 0;
};

// You can instantiate only one class like this at a time.  During its
// lifetime, RenderViewHost objects created may be used via
// RenderViewHostTester.
class RenderViewHostTestEnabler {
 public:
  RenderViewHostTestEnabler();
  ~RenderViewHostTestEnabler();

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderViewHostTestEnabler);
  friend class RenderViewHostTestHarness;

  scoped_ptr<MockRenderProcessHostFactory> rph_factory_;
  scoped_ptr<TestRenderViewHostFactory> rvh_factory_;
};

// RenderViewHostTestHarness ---------------------------------------------------
class RenderViewHostTestHarness : public testing::Test {
 public:
  RenderViewHostTestHarness();
  virtual ~RenderViewHostTestHarness();

  NavigationController& controller();
  virtual WebContents* web_contents();
  RenderViewHost* rvh();
  RenderViewHost* pending_rvh();
  RenderViewHost* active_rvh();
  BrowserContext* browser_context();
  MockRenderProcessHost* process();

  // Frees the current WebContents for tests that want to test destruction.
  void DeleteContents();

  // Sets the current WebContents for tests that want to alter it. Takes
  // ownership of the WebContents passed.
  virtual void SetContents(WebContents* contents);

  // Creates a new test-enabled WebContents. Ownership passes to the
  // caller.
  WebContents* CreateTestWebContents();

  // Cover for |contents()->NavigateAndCommit(url)|. See
  // WebContentsTester::NavigateAndCommit for details.
  void NavigateAndCommit(const GURL& url);

  // Simulates a reload of the current page.
  void Reload();

 protected:
  // testing::Test
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

#if defined(USE_AURA)
  aura::RootWindow* root_window() const { return root_window_.get(); }
#endif

  // Replaces the RPH being used.
  void SetRenderProcessHostFactory(RenderProcessHostFactory* factory);

  // This browser context will be created in SetUp if it has not already been
  // created.  This allows tests to override the browser context if they so
  // choose in their own SetUp function before calling the base class's (us)
  // SetUp().
  scoped_ptr<BrowserContext> browser_context_;

  MessageLoopForUI message_loop_;

 private:
  // It is important not to use this directly in the implementation as
  // web_contents() and SetContents() are virtual and may be
  // overridden by subclasses.
  scoped_ptr<WebContents> contents_;
#if defined(USE_AURA)
  scoped_ptr<aura::RootWindow> root_window_;
  scoped_ptr<aura::test::TestStackingClient> test_stacking_client_;
#endif
  RenderViewHostTestEnabler rvh_test_enabler_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostTestHarness);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDERER_HOST_H_
