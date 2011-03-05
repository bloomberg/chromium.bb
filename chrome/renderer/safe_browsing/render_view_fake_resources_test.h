// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderViewFakeResourcesTest can be used as a base class for tests that need
// to simulate loading network resources (such as http: urls) into a
// RenderView.  It does this by handling the relevant IPC messages that the
// renderer would normally send to the browser, and responding with static
// content from an internal map.  A request for a url that is not in the map
// will return a 404.  Currently, content is always returned as text/html, and
// there is no support for handling POST data.
//
// RenderViewFakeResourcesTest sets up a MessageLoop (message_loop_) that
// can be used by the subclass to post or process tasks.
//
// Note that since WebKit cannot be safely shut down and re-initialized,
// any test that uses this base class should run as part of browser_tests
// so that the test is launched in its own process.
//
// Typical usage:
//
// class MyTest : public RenderVieFakeResourcesTest {
//  protected:
//   virtual void SetUp() {
//     RenderViewFakeResourcesTest::SetUp();
//     <insert test-specific setup>
//   }
//
//   virtual void TearDown() {
//     <insert test-specific teardown>
//     RenderViewFakeResourcesTest::TearDown();
//   }
//   ...
// };
//
// TEST_F(MyTest, TestFoo) {
//   responses_["http://host.com/"] = "<html><body>some content</body></html>";
//   LoadURL("http://host.com/");
//   ...
// }

#ifndef CHROME_RENDERER_SAFE_BROWSING_RENDER_VIEW_FAKE_RESOURCES_TEST_H_
#define CHROME_RENDERER_SAFE_BROWSING_RENDER_VIEW_FAKE_RESOURCES_TEST_H_

#include <map>
#include <string>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "chrome/renderer/render_view_visitor.h"
#include "ipc/ipc_channel.h"
#include "testing/gtest/include/gtest/gtest.h"

class CommandLine;
class MockRenderProcess;
class RendererMainPlatformDelegate;
class RenderThread;
class RenderView;
class SandboxInitWrapper;
struct MainFunctionParams;
struct ResourceHostMsg_Request;

namespace WebKit {
class WebFrame;
}

namespace safe_browsing {

class RenderViewFakeResourcesTest : public ::testing::Test,
                                    public IPC::Channel::Listener,
                                    public RenderViewVisitor {
 public:
  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // RenderViewVisitor implementation.
  virtual bool Visit(RenderView* render_view);

 protected:
  RenderViewFakeResourcesTest();
  virtual ~RenderViewFakeResourcesTest();

  // Call the base class SetUp and TearDown methods as part of your
  // test fixture's SetUp / TearDown.
  virtual void SetUp();
  virtual void TearDown();

  // Loads |url| into the RenderView, waiting for the load to finish.
  // Before loading the url, add any content that you want to return
  // to responses_.
  void LoadURL(const std::string& url);

  // Same as LoadURL, but sends a POST request.  Note that POST data is
  // not supported.
  void LoadURLWithPost(const std::string& url);

  // Navigates the main frame back in session history.
  void GoBack();

  // Returns the main WebFrame for our RenderView.
  WebKit::WebFrame* GetMainFrame();

  // IPC message handlers below

  // Notification that page load has finished.  Exit the message loop
  // so that the test can continue.
  void OnDidStopLoading();

  // Notification that the renderer wants to load a resource.
  // If the requested url is in responses_, we send the renderer a 200
  // and the supplied content, otherwise we send it a 404 error.
  void OnRequestResource(const IPC::Message& message,
                         int request_id,
                         const ResourceHostMsg_Request& request_data);

  // Notification that the render view we've created is ready to use.
  void OnRenderViewReady();

  static const int32 kViewId;  // arbitrary id for our testing view

  MessageLoopForIO message_loop_;
  // channel that the renderer uses to talk to the browser.
  // For this test, we will handle the browser end of the channel.
  scoped_ptr<IPC::Channel> channel_;
  RenderThread* render_thread_;  // owned by mock_process_
  scoped_ptr<MockRenderProcess> mock_process_;
  RenderView* view_;  // not owned, deletes itself on close
  scoped_ptr<RendererMainPlatformDelegate> platform_;
  scoped_ptr<MainFunctionParams> params_;
  scoped_ptr<CommandLine> command_line_;
  scoped_ptr<SandboxInitWrapper> sandbox_init_wrapper_;

  // Map of url -> response body for network requests from the renderer.
  // Any urls not in this map are served a 404 error.
  std::map<std::string, std::string> responses_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderViewFakeResourcesTest);
};

}  // namespace safe_browsing

#endif  // CHROME_RENDERER_SAFE_BROWSING_RENDER_VIEW_FAKE_RESOURCES_TEST_H_
