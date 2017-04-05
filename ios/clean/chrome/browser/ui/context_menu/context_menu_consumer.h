// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_CONSUMER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_CONSUMER_H_

#import <Foundation/Foundation.h>

// Object encapsulating configuration information for an item in a context menu.
@interface ContextMenuItem : NSObject

// Create a new item with |title| and |command|.
+ (instancetype)itemWithTitle:(NSString*)title command:(NSInvocation*)command;

// The title associated with the item. This is usually the text the user will
// see.
@property(nonatomic, readonly) NSString* title;

// The selector and parameters that will be called when the item is
// selected.
@property(nonatomic, readonly) NSInvocation* command;

@end

// A ContextMenuConsumer (typically a view controller) uses data provided by
// this protocol to display an run a context menu.
@protocol ContextMenuConsumer

// Informs the consumer that the overall title of the context menu will be
// |title|. This method should be called only once in the lifetime of the
// consumer.
- (void)setContextMenuTitle:(NSString*)title;

// Informs the consumer that the context menu should display the items defined
// in |items|. This method should be called only once in the lifetime of the
// consumer.
- (void)setContextMenuItems:(NSArray<ContextMenuItem*>*)items;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_CONSUMER_H_
