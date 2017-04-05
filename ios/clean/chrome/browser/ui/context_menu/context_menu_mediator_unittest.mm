// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/context_menu/context_menu_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "base/memory/ptr_util.h"
#import "ios/clean/chrome/browser/ui/context_menu/context_menu_consumer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

@interface TestConsumer : NSObject<ContextMenuConsumer>
@property(nonatomic, copy) NSString* title;
@property(nonatomic, copy) NSArray<ContextMenuItem*>* items;
@end

@implementation TestConsumer
@synthesize title = _title;
@synthesize items = _items;

- (void)setContextMenuTitle:(NSString*)title {
  self.title = title;
}

- (void)setContextMenuItems:(NSArray<ContextMenuItem*>*)items {
  self.items = items;
}

@end

TEST(ContextMenuMediatorTest, TestTitleAndItems) {
  // A minimal test -- that once initialized, the mediator has set a non-empty
  // title on the consumer, and passed a non-empty list of items with non-empty
  // titles.
  TestConsumer* consumer = [[TestConsumer alloc] init];
  ContextMenuMediator* mediator =
      [[ContextMenuMediator alloc] initWithConsumer:consumer];

  EXPECT_NE(nil, mediator);

  EXPECT_NE(0U, consumer.title.length);
  EXPECT_NE(0U, consumer.items.count);

  for (ContextMenuItem* item in consumer.items) {
    EXPECT_NE(0U, item.title.length);
  }
}
