// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_WEB_CONTENTS_WEB_CONTENTS_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_WEB_CONTENTS_WEB_CONTENTS_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol WebContentsConsumer;
class WebStateList;

// A mediator object that provides the relevant properties of a web state
// to a consumer.
@interface WebContentsMediator : NSObject

// Updates to this webStateList are mediated to the consumer. This can change
// during the lifetime of this object and may be nil.
@property(nonatomic, assign) WebStateList* webStateList;

// The consumer for this object. This can change during the lifetime of this
// object and may be nil.
@property(nonatomic, weak) id<WebContentsConsumer> consumer;

// Stops observing all objects and sets the active webState's webUsageEnabled
// to false.
- (void)disconnect;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_WEB_CONTENTS_WEB_CONTENTS_MEDIATOR_H_
