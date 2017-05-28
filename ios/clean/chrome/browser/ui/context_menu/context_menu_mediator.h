// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol ContextMenuConsumer;
@class ContextMenuContextImpl;

// A mediator object that provides configuration information for a context
// menu.
@interface ContextMenuMediator : NSObject

// Populates |consumer| with alert items for actions appropriate for |context|.
+ (void)updateConsumer:(id<ContextMenuConsumer>)consumer
           withContext:(ContextMenuContextImpl*)context;

// A ContextMenuConsumer only requires configuration only once, then is
// immutable.  As a result, there is no need to instantiate an object to manage
// ongoing consumer updates; use |+updateConsumer:withContext:| instead.
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_MEDIATOR_H_
