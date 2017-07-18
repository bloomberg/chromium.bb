// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_quota_permission_context.h"

#include "storage/common/quota/quota_types.h"

namespace headless {

HeadlessQuotaPermissionContext::HeadlessQuotaPermissionContext() {}

void HeadlessQuotaPermissionContext::RequestQuotaPermission(
    const content::StorageQuotaParams& params,
    int render_process_id,
    const PermissionCallback& callback) {
  if (params.storage_type != storage::kStorageTypePersistent) {
    // For now we only support requesting quota with this interface
    // for Persistent storage type.
    callback.Run(QUOTA_PERMISSION_RESPONSE_DISALLOW);
    return;
  }

  callback.Run(QUOTA_PERMISSION_RESPONSE_ALLOW);
}

HeadlessQuotaPermissionContext::~HeadlessQuotaPermissionContext() {}

}  // namespace headless
