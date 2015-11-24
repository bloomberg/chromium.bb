// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/extensions/extension_installed_bubble_controller.h"

void extensions::BundleInstaller::ShowInstalledBubble(
    const extensions::BundleInstaller* bundle,
    Browser* browser) {
  // The controller is deallocated when the window is closed, so no need to
  // worry about it here.
  [[ExtensionInstalledBubbleController alloc]
      initWithParentWindow:browser->window()->GetNativeWindow()
                    bundle:bundle
                   browser:browser];
}
