// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_BROWSER_PLUGIN_MESSAGE_ENUMS_H_
#define CONTENT_COMMON_BROWSER_PLUGIN_MESSAGE_ENUMS_H_

enum BrowserPluginPermissionType {
  // Unknown type of permission request.
  BrowserPluginPermissionTypeUnknown,

  // Media access (audio/video) permission request type.
  BrowserPluginPermissionTypeMedia,
};

#endif  // CONTENT_COMMON_BROWSER_PLUGIN_MESSAGE_ENUMS_H_
