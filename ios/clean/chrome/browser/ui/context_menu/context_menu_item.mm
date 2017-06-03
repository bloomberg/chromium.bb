// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/context_menu/context_menu_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContextMenuItem ()

// Convenience initializer for use by the factory method.
- (instancetype)initWithTitle:(NSString*)title
                      command:(SEL)command
              commandOpensTab:(BOOL)commandOpensTab NS_DESIGNATED_INITIALIZER;

@end

@implementation ContextMenuItem

@synthesize title = _title;
@synthesize command = _command;
@synthesize commandOpensTab = _commandOpensTab;

- (instancetype)initWithTitle:(NSString*)title
                      command:(SEL)command
              commandOpensTab:(BOOL)commandOpensTab {
  if ((self = [super init])) {
    _title = [title copy];
    _command = command;
    _commandOpensTab = commandOpensTab;
  }
  return self;
}

#pragma mark - Public

+ (instancetype)itemWithTitle:(NSString*)title
                      command:(SEL)command
              commandOpensTab:(BOOL)commandOpensTab {
  return [[ContextMenuItem alloc] initWithTitle:title
                                        command:command
                                commandOpensTab:commandOpensTab];
}

@end
