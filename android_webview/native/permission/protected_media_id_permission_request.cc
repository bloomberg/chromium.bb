// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/permission/protected_media_id_permission_request.h"

#include "android_webview/native/permission/aw_permission_request.h"
#include "base/callback.h"

namespace android_webview {

ProtectedMediaIdPermissionRequest::ProtectedMediaIdPermissionRequest(
    const GURL& origin,
    const base::Callback<void(bool)>& callback)
    : origin_(origin),
      callback_(callback) {
}

ProtectedMediaIdPermissionRequest::~ProtectedMediaIdPermissionRequest() {
}

void ProtectedMediaIdPermissionRequest::NotifyRequestResult(bool allowed) {
  callback_.Run(allowed);
}

const GURL& ProtectedMediaIdPermissionRequest::GetOrigin() {
  return origin_;
}

int64 ProtectedMediaIdPermissionRequest::GetResources() {
  return AwPermissionRequest::ProtectedMediaId;
}

}  // namespace android_webview
