// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WK_WEB_VIEW_WEB_CONTROLLER_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WK_WEB_VIEW_WEB_CONTROLLER_H_

#import "ios/web/web_state/ui/crw_web_controller.h"

// A concrete implementation of CRWWebController based on WKWebView.
@interface CRWWKWebViewWebController : CRWWebController

// Designated initializer.
- (instancetype)initWithWebState:(scoped_ptr<web::WebStateImpl>)webState;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WK_WEB_VIEW_WEB_CONTROLLER_H_
