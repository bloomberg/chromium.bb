// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_address_view_controller_mac.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/autofill/autofill_address_model_mac.h"
#import "chrome/browser/autofill/autofill_dialog_controller_mac.h"
#include "chrome/browser/autofill/autofill_profile.h"
#import "third_party/GTM/Foundation/GTMNSObject+KeyValueObserving.h"

@interface AutoFillAddressViewController (PrivateMethods)
- (void)labelChanged:(GTMKeyValueChangeNotification*)notification;
- (void)addressesChanged:(GTMKeyValueChangeNotification*)notification;
- (void)defaultChanged:(GTMKeyValueChangeNotification*)notification;
@end

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
    [addressModel_ gtm_addObserver:self
                        forKeyPath:@"label"
                          selector:@selector(labelChanged:)
                          userInfo:nil
                           options:0];
    [parentController_ gtm_addObserver:self
                            forKeyPath:@"addressLabels"
                              selector:@selector(addressesChanged:)
                              userInfo:nil
                               options:0];
    [parentController_ gtm_addObserver:self
                            forKeyPath:@"defaultAddressLabel"
                              selector:@selector(defaultChanged:)
                              userInfo:nil
                               options:0];
  }
  return self;
}

- (void)dealloc {
  [addressModel_ gtm_removeObserver:self
                         forKeyPath:@"label"
                           selector:@selector(labelChanged:)];
  [parentController_ gtm_removeObserver:self
                             forKeyPath:@"addressLabels"
                               selector:@selector(addressesChanged:)];
  [parentController_ gtm_removeObserver:self
                             forKeyPath:@"defaultAddressLabel"
                               selector:@selector(defaultChanged:)];
  [addressModel_ release];
  [super dealloc];
}

// Override KVO method to notify parent controller when the address "label"
// changes.  Credit card UI updates accordingly.
- (void)labelChanged:(GTMKeyValueChangeNotification*)notification {
  [parentController_ notifyAddressChange:self];
}

- (void)addressesChanged:(GTMKeyValueChangeNotification*)notification {
  [self willChangeValueForKey:@"canAlterDefault"];
  [self didChangeValueForKey:@"canAlterDefault"];
}

- (void)defaultChanged:(GTMKeyValueChangeNotification*)notification {
  [self willChangeValueForKey:@"isDefault"];
  [self didChangeValueForKey:@"isDefault"];
}

- (IBAction)deleteAddress:(id)sender {
  [parentController_ deleteAddress:self];
}

- (void)copyModelToProfile:(AutoFillProfile*)profile {
  [addressModel_ copyModelToProfile:profile];
}

- (BOOL)canAlterDefault {
  return [[parentController_ addressLabels] count] > 1;
}

- (BOOL)isDefault {
  return
      [[addressModel_ label] isEqual:[parentController_ defaultAddressLabel]];
}

- (void)setIsDefault:(BOOL)def {
  [parentController_ setDefaultAddressLabel:def ? [addressModel_ label] : nil];
}

@end


