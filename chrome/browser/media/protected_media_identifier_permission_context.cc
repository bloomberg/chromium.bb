// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/protected_media_identifier_permission_context.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/permission_request_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/settings/cros_settings_names.h"
#endif

ProtectedMediaIdentifierPermissionContext::
    ProtectedMediaIdentifierPermissionContext(Profile* profile)
    : PermissionContextBase(profile,
                            CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER) {
}

ProtectedMediaIdentifierPermissionContext::
    ~ProtectedMediaIdentifierPermissionContext() {
}

void ProtectedMediaIdentifierPermissionContext::RequestPermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_frame_origin,
    bool user_gesture,
    const BrowserPermissionCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!IsProtectedMediaIdentifierEnabled()) {
    NotifyPermissionSet(id,
                        requesting_frame_origin,
                        web_contents->GetLastCommittedURL().GetOrigin(),
                        callback, false, false);
    return;
  }

  PermissionContextBase::RequestPermission(web_contents, id,
                                           requesting_frame_origin,
                                           user_gesture,
                                           callback);
}

ContentSetting ProtectedMediaIdentifierPermissionContext::GetPermissionStatus(
      const GURL& requesting_origin,
      const GURL& embedding_origin) const {
  if (!IsProtectedMediaIdentifierEnabled())
    return CONTENT_SETTING_BLOCK;

  return PermissionContextBase::GetPermissionStatus(requesting_origin,
                                                    embedding_origin);
}

void ProtectedMediaIdentifierPermissionContext::UpdateTabContext(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool allowed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // WebContents may have gone away.
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::Get(id.render_process_id(),
                                      id.render_view_id());
  if (content_settings) {
    content_settings->OnProtectedMediaIdentifierPermissionSet(
        requesting_frame.GetOrigin(), allowed);
  }

}

// TODO(xhwang): We should consolidate the "protected content" related pref
// across platforms.
bool ProtectedMediaIdentifierPermissionContext::
    IsProtectedMediaIdentifierEnabled() const {
  bool enabled = false;

#if defined(OS_ANDROID)
  enabled = profile()->GetPrefs()->GetBoolean(
      prefs::kProtectedMediaIdentifierEnabled);
#endif

#if defined(OS_CHROMEOS)
  // This could be disabled by the device policy.
  bool enabled_for_device = false;
  enabled = chromeos::CrosSettings::Get()->GetBoolean(
                chromeos::kAttestationForContentProtectionEnabled,
                &enabled_for_device) &&
            enabled_for_device &&
            profile()->GetPrefs()->GetBoolean(prefs::kEnableDRM);
#endif

  DVLOG_IF(1, !enabled)
      << "Protected media identifier disabled by the user or by device policy.";
  return enabled;
}
