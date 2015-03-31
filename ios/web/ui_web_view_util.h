// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_UI_WEB_VIEW_UTIL_H_
#define IOS_WEB_UI_WEB_VIEW_UTIL_H_

#include <Foundation/Foundation.h>

namespace web {

// Registers the user agent encoding |request_group_id| in the user defaults.
// This is a utility method, used to workaround crbug.com/228397. Do not use
// it for other purposes.
void BuildAndRegisterUserAgentForUIWebView(NSString* request_group_id,
                                           BOOL use_desktop_user_agent);

// Registers |user_agent| as the user agent string to be used by the UIWebView
// instances that are created from now on.
void RegisterUserAgentForUIWebView(NSString* user_agent);

}  // namespace web

#endif  // IOS_WEB_UI_WEB_VIEW_UTIL_H_
