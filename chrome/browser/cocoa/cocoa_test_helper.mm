// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "base/logging.h"

@implementation CocoaTestHelperWindow

- (id)initWithContentRect:(NSRect)contentRect {
  return [self initWithContentRect:contentRect
                         styleMask:NSBorderlessWindowMask
                           backing:NSBackingStoreBuffered
                             defer:NO];
}

- (id)init {
  return [self initWithContentRect:NSMakeRect(0, 0, 800, 600)];
}

- (void)dealloc {
  // Just a good place to put breakpoints when having problems with
  // unittests and CocoaTestHelperWindow.
  [super dealloc];
}

- (void)makePretendKeyWindowAndSetFirstResponder:(NSResponder*)responder {
  EXPECT_TRUE([self makeFirstResponder:responder]);
  [self setPretendIsKeyWindow:YES];
}

- (void)clearPretendKeyWindowAndFirstResponder {
  [self setPretendIsKeyWindow:NO];
  EXPECT_TRUE([self makeFirstResponder:NSApp]);
}

- (void)setPretendIsKeyWindow:(BOOL)flag {
  pretendIsKeyWindow_ = flag;
}

- (BOOL)isKeyWindow {
  return pretendIsKeyWindow_;
}

@end

CocoaTest::CocoaTest() : called_tear_down_(false), test_window_(nil) {
  BootstrapCocoa();
  // Set the duration of AppKit-evaluated animations (such as frame changes)
  // to zero for testing purposes. That way they take effect immediately.
  [[NSAnimationContext currentContext] setDuration:0.0];
  // Collect the list of windows that were open when the test started so
  // that we don't wait for them to close in TearDown. Has to be done
  // after BootstrapCocoa is called.
  initial_windows_ = ApplicationWindows();
}

CocoaTest::~CocoaTest() {
  // Must call CocoaTest's teardown from your overrides.
  DCHECK(called_tear_down_);
}

void CocoaTest::BootstrapCocoa() {
  // Look in the framework bundle for resources.
  FilePath path;
  PathService::Get(base::DIR_EXE, &path);
  path = path.Append(chrome::kFrameworkName);
  mac_util::SetOverrideAppBundlePath(path);

  // Bootstrap Cocoa. It's very unhappy without this.
  [NSApplication sharedApplication];
}

void CocoaTest::TearDown() {
  called_tear_down_ = true;
  // Call close on our test_window to clean it up if one was opened.
  [test_window_ close];
  test_window_ = nil;

  // Recycle the pool to clean up any stuff that was put on the
  // autorelease pool due to window or windowcontroller closures.
  // Note that many controls (NSTextFields, NSComboboxes etc) may call
  // performSelector:withDelay: to clean up drag handlers and other things.
  // We must spin the event loop a bit to make sure that everything gets cleaned
  // up correctly. We will wait up to one second for windows to clean themselves
  // up (normally only takes one to two loops through the event loop).
  // Radar 5851458 "Closing a window with a NSTextView in it should get rid of
  // it immediately"
  pool_.Recycle();
  NSDate *start_date = [NSDate date];
  const std::vector<NSWindow*> windows_waiting(ApplicationWindows());

  bool loop = windows_waiting.size() > 0;
  while (loop) {
    {
      // Need an autorelease pool to wrap our event loop.
      base::ScopedNSAutoreleasePool pool;
      NSEvent *next_event = [NSApp nextEventMatchingMask:NSAnyEventMask
                                               untilDate:nil
                                                  inMode:NSDefaultRunLoopMode
                                                 dequeue:YES];
      [NSApp sendEvent:next_event];
      [NSApp updateWindows];
    }
    // Check the windows after we have released the event loop pool so that
    // all retains are cleaned up.
    const std::vector<NSWindow*> current_windows(ApplicationWindows());
    std::vector<NSWindow*> windows_left;
    std::set_difference(current_windows.begin(),
                        current_windows.end(),
                        initial_windows_.begin(),
                        initial_windows_.end(),
                        inserter(windows_left, windows_left.begin()));

    if (windows_left.size() == 0) {
      // All our windows are closed.
      break;
    }
    if ([start_date timeIntervalSinceNow] < -1.0) {
      // Took us over a second to shut down, and windows still exist.
      // Log a failure and continue.
      EXPECT_EQ(windows_left.size(), 0U);
      for (size_t i = 0; i < windows_left.size(); ++i) {
        const char* desc = [[windows_left[i] description] UTF8String];
        LOG(WARNING) << "Didn't close window " << desc;
      }
      break;
    }
  }
  PlatformTest::TearDown();
}

std::vector<NSWindow*> CocoaTest::ApplicationWindows() {
  // This must NOT retain the windows it is returning.
  std::vector<NSWindow*> windows;
  // Must create a pool here because [NSApp windows] has created an array
  // with retains on all the windows in it.
  base::ScopedNSAutoreleasePool pool;
  NSArray *appWindows = [NSApp windows];
  for (NSWindow *window in appWindows) {
    windows.push_back(window);
  }
  return windows;
}

CocoaTestHelperWindow* CocoaTest::test_window() {
  if (!test_window_) {
    test_window_ = [[CocoaTestHelperWindow alloc] init];
    if (DebugUtil::BeingDebugged()) {
      [test_window_ orderFront:nil];
    } else {
      [test_window_ orderBack:nil];
    }
  }
  return test_window_;
}
