// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_JS_CRW_JS_MESSAGE_MANAGER_H_
#define IOS_WEB_PUBLIC_WEB_STATE_JS_CRW_JS_MESSAGE_MANAGER_H_

#import "ios/web/public/web_state/js/crw_js_injection_manager.h"

// Loads the JavaScript file message.js which contains methods allowing other
// scripts to message the main application.
@interface CRWJSMessageManager : CRWJSInjectionManager
@end

#endif  // IOS_WEB_PUBLIC_WEB_STATE_JS_CRW_JS_MESSAGE_MANAGER_H_
