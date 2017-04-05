// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/context_menu/context_menu_mediator.h"

#import "ios/clean/chrome/browser/ui/context_menu/context_menu_consumer.h"
#import "ios/web/public/web_state/context_menu_params.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContextMenuMediator ()
@property(nonatomic, weak) id<ContextMenuConsumer> consumer;
@end

@implementation ContextMenuMediator
@synthesize consumer = _consumer;

- (instancetype)initWithConsumer:(id<ContextMenuConsumer>)consumer {
  if ((self = [super init])) {
    _consumer = consumer;
    [self updateConsumer];
  }
  return self;
}

// Update the consumer.
- (void)updateConsumer {
  // PLACEHOLDER. Fake title.
  [self.consumer setContextMenuTitle:@"http://some/link.html"];
  NSMutableArray<ContextMenuItem*>* items =
      [[NSMutableArray<ContextMenuItem*> alloc] init];

  // PLACEHOLDER. Two non-functional items.
  [items
      addObject:[ContextMenuItem itemWithTitle:@"Open in New Tab" command:nil]];
  [items
      addObject:[ContextMenuItem itemWithTitle:@"Copy Link URL" command:nil]];
  [self.consumer setContextMenuItems:[items copy]];
}

@end
