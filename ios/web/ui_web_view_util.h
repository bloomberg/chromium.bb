// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_UI_WEB_VIEW_UTIL_H_
#define IOS_WEB_UI_WEB_VIEW_UTIL_H_

#include <Foundation/Foundation.h>

namespace web {

// Registers |user_agent| as the user agent string to be used by the UIWebView
// instances that are created from now on.
void RegisterUserAgentForUIWebView(NSString* user_agent);

}  // namespace web

#endif  // IOS_WEB_UI_WEB_VIEW_UTIL_H_
