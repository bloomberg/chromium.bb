// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_TEST_EVENT_UTILS_H_
#define CHROME_BROWSER_COCOA_TEST_EVENT_UTILS_H_

#import <objc/objc-class.h>

#include "base/logging.h"

// Within a given scope, replace the selector |selector| on |target| with that
// from |source|.
class ScopedClassSwizzler {
 public:
  ScopedClassSwizzler(Class target, Class source, SEL selector);
  ~ScopedClassSwizzler();

 private:
  Method old_selector_impl_;
  Method new_selector_impl_;

  DISALLOW_COPY_AND_ASSIGN(ScopedClassSwizzler);
};

namespace test_event_utils {

// Create synthetic mouse events for testing. Currently these are very basic,
// flesh out as needed.
NSEvent* MakeMouseEvent(NSEventType type, NSUInteger modifiers);
NSEvent* LeftMouseDownAtPoint(NSPoint point);

}  // namespace test_event_utils

#endif  // CHROME_BROWSER_COCOA_TEST_EVENT_UTILS_H_

