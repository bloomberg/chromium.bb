// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/side_tabs_toolbar_controller.h"

#include "base/nsimage_cache_mac.h"
#import "chrome/browser/cocoa/autocomplete_text_field_cell.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"


namespace {

NSString* const kSearchButtonImageName = @"omnibox_search.pdf";

}

@implementation SideTabsToolbarController

- (id)initWithModel:(ToolbarModel*)model
           commands:(CommandUpdater*)commands
            profile:(Profile*)profile
            browser:(Browser*)browser
     resizeDelegate:(id<ViewResizer>)resizeDelegate {
  if ((self = [super initWithModel:model
                          commands:commands
                           profile:profile
                           browser:browser
                    resizeDelegate:resizeDelegate
                      nibFileNamed:@"SideToolbar"])) {
  }
  return self;
}

- (void)awakeFromNib {
  [super awakeFromNib];
  [goButton_ setImage:nsimage_cache::ImageNamed(kSearchButtonImageName)];
  [[locationBar_ autocompleteTextFieldCell] setStarIconView:nil];
}

- (void)showOptionalHomeButton {
  // Do nothing for side tabs.
}

- (void)setIsLoading:(BOOL)isLoading {
  if (isLoading)
    [loadingSpinner_ startAnimation:self];
  else
    [loadingSpinner_ stopAnimation:self];
}

@end
