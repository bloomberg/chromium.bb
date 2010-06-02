// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_SIDE_TABS_TOOLBAR_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_SIDE_TABS_TOOLBAR_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "chrome/browser/cocoa/toolbar_controller.h"

// A toolbar controller for when there are side tabs.
// TODO(pinkerton): needs to be fleshed out much more and refactored. It
// probably should not be a subclass, but the ToolbarController factored into
// a base class. This, however, works for now.

@interface SideTabsToolbarController : ToolbarController {
 @private
  IBOutlet NSButton* starButton_;
  IBOutlet NSTextField* title_;
  IBOutlet NSProgressIndicator* loadingSpinner_;
}

@end

#endif  // CHROME_BROWSER_COCOA_SIDE_TABS_TOOLBAR_CONTROLLER_H_
