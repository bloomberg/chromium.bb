// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_JS_PAGE_SCRIPT_UTIL_H_
#define IOS_WEB_WEB_STATE_JS_PAGE_SCRIPT_UTIL_H_

#import <Foundation/Foundation.h>

#include "ios/web/public/web_view_type.h"

namespace web {

// Returns an autoreleased string containing the JavaScript loaded from a
// bundled resource file with the given name (excluding extension).
NSString* GetPageScript(NSString* script_file_name);

// Returns an autoreleased string containing the JavaScript to be injected into
// the web view as early as possible. The type of a target web view must match
// |web_view_type|.
NSString* GetEarlyPageScript(WebViewType web_view_type);

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_JS_PAGE_SCRIPT_UTIL_H_
