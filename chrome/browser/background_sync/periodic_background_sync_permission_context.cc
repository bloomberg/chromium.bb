// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/periodic_background_sync_permission_context.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"

PeriodicBackgroundSyncPermissionContext::
    PeriodicBackgroundSyncPermissionContext(Profile* profile)
    : PermissionContextBase(profile,
                            CONTENT_SETTINGS_TYPE_PERIODIC_BACKGROUND_SYNC,
                            blink::mojom::FeaturePolicyFeature::kNotFound) {}

bool PeriodicBackgroundSyncPermissionContext::IsRestrictedToSecureOrigins()
    const {
  return true;
}

ContentSetting
PeriodicBackgroundSyncPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(features::kPeriodicBackgroundSync))
    return CONTENT_SETTING_BLOCK;

  // TODO(crbug.com/925297): Add PWA detection logic, and restrict the feature
  // to origins with a PWA installed.

  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  DCHECK(host_content_settings_map);

  return PermissionContextBase::GetPermissionStatusInternal(
      render_frame_host, requesting_origin, embedding_origin);
}

void PeriodicBackgroundSyncPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    const BrowserPermissionCallback& callback) {
  // The user should never be prompted to authorize Periodic Background Sync
  // from PeriodicBackgroundSyncPermissionContext.
  NOTREACHED();
}

void PeriodicBackgroundSyncPermissionContext::NotifyPermissionSet(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const BrowserPermissionCallback& callback,
    bool persist,
    ContentSetting content_setting) {
  DCHECK(!persist);
  PermissionContextBase::NotifyPermissionSet(
      id, requesting_origin, embedding_origin, std::move(callback), persist,
      content_setting);
}
