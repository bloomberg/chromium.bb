// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcast_observer.h"

@protocol ToolbarConsumer;

namespace web {
class WebState;
}
class WebStateList;

// A mediator object that provides the relevant properties of a web state
// to a consumer.
@interface ToolbarMediator : NSObject<ChromeBroadcastObserver>

// The WebState whose properties this object mediates. This can change during
// the lifetime of this object and may be null.
@property(nonatomic, assign) web::WebState* webState;

// The WebStateList that this mediator listens for any changes on the total
// number of Webstates.
@property(nonatomic, assign) WebStateList* webStateList;

// The consumer for this object. This can change during the lifetime of this
// object and may be nil.
@property(nonatomic, strong) id<ToolbarConsumer> consumer;

// Stops observing all objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_MEDIATOR_H_
