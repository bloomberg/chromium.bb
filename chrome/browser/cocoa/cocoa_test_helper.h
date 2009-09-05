// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_COCOA_TEST_HELPER
#define CHROME_BROWSER_COCOA_COCOA_TEST_HELPER

#import <Cocoa/Cocoa.h>

#include "base/debug_util.h"
#include "base/file_path.h"
#include "base/mac_util.h"
#include "base/path_service.h"
#import "base/scoped_nsobject.h"
#include "chrome/common/mac_app_names.h"

// Background windows normally will not display things such as focus
// rings.  This class allows -isKeyWindow to be manipulated to test
// such things.

@interface CocoaTestHelperWindow : NSWindow {
 @private
  BOOL pretendIsKeyWindow_;
}

// Init a borderless non-defered window with backing store.
- (id)initWithContentRect:(NSRect)contentRect;

// Init with a default frame.
- (id)init;

// Set value to return for -isKeyWindow.
- (void)setPretendIsKeyWindow:(BOOL)isKeyWindow;

- (BOOL)isKeyWindow;

@end

// A class that initializes Cocoa and sets up resources for many of our
// Cocoa controller unit tests. It does several key things:
//   - Creates and displays an empty Cocoa window for views to live in
//   - Loads the appropriate bundle so nib loading works. When loading the
//     nib in the class being tested, your must use |mac_util::MainAppBundle()|
//     as the bundle. If you do not specify a bundle, your test will likely
//     fail.
// It currently does not create an autorelease pool, though that can easily be
// added. If your test wants one, it can derrive from PlatformTest instead of
// testing::Test.

class CocoaTestHelper {
 public:
  CocoaTestHelper() {
    // Look in the Chromium app bundle for resources.
    FilePath path;
    PathService::Get(base::DIR_EXE, &path);
    path = path.AppendASCII(MAC_BROWSER_APP_NAME);
    mac_util::SetOverrideAppBundlePath(path);

    // Bootstrap Cocoa. It's very unhappy without this.
    [NSApplication sharedApplication];

    // Create a window.
    window_.reset([[CocoaTestHelperWindow alloc] init]);
    if (DebugUtil::BeingDebugged()) {
      [window_ orderFront:nil];
    } else {
      [window_ orderBack:nil];
    }

    // Set the duration of AppKit-evaluated animations (such as frame changes)
    // to zero for testing purposes. That way they take effect immediately.
    [[NSAnimationContext currentContext] setDuration:0.0];
  }

  // Access the Cocoa window created for the test.
  NSWindow* window() const { return window_.get(); }
  NSView* contentView() const { return [window_ contentView]; }

  // Set |window_| to pretend to be key and make |aView| its
  // firstResponder.
  void makeFirstResponder(NSView* aView) {
    [window_ setPretendIsKeyWindow:YES];
    [window_ makeFirstResponder:aView];
  }

  // Clear |window_| firstResponder and stop pretending to be key.
  void clearFirstResponder() {
    [window_ makeFirstResponder:nil];
    [window_ setPretendIsKeyWindow:NO];
  }

 private:
  scoped_nsobject<CocoaTestHelperWindow> window_;
};

#endif  // CHROME_BROWSER_COCOA_COCOA_TEST_HELPER
