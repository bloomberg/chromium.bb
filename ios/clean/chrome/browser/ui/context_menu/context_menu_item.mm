// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/context_menu/context_menu_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContextMenuItem () {
  // Backing object for |commands|.
  std::vector<SEL> _commands;
}

// Convenience initializer for use by the factory method.
- (instancetype)initWithTitle:(NSString*)title
                     commands:(const std::vector<SEL>&)commands;

@end

@implementation ContextMenuItem

@synthesize title = _title;

- (instancetype)initWithTitle:(NSString*)title
                     commands:(const std::vector<SEL>&)commands {
  if ((self = [super init])) {
    _title = [title copy];
    _commands = commands;
  }
  return self;
}

#pragma mark - Accessors

- (const std::vector<SEL>&)commands {
  return _commands;
}

#pragma mark - Public

+ (instancetype)itemWithTitle:(NSString*)title
                     commands:(const std::vector<SEL>&)commands {
  return [[ContextMenuItem alloc] initWithTitle:title commands:commands];
}

@end
