// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_JS_PAGE_SCRIPT_UTIL_H_
#define IOS_WEB_WEB_STATE_JS_PAGE_SCRIPT_UTIL_H_

#import <Foundation/Foundation.h>


namespace web {

class BrowserState;

// Returns an autoreleased string containing the JavaScript loaded from a
// bundled resource file with the given name (excluding extension).
NSString* GetPageScript(NSString* script_file_name);

// Returns an autoreleased string containing the JavaScript to be injected into
// the web view as early as possible.
NSString* GetEarlyPageScript(BrowserState* browser_state);

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_JS_PAGE_SCRIPT_UTIL_H_
