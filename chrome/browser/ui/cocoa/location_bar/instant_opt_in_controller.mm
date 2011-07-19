// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/instant_opt_in_controller.h"

#include "base/mac/mac_util.h"

@implementation InstantOptInController

- (id)initWithDelegate:(InstantOptInControllerDelegate*)delegate {
  if ((self = [super initWithNibName:@"InstantOptIn"
                     bundle:base::mac::MainAppBundle()])) {
    delegate_ = delegate;
  }
  return self;
}

- (void)awakeFromNib {
  // TODO(rohitrao): Translate and resize strings.
}

- (IBAction)ok:(id)sender {
  delegate_->UserPressedOptIn(true);
}

- (IBAction)cancel:(id)sender {
  delegate_->UserPressedOptIn(false);
}

@end
