// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/context_menu/context_menu_mediator.h"

#import "ios/clean/chrome/browser/ui/commands/context_menu_commands.h"
#import "ios/clean/chrome/browser/ui/context_menu/context_menu_consumer.h"
#import "ios/clean/chrome/browser/ui/context_menu/context_menu_context_impl.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser.h"
#import "ios/web/public/url_scheme_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContextMenuMediator ()

// The ContextMenuItem to use for script menus.
+ (ContextMenuItem*)scriptItem;

// The ContextMenuItems to use for link menus.
+ (NSArray<ContextMenuItem*>*)linkItems;

// The ContextMenuItems to use for image menus.
+ (NSArray<ContextMenuItem*>*)imageItems;

// The cancel item to use.
+ (ContextMenuItem*)cancelItem;

@end

@implementation ContextMenuMediator

+ (void)updateConsumer:(id<ContextMenuConsumer>)consumer
           withContext:(ContextMenuContextImpl*)context {
  DCHECK(consumer);
  DCHECK(context);
  // Set the context and menu title.
  [consumer setContextMenuContext:context];
  [consumer setContextMenuTitle:context.menuTitle];
  // Add the appropriate items.
  NSMutableArray<ContextMenuItem*>* items = [[NSMutableArray alloc] init];
  if (context.script.size())
    [items addObject:[self scriptItem]];
  BOOL showLinkOptions =
      context.linkURL.is_valid() && web::UrlHasWebScheme(context.linkURL);
  if (showLinkOptions)
    [items addObjectsFromArray:[self linkItems]];
  BOOL showImageOptions = context.imageURL.is_valid();
  if (showImageOptions)
    [items addObjectsFromArray:[self imageItems]];
  [consumer setContextMenuItems:[items copy] cancelItem:[self cancelItem]];
}

#pragma mark -

+ (ContextMenuItem*)scriptItem {
  std::vector<SEL> commands(2U);
  commands[0] = @selector(executeContextMenuScript:);
  commands[1] = @selector(hideContextMenu:);
  return [ContextMenuItem itemWithTitle:@"Execute Script" commands:commands];
}

+ (NSArray<ContextMenuItem*>*)linkItems {
  // Opening the link in a new Tab will stop this context menu's coordinator, so
  // there's no need to hide it.
  std::vector<SEL> newTabCommands(1U);
  newTabCommands[0] = @selector(openContextMenuLinkInNewTab:);
  // TODO: Add |-openContextMenuLinkInNewIncognitoTab:| as the first command for
  // "Open In New Incognito Tab" once the incognito tab grid is implemented.
  std::vector<SEL> newIncognitoTabCommands(1U);
  newIncognitoTabCommands[0] = @selector(hideContextMenu:);
  // TODO: Add |-copyContextMenuLink:| as the first command for "Copy Link" once
  // copying to pasteboard is implemented.
  std::vector<SEL> copyLinkCommands(1U);
  newIncognitoTabCommands[0] = @selector(hideContextMenu:);
  return @[
    [ContextMenuItem itemWithTitle:@"Open In New Tab" commands:newTabCommands],
    [ContextMenuItem itemWithTitle:@"Open In New Incognito Tab"
                          commands:newIncognitoTabCommands],
    [ContextMenuItem itemWithTitle:@"Copy Link" commands:copyLinkCommands],
  ];
}

+ (NSArray<ContextMenuItem*>*)imageItems {
  // TODO: Add |-saveContextMenuImage:| as the first command for "Save Image"
  // once camera roll access has been implemented.
  std::vector<SEL> saveImageCommands(1U);
  saveImageCommands[0] = @selector(hideContextMenu:);
  std::vector<SEL> openImageCommands(2U);
  openImageCommands[0] = @selector(openContextMenuImage:);
  openImageCommands[1] = @selector(hideContextMenu:);
  // Opening the image in a new Tab will stop this context menu's coordinator,
  // so there's no need to hide it.
  std::vector<SEL> openImageInNewTabCommands(1U);
  openImageInNewTabCommands[0] = @selector(openContextMenuImageInNewTab:);
  return @[
    [ContextMenuItem itemWithTitle:@"Save Image" commands:saveImageCommands],
    [ContextMenuItem itemWithTitle:@"Open Image" commands:openImageCommands],
    [ContextMenuItem itemWithTitle:@"Open Image In New Tab"
                          commands:openImageInNewTabCommands],
  ];
}

+ (ContextMenuItem*)cancelItem {
  std::vector<SEL> cancelCommands(1U);
  cancelCommands[0] = @selector(hideContextMenu:);
  return [ContextMenuItem itemWithTitle:@"Cancel" commands:cancelCommands];
}

@end
