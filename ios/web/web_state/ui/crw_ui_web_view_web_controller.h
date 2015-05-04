// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_UI_WEB_VIEW_WEB_CONTROLLER_H_
#define IOS_WEB_WEB_STATE_UI_CRW_UI_WEB_VIEW_WEB_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/web/web_state/crw_recurring_task_delegate.h"
#import "ios/web/web_state/ui/crw_web_controller.h"

namespace web {

// Continuous JavaScript check timer frequency constants (exposed for tests).
extern const int64 kContinuousCheckIntervalMSHigh;
extern const int64 kContinuousCheckIntervalMSLow;

}  // namespace web

@class CRWJSInvokeParameterQueue;

// A concrete implementation of CRWWebController based on UIWebView.
@interface CRWUIWebViewWebController :
    CRWWebController<CRWRecurringTaskDelegate>

// Designated initializer.
- (instancetype)initWithWebState:(scoped_ptr<web::WebStateImpl>)webState;

@end

#pragma mark Testing

@interface CRWUIWebViewWebController (UsedOnlyForTesting)
// Queued message strings received from JavaScript and deferred for processing.
@property(nonatomic, readonly)
    CRWJSInvokeParameterQueue* jsInvokeParameterQueue;
// Used by tests to measure time taken to inject JavaScript.
@property(nonatomic, readonly)
    id<CRWRecurringTaskDelegate> recurringTaskDelegate;
// Acts on the queue of messages received from the JS object encoded as
// JSON in plain text.
- (BOOL)respondToMessageQueue:(NSString*)messageQueue
            userIsInteracting:(BOOL)userIsInteracting
                    originURL:(const GURL&)originURL;
@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_UI_WEB_VIEW_WEB_CONTROLLER_H_
