// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_PERMISSION_PERMISSION_REQUEST_HANDLER_CLIENT_H
#define ANDROID_WEBVIEW_NATIVE_PERMISSION_PERMISSION_REQUEST_HANDLER_CLIENT_H

namespace android_webview {

class AwPermissionRequest;

class PermissionRequestHandlerClient {
 public:
  PermissionRequestHandlerClient();
  virtual ~PermissionRequestHandlerClient();

  virtual void OnPermissionRequest(AwPermissionRequest* request) = 0;
  virtual void OnPermissionRequestCanceled(AwPermissionRequest* request) = 0;
};

}  // namespace android_webivew

#endif  // ANDROID_WEBVIEW_NATIVE_PERMISSION_PERMISSION_REQUEST_HANDLER_CLIENT_H
