// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol ToolsConsumer;
@class ToolsMenuConfiguration;

// A mediator object that sets a ToolsMenuVC appeareance based on various data
// sources.
@interface ToolsMediator : NSObject
- (instancetype)initWithConsumer:(id<ToolsConsumer>)consumer
                andConfiguration:(ToolsMenuConfiguration*)menuConfiguration
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_MEDIATOR_H_
