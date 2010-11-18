// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "extension_installed_bubble_bridge.h"

#import "chrome/browser/cocoa/extension_installed_bubble_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension.h"

void ExtensionInstalledBubbleCocoa::ShowExtensionInstalledBubble(
    gfx::NativeWindow window,
    const Extension* extension,
    Browser* browser,
    SkBitmap icon) {
  // The controller is deallocated when the window is closed, so no need to
  // worry about it here.
  [[ExtensionInstalledBubbleController alloc]
      initWithParentWindow:window
                 extension:extension
                   browser:browser
                      icon:icon];
}
