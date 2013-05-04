// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#ifndef CONTENT_PUBLIC_TEST_RENDER_VIEW_FAKE_RESOURCES_TEST_H_
#define CONTENT_PUBLIC_TEST_RENDER_VIEW_FAKE_RESOURCES_TEST_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_view_visitor.h"
#include "ipc/ipc_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

struct ResourceHostMsg_Request;

namespace IPC {
class Channel;
}

namespace WebKit {
class WebFrame;
class WebHistoryItem;
}

namespace content {
class MockRenderProcess;
class RenderThreadImpl;

class RenderViewFakeResourcesTest : public ::testing::Test,
                                    public IPC::Listener,
                                    public RenderViewVisitor {
 public:
  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // RenderViewVisitor implementation.
  virtual bool Visit(RenderView* render_view) OVERRIDE;

 protected:
  RenderViewFakeResourcesTest();
  virtual ~RenderViewFakeResourcesTest();

  // Call the base class SetUp and TearDown methods as part of your
  // test fixture's SetUp / TearDown.
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  RenderView* view();

  // Loads |url| into the RenderView, waiting for the load to finish.
  // Before loading the url, add any content that you want to return
  // to responses_.
  void LoadURL(const std::string& url);

  // Same as LoadURL, but sends a POST request.  Note that POST data is
  // not supported.
  void LoadURLWithPost(const std::string& url);

  // Navigates the main frame back in session history.
  void GoBack();

  // Navigates the main frame forward in session history.  Note that for
  // forward navigations, the caller needs to capture the WebHistoryItem
  // for the page to go forward to (before going back) and pass it to
  // this method.  The WebHistoryItem is available from the WebFrame.
  void GoForward(const WebKit::WebHistoryItem& history_item);

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

  base::MessageLoopForIO message_loop_;
  ContentRendererClient content_renderer_client_;
  // channel that the renderer uses to talk to the browser.
  // For this test, we will handle the browser end of the channel.
  scoped_ptr<IPC::Channel> channel_;
  RenderThreadImpl* render_thread_;  // owned by mock_process_
  scoped_ptr<MockRenderProcess> mock_process_;
  RenderView* view_;  // not owned, deletes itself on close

  // Map of url -> response body for network requests from the renderer.
  // Any urls not in this map are served a 404 error.
  std::map<std::string, std::string> responses_;

 private:
  // A helper for GoBack and GoForward.
  void GoToOffset(int offset, const WebKit::WebHistoryItem& history_item);

  // The previous state for whether sandbox support was enabled in
  // RenderViewWebKitPlatformSupportImpl.
  bool sandbox_was_enabled_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewFakeResourcesTest);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_RENDER_VIEW_FAKE_RESOURCES_TEST_H_
