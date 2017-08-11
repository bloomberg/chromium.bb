// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/app/steps/simple_application_step.h"

@implementation SimpleApplicationStep

@synthesize providedFeature = _providedFeature;
@synthesize providedFeatures = _providedFeatures;
@synthesize requiredFeatures = _requiredFeatures;
@synthesize synchronous = _synchronous;

- (instancetype)init {
  if ((self = [super init])) {
    _synchronous = YES;
    _providedFeatures = @[];
    _requiredFeatures = @[];
  }
  return self;
}

- (void)setProvidedFeature:(NSString*)providedFeature {
  _providedFeature = providedFeature;
  self.providedFeatures = @[ self.providedFeature ];
}

@end
