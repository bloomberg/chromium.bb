// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_JS_CRW_JS_PLUGIN_PLACEHOLDER_MANAGER_H_
#define IOS_WEB_WEB_STATE_JS_CRW_JS_PLUGIN_PLACEHOLDER_MANAGER_H_

#import "ios/web/public/web_state/js/crw_js_injection_manager.h"

// Loads the JavaScript file plugin_placeholder.js, which contains logic for
// adding placeholders to plugins on the page. It will be evaluated on a page
// where plugins that need placeholders have been detected.
@interface CRWJSPluginPlaceholderManager : CRWJSInjectionManager

@end

#endif  // IOS_WEB_WEB_STATE_JS_CRW_JS_PLUGIN_PLACEHOLDER_MANAGER_H_
