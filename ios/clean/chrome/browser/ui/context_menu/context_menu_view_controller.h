// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_VIEW_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/clean/chrome/browser/ui/context_menu/context_menu_consumer.h"

@protocol ContextMenuCommands;
@class ContextMenuContext;

// A view controller that displays a context menu.
@interface ContextMenuViewController : UIAlertController<ContextMenuConsumer>

// Designated initializer that uses |dispatcher| to communicate commands.
- (instancetype)initWithDispatcher:(id<ContextMenuCommands>)dispatcher;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_VIEW_CONTROLLER_H_
