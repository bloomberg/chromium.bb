// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/time.h"
#include "content/browser/debugger/devtools_manager_impl.h"
#include "content/browser/debugger/render_view_devtools_agent_host.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/web_contents/test_web_contents.h"
#include "content/common/view_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host_registry.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/test/test_content_browser_client.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using content::DevToolsAgentHost;
using content::DevToolsAgentHostRegistry;
using content::DevToolsClientHost;
using content::DevToolsManager;
using content::DevToolsManagerImpl;
using content::RenderViewHostImplTestHarness;
using content::WebContents;

namespace {

class TestDevToolsClientHost : public DevToolsClientHost {
 public:
  TestDevToolsClientHost()
      : last_sent_message(NULL),
        closed_(false) {
  }

  virtual ~TestDevToolsClientHost() {
    EXPECT_TRUE(closed_);
  }

  virtual void Close(DevToolsManager* manager) {
    EXPECT_FALSE(closed_);
    close_counter++;
    manager->ClientHostClosing(this);
    closed_ = true;
  }
  virtual void InspectedContentsClosing() {
    FAIL();
  }

  virtual void DispatchOnInspectorFrontend(const std::string& message) {
    last_sent_message = &message;
  }

  virtual void ContentsReplaced(WebContents* new_contents) {
  }

  static void ResetCounters() {
    close_counter = 0;
  }

  static int close_counter;

  const std::string* last_sent_message;

 private:
  bool closed_;

  virtual void FrameNavigating(const std::string& url) {}

  DISALLOW_COPY_AND_ASSIGN(TestDevToolsClientHost);
};

int TestDevToolsClientHost::close_counter = 0;


class TestWebContentsDelegate : public content::WebContentsDelegate {
 public:
  TestWebContentsDelegate() : renderer_unresponsive_received_(false) {}

  // Notification that the contents is hung.
  virtual void RendererUnresponsive(WebContents* source) {
    renderer_unresponsive_received_ = true;
  }

  bool renderer_unresponsive_received() const {
    return renderer_unresponsive_received_;
  }

 private:
  bool renderer_unresponsive_received_;
};

class DevToolsManagerTestBrowserClient
    : public content::TestContentBrowserClient {
 public:
  DevToolsManagerTestBrowserClient() {
  }

  virtual bool ShouldSwapProcessesForNavigation(
      const GURL& current_url,
      const GURL& new_url) OVERRIDE {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DevToolsManagerTestBrowserClient);
};

}  // namespace

class DevToolsManagerTest : public RenderViewHostImplTestHarness {
 public:
  DevToolsManagerTest() {
  }

 protected:
  virtual void SetUp() OVERRIDE {
    original_browser_client_ = content::GetContentClient()->browser();
    content::GetContentClient()->set_browser_for_testing(&browser_client_);

    RenderViewHostImplTestHarness::SetUp();
    TestDevToolsClientHost::ResetCounters();
  }

  virtual void TearDown() OVERRIDE {
    RenderViewHostImplTestHarness::TearDown();
    content::GetContentClient()->set_browser_for_testing(
        original_browser_client_);
  }

 private:
  content::ContentBrowserClient* original_browser_client_;
  DevToolsManagerTestBrowserClient browser_client_;
};

TEST_F(DevToolsManagerTest, OpenAndManuallyCloseDevToolsClientHost) {
  DevToolsManagerImpl manager;

  DevToolsAgentHost* agent =
      DevToolsAgentHostRegistry::GetDevToolsAgentHost(rvh());
  DevToolsClientHost* host = manager.GetDevToolsClientHostFor(agent);
  EXPECT_TRUE(NULL == host);

  TestDevToolsClientHost client_host;
  manager.RegisterDevToolsClientHostFor(agent, &client_host);
  // Test that just registered devtools host is returned.
  host = manager.GetDevToolsClientHostFor(agent);
  EXPECT_TRUE(&client_host == host);
  EXPECT_EQ(0, TestDevToolsClientHost::close_counter);

  // Test that the same devtools host is returned.
  host = manager.GetDevToolsClientHostFor(agent);
  EXPECT_TRUE(&client_host == host);
  EXPECT_EQ(0, TestDevToolsClientHost::close_counter);

  client_host.Close(&manager);
  EXPECT_EQ(1, TestDevToolsClientHost::close_counter);
  host = manager.GetDevToolsClientHostFor(agent);
  EXPECT_TRUE(NULL == host);
}

