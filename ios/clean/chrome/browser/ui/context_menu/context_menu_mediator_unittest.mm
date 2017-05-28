// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/context_menu/context_menu_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "base/memory/ptr_util.h"
#import "ios/clean/chrome/browser/ui/context_menu/context_menu_consumer.h"
#import "ios/clean/chrome/browser/ui/context_menu/context_menu_context_impl.h"
#import "ios/web/public/web_state/context_menu_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

@interface TestConsumer : NSObject<ContextMenuConsumer>
@property(nonatomic, strong) ContextMenuContext* context;
@property(nonatomic, copy) NSString* title;
@property(nonatomic, copy) NSArray<ContextMenuItem*>* items;
@property(nonatomic, strong) ContextMenuItem* cancelItem;
@end

@implementation TestConsumer
@synthesize context = _context;
@synthesize title = _title;
@synthesize items = _items;
@synthesize cancelItem = _cancelItem;

- (void)setContextMenuContext:(ContextMenuContext*)context {
  self.context = context;
}

- (void)setContextMenuTitle:(NSString*)title {
  self.title = title;
}

- (void)setContextMenuItems:(NSArray<ContextMenuItem*>*)items
                 cancelItem:(ContextMenuItem*)cancelItem {
  self.items = items;
  self.cancelItem = cancelItem;
}

@end

// Tests that the menu title is set properly.
TEST(ContextMenuMediatorTest, MenuTitle) {
  // Create a context with |kMenuTitle|.
  NSString* kMenuTitle = @"Menu Title";
  web::ContextMenuParams params;
  params.menu_title.reset(kMenuTitle);
  ContextMenuContextImpl* context =
      [[ContextMenuContextImpl alloc] initWithParams:params];
  // Update the consumer and verify that |kMenuTitle| is set.
  TestConsumer* consumer = [[TestConsumer alloc] init];
  [ContextMenuMediator updateConsumer:consumer withContext:context];
  EXPECT_NSEQ(kMenuTitle, consumer.title);
}

// Tests that the script option is shown for javacript: scheme URLs.
TEST(ContextMenuMediatorTest, ScriptItem) {
  // Create a context with |kJavaScriptURL|.
  GURL kJavaScriptURL("javascript:alert('test');");
  web::ContextMenuParams params;
  params.link_url = kJavaScriptURL;
  ContextMenuContextImpl* context =
      [[ContextMenuContextImpl alloc] initWithParams:params];
  // Update the consumer and verify that the script option was added.
  TestConsumer* consumer = [[TestConsumer alloc] init];
  [ContextMenuMediator updateConsumer:consumer withContext:context];
  ContextMenuItem* script_item = [consumer.items firstObject];
  EXPECT_NSEQ(script_item.title, @"Execute Script");
}

// Tests that the link options are shown for valid link URLs.
TEST(ContextMenuMediatorTest, LinkItems) {
  // Create a context with a valid link URL.
  web::ContextMenuParams params;
  params.link_url = GURL("http://valid.url.com");
  ;
  ContextMenuContextImpl* context =
      [[ContextMenuContextImpl alloc] initWithParams:params];
  // Update the consumer and verify that the link options were added.
  TestConsumer* consumer = [[TestConsumer alloc] init];
  [ContextMenuMediator updateConsumer:consumer withContext:context];
  NSArray* link_option_titles = @[
    @"Open In New Tab",
    @"Open In New Incognito Tab",
    @"Copy Link",
  ];
  ASSERT_EQ(link_option_titles.count, consumer.items.count);
  for (NSUInteger i = 0; i < link_option_titles.count; ++i) {
    EXPECT_NSEQ(link_option_titles[i], consumer.items[i].title);
  }
}

// Tests that the image options are shown for valid image URLs.
TEST(ContextMenuMediatorTest, ImageItems) {
  // Create a context with a valid image URL.
  web::ContextMenuParams params;
  params.src_url = GURL("http://valid.url.com");
  ;
  ContextMenuContextImpl* context =
      [[ContextMenuContextImpl alloc] initWithParams:params];
  // Update the consumer and verify that the image options were added.
  TestConsumer* consumer = [[TestConsumer alloc] init];
  [ContextMenuMediator updateConsumer:consumer withContext:context];
  NSArray* image_option_titles = @[
    @"Save Image",
    @"Open Image",
    @"Open Image In New Tab",
  ];
  ASSERT_EQ(image_option_titles.count, consumer.items.count);
  for (NSUInteger i = 0; i < image_option_titles.count; ++i) {
    EXPECT_NSEQ(image_option_titles[i], consumer.items[i].title);
  }
}

// Tests that the cancel item is always added.
TEST(ContextMenuMediatorTest, CancelItem) {
  // Create an empty context; the cancel item should be added regardless.
  web::ContextMenuParams params;
  ContextMenuContextImpl* context =
      [[ContextMenuContextImpl alloc] initWithParams:params];
  // Update the consumer and verify that the image options were added.
  TestConsumer* consumer = [[TestConsumer alloc] init];
  [ContextMenuMediator updateConsumer:consumer withContext:context];
  EXPECT_NSEQ(@"Cancel", consumer.cancelItem.title);
}
