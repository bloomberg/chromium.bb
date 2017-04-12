// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_FIND_IN_PAGE_FIND_IN_PAGE_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_FIND_IN_PAGE_FIND_IN_PAGE_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol FindInPageConsumer;
@protocol FindInPageVisibilityCommands;
class WebStateList;

// FindInPageConsumerProvider allows FindInPageMediator to ask its provider to
// return a |consumer| object.
@protocol FindInPageConsumerProvider
// Returns the consumer that should be updated by the caller.  The returned
// |consumer| can be nil, in which case no updates are necessary.
@property(nonatomic, readonly, weak) id<FindInPageConsumer> consumer;
@end

// FindInPageMediator provides updates to a FindInPageConsumer based on observer
// callbacks from multiple models.
@interface FindInPageMediator : NSObject

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                            provider:(id<FindInPageConsumerProvider>)provider
                          dispatcher:
                              (id<FindInPageVisibilityCommands>)dispatcher
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Stops finding in page for the current active WebState.
- (void)stopFinding;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_FIND_IN_PAGE_FIND_IN_PAGE_MEDIATOR_H_
