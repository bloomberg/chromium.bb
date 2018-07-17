// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manualfill_accessory_provider.h"

#include "base/feature_list.h"
#import "base/mac/foundation_util.h"
#include "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/ui/autofill/manualfill/keyboard_accessory_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ManualfillAccessoryProvider ()

// The default accesory view to return in the update block.
@property(nonatomic, readonly) KeyboardAccessoryView* accessoryView;

// Callback to update the accessory view.
@property(nonatomic, copy)
    AccessoryViewReadyCompletion accessoryViewUpdateBlock;

@end

@implementation ManualfillAccessoryProvider

@synthesize accessoryViewDelegate = _accessoryViewDelegate;
@synthesize accessoryView = _accessoryView;
@synthesize accessoryViewUpdateBlock = _accessoryViewUpdateBlock;

#pragma mark - FormInputAccessoryViewProvider

- (void)retrieveAccessoryViewForForm:(const web::FormActivityParams&)params
                            webState:(web::WebState*)webState
            accessoryViewUpdateBlock:
                (AccessoryViewReadyCompletion)accessoryViewUpdateBlock {
  DCHECK(accessoryViewUpdateBlock);
  BOOL isManualfillEnabled =
      base::FeatureList::IsEnabled(autofill::features::kAutofillManualFallback);
  if (!isManualfillEnabled) {
    accessoryViewUpdateBlock(nil, self);
    return;
  }
  accessoryViewUpdateBlock(self.accessoryView, self);
  self.accessoryViewUpdateBlock = accessoryViewUpdateBlock;
}

- (void)inputAccessoryViewControllerDidReset:
    (FormInputAccessoryViewController*)controller {
  self.accessoryViewUpdateBlock = nil;
}

- (void)resizeAccessoryView {
  DCHECK(_accessoryViewUpdateBlock);
  self.accessoryViewUpdateBlock(self.accessoryView, self);
}

#pragma mark - Getters

- (KeyboardAccessoryView*)accessoryView {
  if (!_accessoryView) {
    _accessoryView = [[KeyboardAccessoryView alloc] initWithDelegate:nil];
  }
  return _accessoryView;
}

@end
