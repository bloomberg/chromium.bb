// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_JS_CRW_JS_COMMON_MANAGER_H_
#define IOS_WEB_WEB_STATE_JS_CRW_JS_COMMON_MANAGER_H_

#import "ios/web/public/web_state/js/crw_js_injection_manager.h"

@class CRWJSInjectionReceiver;

// Loads JavaScript file common.js which contains common JavaScript methods that
// can be called by other CRWJSInjectionManagers.
@interface CRWJSCommonManager : CRWJSInjectionManager
@end

#endif  // IOS_WEB_WEB_STATE_JS_CRW_JS_COMMON_MANAGER_H_
