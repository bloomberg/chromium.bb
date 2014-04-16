// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_BROWSER_PLUGIN_PERMISSION_TYPE_H_
#define CONTENT_COMMON_BROWSER_PLUGIN_PERMISSION_TYPE_H_

enum BrowserPluginPermissionType {
  // Unknown type of permission request.
  BROWSER_PLUGIN_PERMISSION_TYPE_UNKNOWN,

  BROWSER_PLUGIN_PERMISSION_TYPE_POINTER_LOCK,

  // New window requests.
  // Note: Even though new windows don't use the permission API, the new window
  // API is sufficiently similar that it's convenient to consider it a
  // permission type for code reuse.
  BROWSER_PLUGIN_PERMISSION_TYPE_NEW_WINDOW,

  // JavaScript Dialogs: prompt, alert, confirm
  // Note: Even through dialogs do not use the permission API, the dialog API
  // is sufficiently similiar that it's convenient to consider it a permission
  // type for code reuse.
  BROWSER_PLUGIN_PERMISSION_TYPE_JAVASCRIPT_DIALOG,

  BROWSER_PLUGIN_PERMISSION_TYPE_CONTENT_END,
};

#endif  // CONTENT_COMMON_BROWSER_PLUGIN_PERMISSION_TYPE_H_
