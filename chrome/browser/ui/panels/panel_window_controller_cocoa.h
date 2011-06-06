// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_WINDOW_CONTROLLER_COCOA_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_WINDOW_CONTROLLER_COCOA_H_

// A class acting as the Objective-C controller for the Panel window
// object. Handles interactions between Cocoa and the cross-platform
// code. Each window has a single titlebar and is managed/owned by Panel.

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#include "base/memory/scoped_ptr.h"

class PanelBrowserWindowCocoa;

@interface PanelWindowControllerCocoa : NSWindowController<NSWindowDelegate> {
 @private
  scoped_ptr<PanelBrowserWindowCocoa> windowShim_;
}

// Load the browser window nib and do any Cocoa-specific initialization.
- (id)initWithBrowserWindow:(PanelBrowserWindowCocoa*)window;

@end  // @interface PanelWindowController

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_WINDOW_CONTROLLER_COCOA_H_
