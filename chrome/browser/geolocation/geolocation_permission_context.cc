// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context.h"

#include "base/bind.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/geolocation/geolocation_provider.h"
#include "url/origin.h"

GeolocationPermissionContext::GeolocationPermissionContext(Profile* profile)
    : PermissionContextBase(profile,
                            CONTENT_SETTINGS_TYPE_GEOLOCATION,
                            blink::FeaturePolicyFeature::kGeolocation),
      extensions_context_(profile) {}

GeolocationPermissionContext::~GeolocationPermissionContext() {
}

void GeolocationPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    const BrowserPermissionCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool permission_set;
  bool new_permission;
  if (extensions_context_.DecidePermission(
      web_contents, id, id.request_id(), requesting_origin, user_gesture,
      callback, &permission_set, &new_permission)) {
    if (permission_set) {
      ContentSetting content_setting =
          new_permission ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
      NotifyPermissionSet(id,
                          requesting_origin,
                          web_contents->GetLastCommittedURL().GetOrigin(),
                          callback,
                          false /* persist */,
                          content_setting);
    }
    return;
  }

  PermissionContextBase::DecidePermission(web_contents,
                                          id,
                                          requesting_origin,
                                          embedding_origin,
                                          user_gesture,
                                          callback);
}

void GeolocationPermissionContext::CancelPermissionRequest(
    content::WebContents* web_contents,
    const PermissionRequestID& id) {

    if (extensions_context_.CancelPermissionRequest(
        web_contents, id.request_id()))
      return;
    PermissionContextBase::CancelPermissionRequest(web_contents, id);
}

void GeolocationPermissionContext::UpdateTabContext(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool allowed) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::GetForFrame(id.render_process_id(),
                                              id.render_frame_id());

  // WebContents might not exist (extensions) or no longer exist. In which case,
  // TabSpecificContentSettings will be null.
  if (content_settings)
    content_settings->OnGeolocationPermissionSet(
        requesting_frame.GetOrigin(), allowed);

  if (allowed) {
    device::GeolocationProvider::GetInstance()
        ->UserDidOptIntoLocationServices();
  }
}

bool GeolocationPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
