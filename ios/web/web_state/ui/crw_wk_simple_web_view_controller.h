// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WK_SIMPLE_WEB_VIEW_CONTROLLER_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WK_SIMPLE_WEB_VIEW_CONTROLLER_H_

#import <WebKit/WebKit.h>

#import "ios/web/web_state/ui/crw_simple_web_view_controller.h"

// A CRWSimpleWebViewController implentation backed by a WKWebView.
@interface CRWWKSimpleWebViewController : NSObject<CRWSimpleWebViewController>

// Creates a new view controller backed by a WKWebView.
- (instancetype)initWithWKWebView:(WKWebView*)webView;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WK_SIMPLE_WEB_VIEW_CONTROLLER_H_
