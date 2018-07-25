// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/accessory_provider.h"

#include "base/feature_list.h"
#import "base/mac/foundation_util.h"
#include "components/autofill/core/common/autofill_features.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/keyboard_accessory_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ManualFillAccessoryProvider ()

// The default accesory view to return in the update block.
@property(nonatomic, readonly) ManualFillKeyboardAccessoryView* accessoryView;

// Callback to update the accessory view.
@property(nonatomic, copy)
    AccessoryViewReadyCompletion accessoryViewUpdateBlock;

@end

@implementation ManualFillAccessoryProvider

@synthesize accessoryViewDelegate = _accessoryViewDelegate;
@synthesize accessoryView = _accessoryView;
@synthesize accessoryViewUpdateBlock = _accessoryViewUpdateBlock;

#pragma mark - FormInputAccessoryViewProvider

- (void)retrieveAccessoryViewForForm:(const web::FormActivityParams&)params
                            webState:(web::WebState*)webState
            accessoryViewUpdateBlock:
                (AccessoryViewReadyCompletion)accessoryViewUpdateBlock {
  DCHECK(accessoryViewUpdateBlock);
  BOOL isManualFillEnabled =
      base::FeatureList::IsEnabled(autofill::features::kAutofillManualFallback);
  if (!isManualFillEnabled) {
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

- (ManualFillKeyboardAccessoryView*)accessoryView {
  if (!_accessoryView) {
    _accessoryView =
        [[ManualFillKeyboardAccessoryView alloc] initWithDelegate:nil];
  }
  return _accessoryView;
}

@end
