// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/browser/window.h"

#import <Cocoa/Cocoa.h>

// An NSWindow is using RemoteMacViews if it has no BridgedNativeWidgetImpl.
// This is the most expedient method of determining if an NSWindow uses
// RemoteMacViews.
namespace remote_cocoa {
class BridgedNativeWidgetImpl;
}  // namespace remote_cocoa

@interface NSWindow (Private)
- (remote_cocoa::BridgedNativeWidgetImpl*)bridgeImpl;
@end

namespace remote_cocoa {

bool IsWindowRemote(gfx::NativeWindow gfx_window) {
  NSWindow* ns_window = gfx_window.GetNativeNSWindow();
  if ([ns_window respondsToSelector:@selector(bridgeImpl)]) {
    if (![ns_window bridgeImpl])
      return true;
  }
  return false;
}

NSWindow* CreateInProcessTransparentClone(gfx::NativeWindow remote_window) {
  DCHECK(IsWindowRemote(remote_window));
  NSWindow* window = [[NSWindow alloc]
      initWithContentRect:[remote_window.GetNativeNSWindow() frame]
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO];
  [window setAlphaValue:0];
  [window makeKeyAndOrderFront:nil];
  [window setLevel:NSModalPanelWindowLevel];
  return window;
}

}  // namespace remote_cocoa
