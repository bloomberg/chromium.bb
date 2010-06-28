// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_EXTENSIONS_EXTENSION_INFOBAR_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_EXTENSIONS_EXTENSION_INFOBAR_CONTROLLER_H_

#import "chrome/browser/cocoa/infobar_controller.h"

#import <Cocoa/Cocoa.h>

@interface ExtensionInfoBarController : InfoBarController {
  // The native extension view retrieved from the extension host. Weak.
  NSView* extensionView_;

  // The window containing this InfoBar. Weak.
  NSWindow* window_;
}

@end

#endif  // CHROME_BROWSER_COCOA_EXTENSIONS_EXTENSION_INFOBAR_CONTROLLER_H_
