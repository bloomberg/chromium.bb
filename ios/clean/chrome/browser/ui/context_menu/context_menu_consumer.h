// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_CONSUMER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_CONSUMER_H_

#import "ios/clean/chrome/browser/ui/context_menu/context_menu_item.h"

@class ContextMenuContext;

// A ContextMenuConsumer (typically a view controller) uses data provided by
// this protocol to display an run a context menu.
@protocol ContextMenuConsumer

// Sets the context that should be sent with the ContextMenuCommands dispatched
// by ContextMenuItems added to this consumer.
- (void)setContextMenuContext:(ContextMenuContext*)context;

// Informs the consumer that the overall title of the context menu will be
// |title|. This method should be called only once in the lifetime of the
// consumer.
- (void)setContextMenuTitle:(NSString*)title;

// Informs the consumer that the context menu should display the items defined
// in |items|.  |cancelItem| will be added as the last option in the menu. This
// method should be called only once in the lifetime of the consumer.
- (void)setContextMenuItems:(NSArray<ContextMenuItem*>*)items
                 cancelItem:(ContextMenuItem*)cancelItem;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_CONSUMER_H_
