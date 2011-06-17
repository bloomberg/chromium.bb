// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_window_controller_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_browser_window_cocoa.h"
#import "chrome/browser/ui/panels/panel_titlebar_view_cocoa.h"

@implementation PanelWindowControllerCocoa

- (id)initWithBrowserWindow:(PanelBrowserWindowCocoa*)window {
  NSString* nibpath =
      [base::mac::MainAppBundle() pathForResource:@"Panel" ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self]))
    windowShim_.reset(window);
  return self;
}

- (void)awakeFromNib {
  DCHECK([self window]);
  DCHECK(titlebar_view_);
  DCHECK_EQ(self, [[self window] delegate]);

  // Using NSModalPanelWindowLevel (8) rather then NSStatusWindowLevel (25)
  // ensures notification balloons on top of regular windows, but below
  // popup menus which are at NSPopUpMenuWindowLevel (101) and Spotlight
  // drop-out, which is at NSStatusWindowLevel-2 (23) for OSX 10.6/7.
  // See http://crbug.com/59878.
  [[self window] setLevel:NSModalPanelWindowLevel];

  [titlebar_view_ attach];
}

- (void)windowWillClose:(NSNotification*)notification {
  [self autorelease];
}

- (PanelTitlebarViewCocoa*)titlebarView {
  return titlebar_view_;
}

- (void)closePanel {
  windowShim_->panel()->Close();
}

@end
