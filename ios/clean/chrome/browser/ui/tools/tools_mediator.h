// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol ToolsConsumer;
@class ToolsMenuConfiguration;
namespace web {
class WebState;
}

// A mediator object that sets a ToolsMenuVC appearance based on various data
// sources.
@interface ToolsMediator : NSObject
- (nullable instancetype)initWithConsumer:(nonnull id<ToolsConsumer>)consumer
                            configuration:(nonnull ToolsMenuConfiguration*)
                                              menuConfiguration
    NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)init NS_UNAVAILABLE;

// The WebState whose properties this object mediates. This can change during
// the lifetime of this object and may be null.
@property(nonatomic, assign, nullable) web::WebState* webState;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_MEDIATOR_H_
