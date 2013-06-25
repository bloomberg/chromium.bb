// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Carbon/Carbon.h>

#include "base/debug/debugger.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"

namespace {

NSEvent* KeyEvent(const NSUInteger flags, const NSUInteger keyCode) {
  return [NSEvent keyEventWithType:NSKeyDown
                          location:NSZeroPoint
                     modifierFlags:flags
                         timestamp:0.0
                      windowNumber:0
                           context:nil
                        characters:@""
       charactersIgnoringModifiers:@""
                         isARepeat:NO
                          keyCode:keyCode];
}

class ChromeEventProcessingWindowTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    // Create a window.
    const NSUInteger mask = NSTitledWindowMask | NSClosableWindowMask |
        NSMiniaturizableWindowMask | NSResizableWindowMask;
    window_ = [[ChromeEventProcessingWindow alloc]
               initWithContentRect:NSMakeRect(0, 0, 800, 600)
                         styleMask:mask
                           backing:NSBackingStoreBuffered
                             defer:NO];
    if (base::debug::BeingDebugged()) {
      [window_ orderFront:nil];
    } else {
      [window_ orderBack:nil];
    }
  }

  virtual void TearDown() {
    [window_ close];
    CocoaTest::TearDown();
  }

  ChromeEventProcessingWindow* window_;
};

id CreateBrowserWindowControllerMock() {
  id delegate = [OCMockObject mockForClass:[BrowserWindowController class]];
  // Make conformsToProtocol return YES for @protocol(BrowserCommandExecutor)
  // to satisfy the DCHECK() in handleExtraKeyboardShortcut.
  [[[delegate stub] andReturnBool:YES]
    conformsToProtocol:[OCMArg conformsToProtocol:
                        @protocol(BrowserCommandExecutor)]];
  return delegate;
}

// Verify that the window intercepts a particular key event and
// forwards it to [delegate executeCommand:].  Assume that other
// CommandForKeyboardShortcut() will work the same for the rest.
TEST_F(ChromeEventProcessingWindowTest,
       PerformKeyEquivalentForwardToExecuteCommand) {
  NSEvent* event = KeyEvent(NSCommandKeyMask, kVK_ANSI_1);

  id delegate = CreateBrowserWindowControllerMock();
  [[delegate expect] executeCommand:IDC_SELECT_TAB_0];

  [window_ setDelegate:delegate];
  [window_ performKeyEquivalent:event];

  // Don't wish to mock all the way down...
  [window_ setDelegate:nil];
  EXPECT_OCMOCK_VERIFY(delegate);
}

// Verify that an unhandled shortcut does not get forwarded via
// -executeCommand:.
// TODO(shess) Think of a way to test that it is sent to the
// superclass.
TEST_F(ChromeEventProcessingWindowTest, PerformKeyEquivalentNoForward) {
  NSEvent* event = KeyEvent(0, 0);

  id delegate = CreateBrowserWindowControllerMock();

  [window_ setDelegate:delegate];
  [window_ performKeyEquivalent:event];

  // Don't wish to mock all the way down...
  [window_ setDelegate:nil];
  EXPECT_OCMOCK_VERIFY(delegate);
}

}  // namespace
