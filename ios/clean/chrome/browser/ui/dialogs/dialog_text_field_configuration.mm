// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/dialog_text_field_configuration.h"

#include "base/logging.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_configuration_identifier.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface DialogTextFieldConfiguration ()

// Initializer used by the factory method.
- (instancetype)initWithDefaultText:(NSString*)defaultText
                    placeholderText:(NSString*)placeholderText
                             secure:(BOOL)secure NS_DESIGNATED_INITIALIZER;

@end

@implementation DialogTextFieldConfiguration
@synthesize defaultText = _defaultText;
@synthesize placeholderText = _placeholderText;
@synthesize secure = _secure;
@synthesize identifier = _identifier;

- (instancetype)initWithDefaultText:(NSString*)defaultText
                    placeholderText:(NSString*)placeholderText
                             secure:(BOOL)secure {
  if ((self = [super init])) {
    _defaultText = [defaultText copy];
    _placeholderText = [placeholderText copy];
    _secure = secure;
    _identifier = [[DialogConfigurationIdentifier alloc] init];
  }
  return self;
}

#pragma mark - Public

+ (instancetype)configWithDefaultText:(NSString*)defaultText
                      placeholderText:(NSString*)placeholderText
                               secure:(BOOL)secure {
  return
      [[DialogTextFieldConfiguration alloc] initWithDefaultText:defaultText
                                                placeholderText:placeholderText
                                                         secure:secure];
}

@end
