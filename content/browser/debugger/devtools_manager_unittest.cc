// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/time.h"
#include "content/browser/content_browser_client.h"
#include "content/browser/debugger/devtools_client_host.h"
#include "content/browser/debugger/devtools_manager.h"
#include "content/browser/mock_content_browser_client.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

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

  virtual void Close() {
    EXPECT_FALSE(closed_);
    close_counter++;
    NotifyCloseListener();
    closed_ = true;
  }
  virtual void InspectedTabClosing() {
    Close();
  }

  virtual void SetInspectedTabUrl(const std::string& url) {
  }

  virtual void SendMessageToClient(const IPC::Message& message) {
    last_sent_message = &message;
  }

  virtual void TabReplaced(TabContents* new_tab) {
  }

  static void ResetCounters() {
    close_counter = 0;
  }

  static int close_counter;

  const IPC::Message* last_sent_message;

 private:
  bool closed_;

  virtual void FrameNavigating(const std::string& url) {}

  DISALLOW_COPY_AND_ASSIGN(TestDevToolsClientHost);
};

int TestDevToolsClientHost::close_counter = 0;


class TestTabContentsDelegate : public TabContentsDelegate {
 public:
  TestTabContentsDelegate() : renderer_unresponsive_received_(false) {}

  // Notification that the tab is hung.
  virtual void RendererUnresponsive(TabContents* source) {
    renderer_unresponsive_received_ = true;
  }

  bool renderer_unresponsive_received() const {
    return renderer_unresponsive_received_;
  }

 private:
  bool renderer_unresponsive_received_;
};

class DevToolsManagerTestBrowserClient
    : public content::MockContentBrowserClient {
 public:
  DevToolsManagerTestBrowserClient() {
  }

  virtual DevToolsManager* GetDevToolsManager() OVERRIDE {
    return &dev_tools_manager_;
  }

 private:
  DevToolsManager dev_tools_manager_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsManagerTestBrowserClient);
};

}  // namespace

class DevToolsManagerTest : public RenderViewHostTestHarness {
 public:
  DevToolsManagerTest() : RenderViewHostTestHarness() {
  }

 protected:
  virtual void SetUp() OVERRIDE {
    original_browser_client_ = content::GetContentClient()->browser();
    content::GetContentClient()->set_browser(&browser_client_);

    RenderViewHostTestHarness::SetUp();
    TestDevToolsClientHost::ResetCounters();
  }

  virtual void TearDown() OVERRIDE {
    RenderViewHostTestHarness::TearDown();
    content::GetContentClient()->set_browser(original_browser_client_);
  }

 private:
  content::ContentBrowserClient* original_browser_client_;
  DevToolsManagerTestBrowserClient browser_client_;
};

TEST_F(DevToolsManagerTest, OpenAndManuallyCloseDevToolsClientHost) {
  DevToolsManager manager;

  DevToolsClientHost* host = manager.GetDevToolsClientHostFor(rvh());
  EXPECT_TRUE(NULL == host);

  TestDevToolsClientHost client_host;
  manager.RegisterDevToolsClientHostFor(rvh(), &client_host);
  // Test that just registered devtools host is returned.
  host = manager.GetDevToolsClientHostFor(rvh());
  EXPECT_TRUE(&client_host == host);
  EXPECT_EQ(0, TestDevToolsClientHost::close_counter);

  // Test that the same devtools host is returned.
  host = manager.GetDevToolsClientHostFor(rvh());
  EXPECT_TRUE(&client_host == host);
  EXPECT_EQ(0, TestDevToolsClientHost::close_counter);

  client_host.Close();
  EXPECT_EQ(1, TestDevToolsClientHost::close_counter);
  host = manager.GetDevToolsClientHostFor(rvh());
  EXPECT_TRUE(NULL == host);
}

TEST_F(DevToolsManagerTest, ForwardMessageToClient) {
  DevToolsManager manager;

  TestDevToolsClientHost client_host;
  manager.RegisterDevToolsClientHostFor(rvh(), &client_host);
  EXPECT_EQ(0, TestDevToolsClientHost::close_counter);

  IPC::Message m;
  manager.ForwardToDevToolsClient(rvh(), m);
  EXPECT_TRUE(&m == client_host.last_sent_message);

  client_host.Close();
  EXPECT_EQ(1, TestDevToolsClientHost::close_counter);
}

TEST_F(DevToolsManagerTest, NoUnresponsiveDialogInInspectedTab) {
  TestRenderViewHost* inspected_rvh = rvh();
  inspected_rvh->set_render_view_created(true);
  EXPECT_FALSE(contents()->delegate());
  TestTabContentsDelegate delegate;
  contents()->set_delegate(&delegate);

  TestDevToolsClientHost client_host;
  content::GetContentClient()->browser()->GetDevToolsManager()->
      RegisterDevToolsClientHostFor(inspected_rvh, &client_host);

  // Start with a short timeout.
  inspected_rvh->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(10));
  // Wait long enough for first timeout and see if it fired.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new MessageLoop::QuitTask(), 10);
  MessageLoop::current()->Run();
  EXPECT_FALSE(delegate.renderer_unresponsive_received());

  // Now close devtools and check that the notification is delivered.
  client_host.Close();
  // Start with a short timeout.
  inspected_rvh->StartHangMonitorTimeout(TimeDelta::FromMilliseconds(10));
  // Wait long enough for first timeout and see if it fired.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new MessageLoop::QuitTask(), 10);
  MessageLoop::current()->Run();
  EXPECT_TRUE(delegate.renderer_unresponsive_received());

  contents()->set_delegate(NULL);
}
