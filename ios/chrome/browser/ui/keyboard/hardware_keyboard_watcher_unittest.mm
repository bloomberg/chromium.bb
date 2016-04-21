// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/keyboard/hardware_keyboard_watcher.h"

#include "base/mac/scoped_nsobject.h"
#include "base/test/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

void PostKeyboardWillChangeNotification(CGRect beginFrame, CGRect endFrame) {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIKeyboardWillChangeFrameNotification
                    object:nil
                  userInfo:@{
                    UIKeyboardFrameBeginUserInfoKey :
                        [NSValue valueWithCGRect:beginFrame],
                    UIKeyboardFrameEndUserInfoKey :
                        [NSValue valueWithCGRect:endFrame],
                  }];
}

class HardwareKeyboardWatcherTest : public PlatformTest {
 public:
  HardwareKeyboardWatcherTest() {
    _window.reset(
        [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]]);
    [_window makeKeyAndVisible];
  }

 protected:
  base::scoped_nsobject<UIWindow> _window;
};

TEST_F(HardwareKeyboardWatcherTest, AccessoryViewNotInHierarchy_NoHistogram) {
  base::HistogramTester histogram_tester;
  id mockView = [OCMockObject niceMockForClass:[UIView class]];
  [[mockView stub] andReturn:nil];
  base::scoped_nsobject<HardwareKeyboardWatcher> watcher(
      [[HardwareKeyboardWatcher alloc] initWithAccessoryView:mockView]);

  PostKeyboardWillChangeNotification(CGRectZero, CGRectZero);
  PostKeyboardWillChangeNotification(CGRectInfinite, CGRectInfinite);

  histogram_tester.ExpectTotalCount("Omnibox.HardwareKeyboardModeEnabled", 0);
}

TEST_F(HardwareKeyboardWatcherTest, EmptyBeginFrame_SoftwareKeyboardHistogram) {
  base::HistogramTester histogram_tester;
  id mockView = [OCMockObject niceMockForClass:[UIView class]];
  [[[mockView stub] andReturn:[[UIApplication sharedApplication] keyWindow]]
      window];
  base::scoped_nsobject<HardwareKeyboardWatcher> watcher(
      [[HardwareKeyboardWatcher alloc] initWithAccessoryView:mockView]);

  PostKeyboardWillChangeNotification(CGRectZero, CGRectInfinite);

  histogram_tester.ExpectUniqueSample("Omnibox.HardwareKeyboardModeEnabled",
                                      false, 1);
}

TEST_F(HardwareKeyboardWatcherTest, EmptyEndFrame_SoftwareKeyboardHistogram) {
  base::HistogramTester histogram_tester;
  id mockView = [OCMockObject niceMockForClass:[UIView class]];
  [[[mockView stub] andReturn:[[UIApplication sharedApplication] keyWindow]]
      window];
  base::scoped_nsobject<HardwareKeyboardWatcher> watcher(
      [[HardwareKeyboardWatcher alloc] initWithAccessoryView:mockView]);

  PostKeyboardWillChangeNotification(CGRectInfinite, CGRectZero);

  histogram_tester.ExpectUniqueSample("Omnibox.HardwareKeyboardModeEnabled",
                                      false, 1);
}

TEST_F(HardwareKeyboardWatcherTest,
       KeyboardFullyOnScreen_SoftwareKeyboardHistogram) {
  base::HistogramTester histogram_tester;
  id mockView = [OCMockObject niceMockForClass:[UIView class]];
  [[[mockView stub] andReturn:[[UIApplication sharedApplication] keyWindow]]
      window];
  base::scoped_nsobject<HardwareKeyboardWatcher> watcher(
      [[HardwareKeyboardWatcher alloc] initWithAccessoryView:mockView]);

  PostKeyboardWillChangeNotification(CGRectMake(0, 0, 100, 100),
                                     CGRectMake(0, 100, 100, 100));

  histogram_tester.ExpectUniqueSample("Omnibox.HardwareKeyboardModeEnabled",
                                      false, 1);
}

TEST_F(HardwareKeyboardWatcherTest,
       KeyboardFullyOffScreen_SoftwareKeyboardHistogram) {
  base::HistogramTester histogram_tester;
  id mockView = [OCMockObject niceMockForClass:[UIView class]];
  [[[mockView stub] andReturn:[[UIApplication sharedApplication] keyWindow]]
      window];
  base::scoped_nsobject<HardwareKeyboardWatcher> watcher(
      [[HardwareKeyboardWatcher alloc] initWithAccessoryView:mockView]);

  PostKeyboardWillChangeNotification(CGRectMake(0, -100, 100, 100),
                                     CGRectMake(0, 0, 100, 100));

  histogram_tester.ExpectUniqueSample("Omnibox.HardwareKeyboardModeEnabled",
                                      false, 1);
}

TEST_F(HardwareKeyboardWatcherTest,
       KeyboardPartiallyOnScreen_SoftwareKeyboardHistogram) {
  base::HistogramTester histogram_tester;
  id mockView = [OCMockObject niceMockForClass:[UIView class]];
  [[[mockView stub] andReturn:[[UIApplication sharedApplication] keyWindow]]
      window];
  base::scoped_nsobject<HardwareKeyboardWatcher> watcher(
      [[HardwareKeyboardWatcher alloc] initWithAccessoryView:mockView]);

  PostKeyboardWillChangeNotification(CGRectMake(0, -50, 100, 100),
                                     CGRectMake(0, 0, 100, 100));

  histogram_tester.ExpectUniqueSample("Omnibox.HardwareKeyboardModeEnabled",
                                      true, 1);
}

}  // namespace
