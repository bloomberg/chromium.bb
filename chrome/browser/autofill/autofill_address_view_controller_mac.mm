// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_address_view_controller_mac.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/autofill/autofill_address_model_mac.h"
#include "chrome/browser/autofill/autofill_profile.h"

@implementation AutoFillAddressViewController

@synthesize addressModel = addressModel_;

- (id)initWithProfile:(const AutoFillProfile&)profile {
  self = [super initWithNibName:@"AutoFillAddressFormView"
                         bundle:mac_util::MainAppBundle()];
  if (self) {
    // Pull in the view for initialization.
    [self view];

    // Create the model.
    [self setAddressModel:[[[AutoFillAddressModel alloc]
        initWithProfile:profile] autorelease]];
  }
  return self;
}

- (void)dealloc {
  [addressModel_ release];
  [super dealloc];
}

- (void)copyModelToProfile:(AutoFillProfile*)profile {
  [addressModel_ copyModelToProfile:profile];
}

@end


