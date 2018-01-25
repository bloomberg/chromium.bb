// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_MEDIATOR_H_

#import <UIKit/UIKit.h>

@protocol LocationBarConsumer;
class WebStateList;

// A mediator object that updates the mediator when the web state changes.
@interface LocationBarMediator : NSObject
// The WebStateList that this mediator listens for any changes on the active web
// state.
@property(nonatomic, assign) WebStateList* webStateList;

// The consumer for this object. This can change during the lifetime of this
// object and may be nil.
@property(nonatomic, strong) id<LocationBarConsumer> consumer;

// Stops observing all objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_MEDIATOR_H_
