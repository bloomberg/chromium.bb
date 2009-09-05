// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/cocoa_test_helper.h"

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

- (void)setPretendIsKeyWindow:(BOOL)flag {
  pretendIsKeyWindow_ = flag;
}

- (BOOL)isKeyWindow {
  return pretendIsKeyWindow_;
}

@end
