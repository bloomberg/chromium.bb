// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/string16.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/renderer/render_view_impl.h"
#include "content/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>  // for the kVK_* constants.

using content::NativeWebKeyboardEvent;
using content::RenderViewTest;

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
                          location:NSMakePoint(0, 0)
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
  <html>
  <head><title></title>
  <script type='text/javascript' language='javascript'>
  function OnKeyEvent(ev) {
    var result = document.getElementById(ev.type);
    result.innerText = (ev.which || ev.keyCode) + ',' +
      ev.shiftKey + ',' +
      ev.ctrlKey + ',' +
      ev.metaKey + ',' +
      ev.altKey;
    return %s;  /* Replace with "return true;" when testing in an html file. */
  }
  function OnScroll(ev) {
    var result = document.getElementById("scroll");
    result.innerText = window.pageYOffset;
    return true;
  }
  </script>
  <style type="text/css">
  p { border-bottom:5000px solid black; } /* enforce vertical scroll bar */
  </style>
  </head>
  <body
    onscroll='return OnScroll(event);'
    onkeydown='return OnKeyEvent(event);'>
  <div id='keydown' contenteditable='true'> </div>
  <div id='scroll' contenteditable='true'> </div>
  <p>p1
  <p>p2
  </body>
  </html>
  );
  #undef HTML

  webkit_glue::WebPreferences prefs;
  prefs.enable_scroll_animator = false;

  RenderViewImpl* view = static_cast<RenderViewImpl*>(view_);
  view->OnUpdateWebPreferences(prefs);

  const int kMaxOutputCharacters = 1024;
  string16 output;
  char htmlBuffer[2048];

  NSEvent* arrowDownKeyDown = CmdDeadKeyEvent(NSKeyDown, kVK_DownArrow);
  NSEvent* arrowUpKeyDown = CmdDeadKeyEvent(NSKeyDown, kVK_UpArrow);

  // First test when javascript does not eat keypresses -- should scroll.
  sprintf(htmlBuffer, kRawHtml, "true");
  view->set_send_content_state_immediately(true);
  LoadHTML(htmlBuffer);
  render_thread_->sink().ClearMessages();

  const char* kArrowDownScrollDown =
      "40,false,false,true,false\n10128\np1\n\np2";
  view->OnSetEditCommandsForNextKeyEvent(
      EditCommands(1, EditCommand("moveToEndOfDocument", "")));
  SendNativeKeyEvent(NativeWebKeyboardEvent(arrowDownKeyDown));
  ProcessPendingMessages();
  output = GetMainFrame()->contentAsText(kMaxOutputCharacters);
  EXPECT_EQ(kArrowDownScrollDown, UTF16ToASCII(output));

  const char* kArrowUpScrollUp =
      "38,false,false,true,false\n0\np1\n\np2";
  view->OnSetEditCommandsForNextKeyEvent(
      EditCommands(1, EditCommand("moveToBeginningOfDocument", "")));
  SendNativeKeyEvent(NativeWebKeyboardEvent(arrowUpKeyDown));
  ProcessPendingMessages();
  output = GetMainFrame()->contentAsText(kMaxOutputCharacters);
  EXPECT_EQ(kArrowUpScrollUp, UTF16ToASCII(output));


  // Now let javascript eat the key events -- no scrolling should happen
  sprintf(htmlBuffer, kRawHtml, "false");
  view->set_send_content_state_immediately(true);
  LoadHTML(htmlBuffer);
  render_thread_->sink().ClearMessages();

  const char* kArrowDownNoScroll =
      "40,false,false,true,false\np1\n\np2";
  view->OnSetEditCommandsForNextKeyEvent(
      EditCommands(1, EditCommand("moveToEndOfDocument", "")));
  SendNativeKeyEvent(NativeWebKeyboardEvent(arrowDownKeyDown));
  ProcessPendingMessages();
  output = GetMainFrame()->contentAsText(kMaxOutputCharacters);
  EXPECT_EQ(kArrowDownNoScroll, UTF16ToASCII(output));

  const char* kArrowUpNoScroll =
      "38,false,false,true,false\np1\n\np2";
  view->OnSetEditCommandsForNextKeyEvent(
      EditCommands(1, EditCommand("moveToBeginningOfDocument", "")));
  SendNativeKeyEvent(NativeWebKeyboardEvent(arrowUpKeyDown));
  ProcessPendingMessages();
  output = GetMainFrame()->contentAsText(kMaxOutputCharacters);
  EXPECT_EQ(kArrowUpNoScroll, UTF16ToASCII(output));
}

