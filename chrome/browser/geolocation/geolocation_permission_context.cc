// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context.h"

#include "base/bind.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/permission_request_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/geolocation_provider.h"
#include "content/public/browser/web_contents.h"


GeolocationPermissionContext::GeolocationPermissionContext(
    Profile* profile)
    : PermissionContextBase(profile, CONTENT_SETTINGS_TYPE_GEOLOCATION),
      extensions_context_(profile) {
}

GeolocationPermissionContext::~GeolocationPermissionContext() {
}

void GeolocationPermissionContext::RequestPermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_frame_origin,
    bool user_gesture,
    const BrowserPermissionCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool permission_set;
  bool new_permission;
  if (extensions_context_.RequestPermission(
      web_contents, id, id.bridge_id(), requesting_frame_origin, user_gesture,
      callback, &permission_set, &new_permission)) {
    if (permission_set) {
      ContentSetting content_setting =
          new_permission ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
      NotifyPermissionSet(id,
                          requesting_frame_origin,
                          web_contents->GetLastCommittedURL().GetOrigin(),
                          callback,
                          true,
                          content_setting);
    }
    return;
  }

  PermissionContextBase::RequestPermission(web_contents, id,
                                           requesting_frame_origin,
                                           user_gesture,
                                           callback);
}

void GeolocationPermissionContext::CancelPermissionRequest(
    content::WebContents* web_contents,
    const PermissionRequestID& id) {

    if (extensions_context_.CancelPermissionRequest(
        web_contents, id.bridge_id()))
      return;
    PermissionContextBase::CancelPermissionRequest(web_contents, id);
}

void GeolocationPermissionContext::UpdateTabContext(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool allowed) {
  // WebContents may have gone away (or not exists for extension).
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::Get(id.render_process_id(),
                                      id.render_view_id());
  if (content_settings)
    content_settings->OnGeolocationPermissionSet(
        requesting_frame.GetOrigin(), allowed);

  if (allowed) {
    content::GeolocationProvider::GetInstance()
        ->UserDidOptIntoLocationServices();
  }
}
