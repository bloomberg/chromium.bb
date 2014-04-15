// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUESTVIEW_WEBVIEW_WEBVIEW_PERMISSION_TYPES_H_
#define CHROME_BROWSER_GUESTVIEW_WEBVIEW_WEBVIEW_PERMISSION_TYPES_H_

#include "content/public/common/browser_plugin_permission_type.h"

enum WebViewPermissionType {
  WEB_VIEW_PERMISSION_TYPE_GEOLOCATION =
      BROWSER_PLUGIN_PERMISSION_TYPE_CONTENT_END + 1,

  WEB_VIEW_PERMISSION_TYPE_LOAD_PLUGIN
};

#endif  // CHROME_BROWSER_GUESTVIEW_WEBVIEW_WEBVIEW_PERMISSION_TYPES_H_
