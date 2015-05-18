// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_WEB_VIEW_TYPE_UTIL_H_
#define IOS_CHROME_BROWSER_WEB_WEB_VIEW_TYPE_UTIL_H_

#include "ios/web/public/web_view_type.h"

// Utilities for getting the web view type to use in the application, or to
// wrap web view creation APIs with versions that automatically select the
// right type.
namespace web_view_type_util {

// Returns the type to use for all web views in the application.
web::WebViewType GetWebViewType();

}  // web_view_type_util

#endif  // IOS_CHROME_BROWSER_WEB_WEB_VIEW_TYPE_UTIL_H_
