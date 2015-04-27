// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_JS_CRW_JS_WINDOW_ID_MANAGER_H_
#define IOS_WEB_WEB_STATE_JS_CRW_JS_WINDOW_ID_MANAGER_H_

#import "ios/web/public/web_state/js/crw_js_injection_manager.h"

// Loads the JavaScript file window_id.js which sets __gCrWeb.windowId and
// manages the windowId for messages.
@interface CRWJSWindowIdManager : CRWJSInjectionManager

// A unique window ID is assigned when the script is injected.
@property(nonatomic, copy) NSString* windowId;

@end

#endif  // IOS_WEB_WEB_STATE_JS_CRW_JS_WINDOW_ID_MANAGER_H_
