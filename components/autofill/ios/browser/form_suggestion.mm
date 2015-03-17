// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/form_suggestion.h"

@interface FormSuggestion ()
// TODO(rohitrao): These properties must be redefined readwrite to work around a
// clang bug.  crbug.com/228650
@property(copy, readwrite) NSString* value;
@property(copy, readwrite) NSString* icon;

// Local initializer for a FormSuggestion.
- (id)initWithValue:(NSString*)value
    displayDescription:(NSString*)displayDescription
                  icon:(NSString*)icon
            identifier:(NSUInteger)identifier;

@end

@implementation FormSuggestion {
  NSString* _value;
  NSString* _displayDescription;
  NSString* _icon;
  NSUInteger _identifier;
  base::mac::ObjCPropertyReleaser _propertyReleaser_FormSuggestion;
}

@synthesize value = _value;
@synthesize displayDescription = _displayDescription;
@synthesize icon = _icon;
@synthesize identifier = _identifier;

- (id)initWithValue:(NSString*)value
    displayDescription:(NSString*)displayDescription
                  icon:(NSString*)icon
            identifier:(NSUInteger)identifier {
  self = [super init];
  if (self) {
    _propertyReleaser_FormSuggestion.Init(self, [FormSuggestion class]);
    _value = [value copy];
    _displayDescription = [displayDescription copy];
    _icon = [icon copy];
    _identifier = identifier;
  }
  return self;
}

+ (FormSuggestion*)suggestionWithValue:(NSString*)value
                    displayDescription:(NSString*)displayDescription
                                  icon:(NSString*)icon
                            identifier:(NSUInteger)identifier {
  return [[[FormSuggestion alloc] initWithValue:value
                             displayDescription:displayDescription
                                           icon:icon
                                     identifier:identifier] autorelease];
}

@end
