// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_BROWSER_PLUGIN_BROWSER_PLUGIN_MESSAGE_ENUMS_H_
#define CONTENT_COMMON_BROWSER_PLUGIN_BROWSER_PLUGIN_MESSAGE_ENUMS_H_

enum BrowserPluginPermissionType {
  // Unknown type of permission request.
  BrowserPluginPermissionTypeUnknown,

  // Download.
  BrowserPluginPermissionTypeDownload,

  // Geolocation.
  BrowserPluginPermissionTypeGeolocation,

  // Media access (audio/video) permission request type.
  BrowserPluginPermissionTypeMedia,

  // PointerLock
  BrowserPluginPermissionTypePointerLock,


  // New window requests.
  // Note: Even though new windows don't use the permission API, the new window
  // API is sufficiently similar that it's convenient to consider it a
  // permission type for code reuse.
  BrowserPluginPermissionTypeNewWindow,

  // JavaScript Dialogs: prompt, alert, confirm
  // Note: Even through dialogs do not use the permission API, the dialog API
  // is sufficiently similiar that it's convenient to consider it a permission
  // type for code reuse.
  BrowserPluginPermissionTypeJavaScriptDialog
};

#endif  // CONTENT_COMMON_BROWSER_PLUGIN_BROWSER_PLUGIN_MESSAGE_ENUMS_H_
