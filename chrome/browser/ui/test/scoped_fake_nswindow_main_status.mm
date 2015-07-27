// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/test/scoped_fake_nswindow_main_status.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/foundation_util.h"

namespace {

NSWindow* g_fake_main_window = nil;

}

// Donates a testing implementation of [NSWindow isMainWindow].
@interface IsMainWindowDonorForWindow : NSObject
@end

@implementation IsMainWindowDonorForWindow
- (BOOL)isMainWindow {
  NSWindow* selfAsWindow = base::mac::ObjCCastStrict<NSWindow>(self);
  return selfAsWindow == g_fake_main_window;
}
@end

ScopedFakeNSWindowMainStatus::ScopedFakeNSWindowMainStatus(NSWindow* window)
    : swizzler_([NSWindow class],
                [IsMainWindowDonorForWindow class],
                @selector(isMainWindow)) {
  DCHECK(!g_fake_main_window);
  g_fake_main_window = window;
}

ScopedFakeNSWindowMainStatus::~ScopedFakeNSWindowMainStatus() {
  g_fake_main_window = nil;
}
