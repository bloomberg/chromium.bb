// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_address_view_controller_mac.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/autofill/autofill_address_model_mac.h"
#import "chrome/browser/autofill/autofill_dialog_controller_mac.h"
#include "chrome/browser/autofill/autofill_profile.h"

@implementation AutoFillAddressViewController

@synthesize addressModel = addressModel_;

- (id)initWithProfile:(const AutoFillProfile&)profile
    disclosure:(NSCellStateValue)disclosureState
    controller:(AutoFillDialogController*) parentController {
  self = [super initWithNibName:@"AutoFillAddressFormView"
      bundle:mac_util::MainAppBundle()
      disclosure:disclosureState];
  if (self) {
    // Pull in the view for initialization.
    [self view];

    // Create the model.
    [self setAddressModel:[[[AutoFillAddressModel alloc]
        initWithProfile:profile] autorelease]];

    // We keep track of our parent controller for model-update purposes.
    parentController_ = parentController;

    // Register |self| as observer so we can notify parent controller.  See
    // |observeValueForKeyPath:| for details.
    [addressModel_ addObserver:self forKeyPath:@"label"
        options:(NSKeyValueObservingOptionNew | NSKeyValueObservingOptionOld)
        context:NULL];
  }
  return self;
}

- (void)dealloc {
  [addressModel_ removeObserver:self forKeyPath:@"label"];
  [addressModel_ release];
  [super dealloc];
}

// Override KVO method to notify parent controller when the address "label"
// changes.  Credit card UI updates accordingly.
- (void)observeValueForKeyPath:(NSString *)keyPath
    ofObject:(id)object
    change:(NSDictionary*)change
    context:(void *)context {
  if ([keyPath isEqual:@"label"]) {
    [parentController_ notifyAddressChange:self];
  }

  // Propagate KVO up to super.
  [super observeValueForKeyPath:keyPath
      ofObject:object change:change context:context];
}

- (IBAction)deleteAddress:(id)sender {
  [parentController_ deleteAddress:self];
}

- (void)copyModelToProfile:(AutoFillProfile*)profile {
  [addressModel_ copyModelToProfile:profile];
}

@end


