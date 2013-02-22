// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_quota_permission_context.h"

#include "base/logging.h"

using content::QuotaPermissionContext;

namespace android_webview {

AwQuotaPermissionContext::AwQuotaPermissionContext() {
}

AwQuotaPermissionContext::~AwQuotaPermissionContext() {
}

void AwQuotaPermissionContext::RequestQuotaPermission(
    const GURL& origin_url,
    quota::StorageType type,
    int64 new_quota,
    int render_process_id,
    int render_view_id,
    const PermissionCallback& callback) {
  // Android WebView only uses quota::kStorageTypeTemporary type of storage
  // with quota managed automatically, not through this interface. Therefore
  // unconditionally disallow all quota requests here.
  callback.Run(QUOTA_PERMISSION_RESPONSE_DISALLOW);
}

}  // namespace android_webview
