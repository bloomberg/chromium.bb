// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_replication_state.h"
#include "content/common/input_messages.h"
#include "content/common/text_input_client_messages.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/render_view_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebFrameContentDumper.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

#include <Carbon/Carbon.h>  // for the kVK_* constants.
#include <Cocoa/Cocoa.h>

using blink::WebFrameContentDumper;
using blink::WebCompositionUnderline;

namespace content {

NSEvent* CmdDeadKeyEvent(NSEventType type, unsigned short code) {
  UniChar uniChar = 0;
  switch(code) {
    case kVK_UpArrow:
      uniChar = NSUpArrowFunctionKey;
      break;
    case kVK_DownArrow:
      uniChar = NSDownArrowFunctionKey;
      break;
    default:
      CHECK(false);
  }
  NSString* s = [NSString stringWithFormat:@"%C", uniChar];

  return [NSEvent keyEventWithType:type
                          location:NSZeroPoint
                     modifierFlags:NSCommandKeyMask
                         timestamp:0.0
                      windowNumber:0
                           context:nil
                        characters:s
       charactersIgnoringModifiers:s
                         isARepeat:NO
                           keyCode:code];
}

// Test that cmd-up/down scrolls the page exactly if it is not intercepted by
// javascript.
TEST_F(RenderViewTest, MacTestCmdUp) {
  // Some preprocessor trickery so that we can have literal html in our source,
  // makes it easier to copy html to and from an html file for testing (the
  // preprocessor will remove the newlines at the line ends, turning this into
  // a single long line).
  #define HTML(s) #s
  const char* kRawHtml = HTML(
  <!DOCTYPE html>
  <style>
    /* Add a vertical scrollbar */
    body { height: 10128px; }
  </style>
  <div id='keydown'></div>
  <div id='scroll'></div>
  <script>
    var allowKeyEvents = true;
    var scroll = document.getElementById('scroll');
    var result = document.getElementById('keydown');
    onkeydown = function(event) {
      result.textContent =
        event.keyCode + ',' +
        event.shiftKey + ',' +
        event.ctrlKey + ',' +
        event.metaKey + ',' +
        event.altKey;
      return allowKeyEvents;
    }
  </script>
  <!--
    TODO(esprehn): For some strange reason we need a non-empty document for
    scrolling to work. This is not the case in a real browser only in the test.
  -->
  <p>p1
  );
  #undef HTML

  WebPreferences prefs;
  prefs.enable_scroll_animator = false;

  RenderViewImpl* view = static_cast<RenderViewImpl*>(view_);
  view->OnUpdateWebPreferences(prefs);

  const int kMaxOutputCharacters = 1024;
  std::string output;

  NSEvent* arrowDownKeyDown = CmdDeadKeyEvent(NSKeyDown, kVK_DownArrow);
  NSEvent* arrowUpKeyDown = CmdDeadKeyEvent(NSKeyDown, kVK_UpArrow);

  // First test when javascript does not eat keypresses -- should scroll.
  view->set_send_content_state_immediately(true);
  LoadHTML(kRawHtml);
  render_thread_->sink().ClearMessages();

  const char* kArrowDownScrollDown = "40,false,false,true,false\n10144\np1";
  view->OnSetEditCommandsForNextKeyEvent(
      EditCommands(1, EditCommand("moveToEndOfDocument", "")));
  SendNativeKeyEvent(NativeWebKeyboardEvent(arrowDownKeyDown));
  ProcessPendingMessages();
  ExecuteJavaScriptForTests("scroll.textContent = window.pageYOffset");
  output = WebFrameContentDumper::dumpWebViewAsText(view->GetWebView(),
                                                    kMaxOutputCharacters)
               .ascii();
  EXPECT_EQ(kArrowDownScrollDown, output);

  const char* kArrowUpScrollUp = "38,false,false,true,false\n0\np1";
  view->OnSetEditCommandsForNextKeyEvent(
      EditCommands(1, EditCommand("moveToBeginningOfDocument", "")));
  SendNativeKeyEvent(NativeWebKeyboardEvent(arrowUpKeyDown));
  ProcessPendingMessages();
  ExecuteJavaScriptForTests("scroll.textContent = window.pageYOffset");
  output = WebFrameContentDumper::dumpWebViewAsText(view->GetWebView(),
                                                    kMaxOutputCharacters)
               .ascii();
  EXPECT_EQ(kArrowUpScrollUp, output);

  // Now let javascript eat the key events -- no scrolling should happen.
  // Set a scroll position slightly down the page to ensure that it does not
  // move.
  ExecuteJavaScriptForTests("allowKeyEvents = false; window.scrollTo(0, 100)");

  const char* kArrowDownNoScroll = "40,false,false,true,false\n100\np1";
  view->OnSetEditCommandsForNextKeyEvent(
      EditCommands(1, EditCommand("moveToEndOfDocument", "")));
  SendNativeKeyEvent(NativeWebKeyboardEvent(arrowDownKeyDown));
  ProcessPendingMessages();
  ExecuteJavaScriptForTests("scroll.textContent = window.pageYOffset");
  output = WebFrameContentDumper::dumpWebViewAsText(view->GetWebView(),
                                                    kMaxOutputCharacters)
               .ascii();
  EXPECT_EQ(kArrowDownNoScroll, output);

  const char* kArrowUpNoScroll = "38,false,false,true,false\n100\np1";
  view->OnSetEditCommandsForNextKeyEvent(
      EditCommands(1, EditCommand("moveToBeginningOfDocument", "")));
  SendNativeKeyEvent(NativeWebKeyboardEvent(arrowUpKeyDown));
  ProcessPendingMessages();
  ExecuteJavaScriptForTests("scroll.textContent = window.pageYOffset");
  output = WebFrameContentDumper::dumpWebViewAsText(view->GetWebView(),
                                                    kMaxOutputCharacters)
               .ascii();
  EXPECT_EQ(kArrowUpNoScroll, output);
}

// TODO(ekaramad): This test could be removed once we do not send irrelevant
// IPCs from browser during the time RenderViewImpl is swapped out
// (https://crbug.com/669219).
// This test verfies that when RenderViewImpl is swapped out, handling IPCs
// which need a WebFrameWidget will not lead to a crash.
TEST_F(RenderViewTest, HandleIPCsInSwappedOutState) {
  LoadHTML("<input/>");

  // Normally, we have a WebFrameWidget.
  EXPECT_TRUE(GetWebWidget()->isWebFrameWidget());

  // Swap out the main frame so that the frame widget is destroyed.
  auto* view = static_cast<RenderViewImpl*>(view_);
  auto* main_frame = view->GetMainRenderFrame();
  main_frame->OnMessageReceived(FrameMsg_SwapOut(
      main_frame->GetRoutingID(), 123, true, FrameReplicationState()));

  // We no longer have a frame widget.
  EXPECT_FALSE(GetWebWidget()->isWebFrameWidget());

  int routing_id = view->GetRoutingID();
  // Now simulate some TextInputClientMac IPCs. These will be handled by
  // RenderWidget which forwards them to the TextInputClientObserver
  using Range = gfx::Range;
  using Point = gfx::Point;
  view->OnMessageReceived(
      TextInputClientMsg_CharacterIndexForPoint(routing_id, Point()));
  view->OnMessageReceived(
      TextInputClientMsg_FirstRectForCharacterRange(routing_id, Range()));
  view->OnMessageReceived(
      TextInputClientMsg_StringForRange(routing_id, Range()));
  view->OnMessageReceived(
      TextInputClientMsg_CharacterIndexForPoint(routing_id, Point()));

  // Simulate some IME related IPCs.
  using Text = base::string16;
  using Underlines = std::vector<blink::WebCompositionUnderline>;
  view->OnMessageReceived(InputMsg_ImeSetComposition(
      routing_id, Text(), Underlines(), Range(), 0, 0));
  view->OnMessageReceived(
      InputMsg_ImeCommitText(routing_id, Text(), Underlines(), Range(), 0));
  view->OnMessageReceived(InputMsg_ImeFinishComposingText(routing_id, false));
}

}  // namespace content
