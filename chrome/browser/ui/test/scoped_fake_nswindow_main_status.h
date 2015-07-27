// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TEST_SCOPED_FAKE_NSWINDOW_MAIN_STATUS_H_
#define CHROME_BROWSER_UI_TEST_SCOPED_FAKE_NSWINDOW_MAIN_STATUS_H_

#import "base/mac/scoped_objc_class_swizzler.h"

@class NSWindow;

// Simulates a particular NSWindow to report YES for [NSWindow isMainWindow].
// This allows test coverage of code relying on window focus changes without
// resorting to an interactive_ui_test.
class ScopedFakeNSWindowMainStatus {
 public:
  explicit ScopedFakeNSWindowMainStatus(NSWindow* window);
  ~ScopedFakeNSWindowMainStatus();

 private:
  base::mac::ScopedObjCClassSwizzler swizzler_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFakeNSWindowMainStatus);
};

#endif  // CHROME_BROWSER_UI_TEST_SCOPED_FAKE_NSWINDOW_MAIN_STATUS_H_
