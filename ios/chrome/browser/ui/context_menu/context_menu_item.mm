// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/context_menu/context_menu_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContextMenuItem

- (instancetype)initWithTitle:(NSString*)title action:(ProceduralBlock)action {
  self = [super init];
  if (self) {
    _title = [title copy];
    _action = [action copy];
  }
  return self;
}

@end
