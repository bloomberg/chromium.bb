// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WK_WEB_VIEW_CRASH_DETECTOR_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WK_WEB_VIEW_CRASH_DETECTOR_H_

#import <WebKit/WebKit.h>

#import "base/ios/block_types.h"

// A watchdog which detects and reports WKWebView crashes.
@interface CRWWKWebViewCrashDetector : NSObject

// Designated initializer. |handler| will be called if |webView| has crashed.
- (instancetype)initWithWebView:(WKWebView*)webView
                   crashHandler:(ProceduralBlock)handler;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WK_WEB_VIEW_CRASH_DETECTOR_H_
