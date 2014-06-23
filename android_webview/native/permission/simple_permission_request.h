// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_PERMISSION_SIMPLE_PERMISSION_REQUEST_H
#define ANDROID_WEBVIEW_NATIVE_PERMISSION_SIMPLE_PERMISSION_REQUEST_H

#include "android_webview/native/permission/aw_permission_request_delegate.h"
#include "base/callback.h"

namespace android_webview {

// The class is used to handle the simple permission request which just needs
// a callback with bool parameter to indicate the permission granted or not.
class SimplePermissionRequest : public AwPermissionRequestDelegate {
 public:
  SimplePermissionRequest(const GURL& origin,
                          int64 resources,
                          const base::Callback<void(bool)>& callback);
  virtual ~SimplePermissionRequest();

  // AwPermissionRequestDelegate implementation.
  virtual const GURL& GetOrigin() OVERRIDE;
  virtual int64 GetResources() OVERRIDE;
  virtual void NotifyRequestResult(bool allowed) OVERRIDE;

 private:
  const GURL origin_;
  int64 resources_;
  const base::Callback<void(bool)> callback_;

  DISALLOW_COPY_AND_ASSIGN(SimplePermissionRequest);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_PERMISSION_PROTECTED_MEDIA_ID_PERMISSION_REQUEST_H
