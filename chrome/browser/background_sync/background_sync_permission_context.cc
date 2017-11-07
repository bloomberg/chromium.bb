// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/background_sync_permission_context.h"

#include "base/logging.h"
#include "components/content_settings/core/common/content_settings_types.h"

BackgroundSyncPermissionContext::BackgroundSyncPermissionContext(
    Profile* profile)
    : PermissionContextBase(profile,
                            CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC,
                            blink::FeaturePolicyFeature::kNotFound) {}

void BackgroundSyncPermissionContext::CancelPermissionRequest(
    content::WebContents* web_contents,
    const PermissionRequestID& id) {
  // Background sync permission requests are resolved instantly without
  // prompting the user, there is no way to cancel them.
  NOTREACHED();
}

void BackgroundSyncPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    const BrowserPermissionCallback& callback) {
  // The user should never be prompted to authorize background sync.
  NOTREACHED();
}

bool BackgroundSyncPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
