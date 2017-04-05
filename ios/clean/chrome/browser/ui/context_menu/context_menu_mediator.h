// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol ContextMenuConsumer;

// A mediator object that provides configuration information for a context
// menu.
@interface ContextMenuMediator : NSObject

// Creates a new mediator with the non-nil consumer |consumer|.
- (instancetype)initWithConsumer:(id<ContextMenuConsumer>)consumer;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_MEDIATOR_H_
