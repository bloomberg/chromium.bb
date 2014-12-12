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

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/suggest_permission_util.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"

using extensions::APIPermission;
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

  GURL embedder = web_contents->GetLastCommittedURL().GetOrigin();

#if defined(ENABLE_EXTENSIONS)
  if (extensions::GetViewType(web_contents) !=
      extensions::VIEW_TYPE_TAB_CONTENTS) {
    // The tab may have gone away, or the request may not be from a tab at all.
    LOG(WARNING)
        << "Attempt to use protected media identifier in tabless renderer: "
        << id.ToString()
        << " (can't prompt user without a visible tab)";
    NotifyPermissionSet(id, origin, embedder, callback, false, false);
    return;
  }
#endif

  if (!requesting_frame_origin.is_valid() || !embedder.is_valid()) {
    LOG(WARNING)
        << "Attempt to use protected media identifier from an invalid URL: "
        << requesting_frame_origin << "," << embedder
        << " (proteced media identifier is not supported in popups)";
    NotifyPermissionSet(id, requesting_frame_origin, embedder,
                        callback, false, false);
    return;
  }

#if defined(OS_ANDROID)
  // Check if the protected media identifier master switch is disabled.
  if (!profile()->GetPrefs()->GetBoolean(
        prefs::kProtectedMediaIdentifierEnabled)) {
    NotifyPermissionSet(id, requesting_frame_origin, embedder, callback,
                        false, false);
    return;
  }
#endif

  PermissionContextBase::RequestPermission(web_contents, id,
                                           requesting_frame_origin,
                                           user_gesture,
                                           callback);
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
