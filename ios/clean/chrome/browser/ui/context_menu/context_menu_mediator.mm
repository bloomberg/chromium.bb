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
  return [ContextMenuItem itemWithTitle:@"Execute Script"
                                command:@selector(executeContextMenuScript:)
                        commandOpensTab:NO];
}

+ (NSArray<ContextMenuItem*>*)linkItems {
  // TODO: Supply commands and update |commandOpensTab| accordingly once
  // incognito and link copying are implemented.
  return @[
    [ContextMenuItem itemWithTitle:@"Open In New Tab"
                           command:@selector(openContextMenuLinkInNewTab:)
                   commandOpensTab:YES],
    [ContextMenuItem itemWithTitle:@"Open In New Incognito Tab"
                           command:nil
                   commandOpensTab:NO],
    [ContextMenuItem itemWithTitle:@"Copy Link" command:nil commandOpensTab:NO],
  ];
}

+ (NSArray<ContextMenuItem*>*)imageItems {
  // TODO: Supply commands and update |commandOpensTab| accordingly once image
  // saving is implemented.
  return @[
    [ContextMenuItem itemWithTitle:@"Save Image"
                           command:nil
                   commandOpensTab:NO],
    [ContextMenuItem itemWithTitle:@"Open Image"
                           command:@selector(openContextMenuImage:)
                   commandOpensTab:NO],
    [ContextMenuItem itemWithTitle:@"Open Image In New Tab"
                           command:@selector(openContextMenuImageInNewTab:)
                   commandOpensTab:YES],
  ];
}

+ (ContextMenuItem*)cancelItem {
  return
      [ContextMenuItem itemWithTitle:@"Cancel" command:nil commandOpensTab:NO];
}

@end
