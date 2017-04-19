// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tools/tools_mediator.h"

#import "ios/clean/chrome/browser/ui/actions/settings_actions.h"
#import "ios/clean/chrome/browser/ui/tools/tools_actions.h"
#import "ios/clean/chrome/browser/ui/tools/tools_consumer.h"
#import "ios/clean/chrome/browser/ui/tools/tools_menu_item.h"
#import "ios/shared/chrome/browser/ui/tools_menu/tools_menu_configuration.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolsMediator ()
@property(nonatomic, strong) id<ToolsConsumer> consumer;
@property(nonatomic, strong) ToolsMenuConfiguration* toolsMenuConfiguration;
@end

@implementation ToolsMediator

@synthesize consumer = _consumer;
@synthesize toolsMenuConfiguration = _toolsMenuConfiguration;

- (instancetype)initWithConsumer:(id<ToolsConsumer>)consumer
                andConfiguration:(ToolsMenuConfiguration*)menuConfiguration {
  self = [super init];
  if (self) {
    self.toolsMenuConfiguration = menuConfiguration;
    self.consumer = consumer;
  }
  return self;
}

- (void)setConsumer:(id<ToolsConsumer>)consumer {
  _consumer = consumer;

  // PLACEHOLDER: Temporary hardcoded consumer model.
  NSArray<ToolsMenuItem*>* menuItems = [[NSArray alloc] init];
  menuItems = @[
    [[ToolsMenuItem alloc] init], [[ToolsMenuItem alloc] init],
    [[ToolsMenuItem alloc] init], [[ToolsMenuItem alloc] init],
    [[ToolsMenuItem alloc] init], [[ToolsMenuItem alloc] init],
    [[ToolsMenuItem alloc] init], [[ToolsMenuItem alloc] init],
    [[ToolsMenuItem alloc] init], [[ToolsMenuItem alloc] init],
    [[ToolsMenuItem alloc] init]
  ];

  menuItems[0].title = @"New Tab";

  menuItems[1].title = @"New Incognito Tab";

  menuItems[2].title = @"Bookmarks";

  menuItems[3].title = @"Reading List";

  menuItems[4].title = @"Recent Tabs";

  menuItems[5].title = @"History";

  menuItems[6].title = @"Report an Issue";

  menuItems[7].title = @"Find in Page…";
  menuItems[7].action = @selector(showFindInPage);

  menuItems[8].title = @"Request Desktop Site";

  menuItems[9].title = @"Settings";
  menuItems[9].action = @selector(showSettings:);

  menuItems[10].title = @"Help";

  [_consumer setToolsMenuItems:menuItems];
  [_consumer
      setDisplayOverflowControls:!self.toolsMenuConfiguration.isInTabSwitcher];
}

@end
