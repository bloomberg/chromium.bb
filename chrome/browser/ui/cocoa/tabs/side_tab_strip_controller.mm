// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/side_tab_strip_controller.h"

@implementation SideTabStripController

// TODO(pinkerton): Still need to figure out several things:
//   - new tab button placement and layout
//   - animating tabs in and out
//   - being able to drop a tab elsewhere besides the 1st position
//   - how to load a different tab view nib for each tab.

- (id)initWithView:(TabStripView*)view
        switchView:(NSView*)switchView
           browser:(Browser*)browser
          delegate:(id<TabStripControllerDelegate>)delegate {
  self = [super initWithView:view
                  switchView:switchView
                     browser:browser
                    delegate:delegate];
  if (self) {
    // Side tabs have no indent since they are not sharing space with the
    // window controls.
    [self setIndentForControls:0.0];
    verticalLayout_ = YES;
  }
  return self;
}

@end

