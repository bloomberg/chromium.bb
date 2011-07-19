// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_contents/sad_tab_controller.h"

#include "base/mac/mac_util.h"
#import "chrome/browser/ui/cocoa/tab_contents/sad_tab_view.h"

@implementation SadTabController

- (id)initWithTabContents:(TabContents*)someTabContents
                superview:(NSView*)superview {
  if ((self = [super initWithNibName:@"SadTab"
                              bundle:base::mac::MainAppBundle()])) {
    tabContents_ = someTabContents;

    NSView* view = [self view];
    [superview addSubview:view];
    [view setFrame:[superview bounds]];
  }

  return self;
}

- (void)awakeFromNib {
  // If tab_contents_ is nil, ask view to remove link.
  if (!tabContents_) {
    SadTabView* sad_view = static_cast<SadTabView*>([self view]);
    [sad_view removeLinkButton];
  }
}

- (void)dealloc {
  [[self view] removeFromSuperview];
  [super dealloc];
}

- (TabContents*)tabContents {
  return tabContents_;
}

- (void)openLearnMoreAboutCrashLink:(id)sender {
  // Send the action up through the responder chain.
  [NSApp sendAction:@selector(openLearnMoreAboutCrashLink:) to:nil from:self];
}

@end
