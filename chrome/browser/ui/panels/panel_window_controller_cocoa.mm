// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_window_controller_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "chrome/browser/ui/panels/panel_browser_window_cocoa.h"

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
  DCHECK_EQ(self, [[self window] delegate]);
  // TODO(dimich): Complete initialization of the NSWindow here.
}

@end
