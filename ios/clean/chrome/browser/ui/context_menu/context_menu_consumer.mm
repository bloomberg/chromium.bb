// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/context_menu/context_menu_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContextMenuItem ()
@property(nonatomic, readwrite) NSString* title;
@property(nonatomic, readwrite) NSInvocation* command;
@end

@implementation ContextMenuItem

@synthesize title = _title;
@synthesize command = _command;

+ (instancetype)itemWithTitle:(NSString*)title command:(NSInvocation*)command {
  ContextMenuItem* item = [[self alloc] init];
  item.title = title;
  item.command = command;
  return item;
}

@end
