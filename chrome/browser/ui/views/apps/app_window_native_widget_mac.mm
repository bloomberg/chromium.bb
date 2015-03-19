// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_window_native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/views/apps/native_widget_mac_frameless_nswindow.h"
#import "ui/base/cocoa/window_size_constants.h"

AppWindowNativeWidgetMac::AppWindowNativeWidgetMac(views::Widget* widget)
    : NativeWidgetMac(widget) {
}

AppWindowNativeWidgetMac::~AppWindowNativeWidgetMac() {
}

NSWindow* AppWindowNativeWidgetMac::CreateNSWindow(
    const views::Widget::InitParams& params) {
  // If the window has a standard frame, use the same NSWindow as
  // NativeWidgetMac.
  if (!params.remove_standard_frame)
    return NativeWidgetMac::CreateNSWindow(params);

  // NSTexturedBackgroundWindowMask is needed to implement draggable window
  // regions.
  NSUInteger style_mask = NSTexturedBackgroundWindowMask | NSTitledWindowMask |
                          NSClosableWindowMask | NSMiniaturizableWindowMask |
                          NSResizableWindowMask;
  return [[[NativeWidgetMacFramelessNSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:style_mask
                  backing:NSBackingStoreBuffered
                    defer:YES] autorelease];
}
