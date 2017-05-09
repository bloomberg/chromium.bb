// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_PERMISSION_AW_PERMISSION_REQUEST_DELEGATE_H
#define ANDROID_WEBVIEW_NATIVE_PERMISSION_AW_PERMISSION_REQUEST_DELEGATE_H

#include <stdint.h>

#include "base/macros.h"
#include "url/gurl.h"

namespace android_webview {

// The delegate interface to be implemented for a specific permission request.
class AwPermissionRequestDelegate {
 public:
  AwPermissionRequestDelegate();
  virtual ~AwPermissionRequestDelegate();

  // Get the origin which initiated the permission request.
  virtual const GURL& GetOrigin() = 0;

  // Get the resources the origin wanted to access.
  virtual int64_t GetResources() = 0;

  // Notify the permission request is allowed or not.
  virtual void NotifyRequestResult(bool allowed) = 0;

  DISALLOW_COPY_AND_ASSIGN(AwPermissionRequestDelegate);
};

}  // namespace android_webivew

#endif  // ANDROID_WEBVIEW_NATIVE_PERMISSION_AW_PERMISSION_REQUEST_DELEGATE_H
