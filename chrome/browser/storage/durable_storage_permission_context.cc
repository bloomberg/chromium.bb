// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage/durable_storage_permission_context.h"

#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "content/public/browser/child_process_security_policy.h"
#include "url/gurl.h"

DurableStoragePermissionContext::DurableStoragePermissionContext(
    Profile* profile)
    : PermissionContextBase(profile, CONTENT_SETTINGS_TYPE_DURABLE_STORAGE) {
}

bool DurableStoragePermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
