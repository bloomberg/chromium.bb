// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/permission/simple_permission_request.h"

#include "android_webview/native/permission/aw_permission_request.h"
#include "base/callback.h"

namespace android_webview {

SimplePermissionRequest::SimplePermissionRequest(
    const GURL& origin,
    int64 resources,
    const base::Callback<void(bool)>& callback)
    : origin_(origin),
      resources_(resources),
      callback_(callback) {
}

SimplePermissionRequest::~SimplePermissionRequest() {
}

void SimplePermissionRequest::NotifyRequestResult(bool allowed) {
  callback_.Run(allowed);
}

const GURL& SimplePermissionRequest::GetOrigin() {
  return origin_;
}

int64 SimplePermissionRequest::GetResources() {
  return resources_;
}

}  // namespace android_webview
