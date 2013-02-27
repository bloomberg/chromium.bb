// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_RENDER_VIEW_TEST_H_
#define CONTENT_PUBLIC_TEST_RENDER_VIEW_TEST_H_

#include <string>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/main_function_params.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/test/mock_render_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/Platform.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

namespace WebKit {
class WebHistoryItem;
class WebWidget;
}

namespace gfx {
class Rect;
}

namespace content {
class MockRenderProcess;
class RendererMainPlatformDelegate;
class RendererWebKitPlatformSupportImplNoSandboxImpl;

class RenderViewTest : public testing::Test {
 public:
  // A special WebKitPlatformSupportImpl class for getting rid off the
  // dependency to the sandbox, which is not available in RenderViewTest.
  class RendererWebKitPlatformSupportImplNoSandbox {
   public:
    RendererWebKitPlatformSupportImplNoSandbox();
    ~RendererWebKitPlatformSupportImplNoSandbox();
    WebKit::Platform* Get();

   private:
    scoped_ptr<RendererWebKitPlatformSupportImplNoSandboxImpl>
        webkit_platform_support_;
  };

  RenderViewTest();
  virtual ~RenderViewTest();

 protected:
  // Spins the message loop to process all messages that are currently pending.
  void ProcessPendingMessages();

  // Returns a pointer to the main frame.
  WebKit::WebFrame* GetMainFrame();

  // Executes the given JavaScript in the context of the main frame. The input
  // is a NULL-terminated UTF-8 string.
  void ExecuteJavaScript(const char* js);

  // Executes the given JavaScript and sets the int value it evaluates to in
  // |result|.
  // Returns true if the JavaScript was evaluated correctly to an int value,
  // false otherwise.
  bool ExecuteJavaScriptAndReturnIntValue(const string16& script, int* result);

  // Loads the given HTML into the main frame as a data: URL and blocks until
  // the navigation is committed.
  void LoadHTML(const char* html);

  // Navigates the main frame back or forward in session history and commits.
  // The caller must capture a WebHistoryItem for the target page. This is
  // available from the WebFrame.
  void GoBack(const WebKit::WebHistoryItem& item);
  void GoForward(const WebKit::WebHistoryItem& item);

  // Sends one native key event over IPC.
  void SendNativeKeyEvent(const NativeWebKeyboardEvent& key_event);

  // Send a raw keyboard event to the renderer.
  void SendWebKeyboardEvent(const WebKit::WebKeyboardEvent& key_event);

  // Send a raw mouse event to the renderer.
  void SendWebMouseEvent(const WebKit::WebMouseEvent& key_event);

  // Returns the bounds (coordinates and size) of the element with id
  // |element_id|.  Returns an empty rect if such an element was not found.
  gfx::Rect GetElementBounds(const std::string& element_id);

  // Sends a left mouse click in the middle of the element with id |element_id|.
  // Returns true if the event was sent, false otherwise (typically because
  // the element was not found).
  bool SimulateElementClick(const std::string& element_id);

  // Simulates |node| being focused.
  void SetFocused(const WebKit::WebNode& node);

  // Clears anything associated with the browsing history.
  void ClearHistory();

  // Simulates a navigation with a type of reload to the given url.
  void Reload(const GURL& url);

  // Returns the IPC message ID of the navigation message.
  uint32 GetNavigationIPCType();

  // Resize the view.
  void Resize(gfx::Size new_size,
              gfx::Rect resizer_rect,
              bool is_fullscreen);

  // These are all methods from RenderViewImpl that we expose to testing code.
  bool OnMessageReceived(const IPC::Message& msg);
  void DidNavigateWithinPage(WebKit::WebFrame* frame, bool is_new_navigation);
  void SendContentStateImmediately();
  WebKit::WebWidget* GetWebWidget();

  // testing::Test
  virtual void SetUp() OVERRIDE;

  virtual void TearDown() OVERRIDE;

  MessageLoop msg_loop_;
  scoped_ptr<MockRenderProcess> mock_process_;
  // We use a naked pointer because we don't want to expose RenderViewImpl in
  // the embedder's namespace.
  RenderView* view_;
  RendererWebKitPlatformSupportImplNoSandbox webkit_platform_support_;
  ContentRendererClient content_renderer_client_;
  scoped_ptr<MockRenderThread> render_thread_;

  // Used to setup the process so renderers can run.
  scoped_ptr<RendererMainPlatformDelegate> platform_;
  scoped_ptr<MainFunctionParams> params_;
  scoped_ptr<CommandLine> command_line_;

 private:
  void GoToOffset(int offset, const WebKit::WebHistoryItem& history_item);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_RENDER_VIEW_TEST_H_