TEST_F(DevToolsManagerTest, ForwardMessageToClient) {
  DevToolsManagerImpl manager;

  TestDevToolsClientHost client_host;
  DevToolsAgentHost* agent_host =
      DevToolsAgentHostRegistry::GetDevToolsAgentHost(rvh());
  manager.RegisterDevToolsClientHostFor(agent_host, &client_host);
  EXPECT_EQ(0, TestDevToolsClientHost::close_counter);

  std::string m = "test message";
  agent_host = DevToolsAgentHostRegistry::GetDevToolsAgentHost(rvh());
  manager.DispatchOnInspectorFrontend(agent_host, m);
  EXPECT_TRUE(&m == client_host.last_sent_message);

  client_host.Close(&manager);
  EXPECT_EQ(1, TestDevToolsClientHost::close_counter);
}

TEST_F(DevToolsManagerTest, NoUnresponsiveDialogInInspectedContents) {
  content::TestRenderViewHost* inspected_rvh = test_rvh();
  inspected_rvh->set_render_view_created(true);
  EXPECT_FALSE(contents()->GetDelegate());
  TestWebContentsDelegate delegate;
  contents()->SetDelegate(&delegate);

  TestDevToolsClientHost client_host;
  DevToolsAgentHost* agent_host =
      DevToolsAgentHostRegistry::GetDevToolsAgentHost(inspected_rvh);
  DevToolsManager::GetInstance()->
      RegisterDevToolsClientHostFor(agent_host, &client_host);

  // Start with a short timeout.
  inspected_rvh->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(10));
  // Wait long enough for first timeout and see if it fired.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, MessageLoop::QuitClosure(), TimeDelta::FromMilliseconds(10));
  MessageLoop::current()->Run();
  EXPECT_FALSE(delegate.renderer_unresponsive_received());

  // Now close devtools and check that the notification is delivered.
  client_host.Close(DevToolsManager::GetInstance());
  // Start with a short timeout.
  inspected_rvh->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(10));
  // Wait long enough for first timeout and see if it fired.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, MessageLoop::QuitClosure(), TimeDelta::FromMilliseconds(10));
  MessageLoop::current()->Run();
  EXPECT_TRUE(delegate.renderer_unresponsive_received());

  contents()->SetDelegate(NULL);
}

TEST_F(DevToolsManagerTest, ReattachOnCancelPendingNavigation) {
  contents()->transition_cross_site = true;
  // Navigate to URL.  First URL should use first RenderViewHost.
  const GURL url("http://www.google.com");
  controller().LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(rvh(), 1, url, content::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());

  TestDevToolsClientHost client_host;
  DevToolsManager* devtools_manager = DevToolsManager::GetInstance();
  devtools_manager->RegisterDevToolsClientHostFor(
      DevToolsAgentHostRegistry::GetDevToolsAgentHost(rvh()),
      &client_host);

  // Navigate to new site which should get a new RenderViewHost.
  const GURL url2("http://www.yahoo.com");
  controller().LoadURL(
      url2, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(contents()->cross_navigation_pending());
  EXPECT_EQ(&client_host, devtools_manager->GetDevToolsClientHostFor(
      DevToolsAgentHostRegistry::GetDevToolsAgentHost(pending_rvh())));

  // Interrupt pending navigation and navigate back to the original site.
  controller().LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_TYPED, std::string());
  contents()->TestDidNavigate(rvh(), 1, url, content::PAGE_TRANSITION_TYPED);
  EXPECT_FALSE(contents()->cross_navigation_pending());
  EXPECT_EQ(&client_host, devtools_manager->GetDevToolsClientHostFor(
      DevToolsAgentHostRegistry::GetDevToolsAgentHost(rvh())));
  client_host.Close(DevToolsManager::GetInstance());
}
