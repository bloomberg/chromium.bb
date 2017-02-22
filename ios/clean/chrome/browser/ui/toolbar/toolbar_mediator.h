// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol ToolbarConsumer;

namespace web {
class WebState;
}

// A mediator object that provides the relevant properties of a web state
// to a consumer.
@interface ToolbarMediator : NSObject

// The WebState whose properties this object mediates. This can change during
// the lifetime of this object and may be null.
@property(nonatomic, assign) web::WebState* webState;

// The consumer for this object. This can change during the lifetime of this
// object and may be nil.
@property(nonatomic, strong) id<ToolbarConsumer> consumer;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MEDIATOR_H_
