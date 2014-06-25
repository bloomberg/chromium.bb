// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/permission_context_base.h"

#include "base/logging.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/permission_queue_controller.h"
#include "chrome/browser/content_settings/permission_request_id.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace gcm {

PermissionContextBase::PermissionContextBase(
    Profile* profile,
    const ContentSettingsType permission_type)
    : profile_(profile),
      permission_type_(permission_type),
      weak_factory_(this) {
  permission_queue_controller_.reset(
      new PermissionQueueController(profile_, permission_type_));
}

PermissionContextBase::~PermissionContextBase() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void PermissionContextBase::RequestPermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool user_gesture,
    const BrowserPermissionCallback& callback) {
  // TODO(miguelg): Add UMA instrumentation.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  DecidePermission(web_contents,
                   id,
                   requesting_frame,
                   requesting_frame,
                   user_gesture,
                   callback);
}

void PermissionContextBase::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    bool user_gesture,
    const BrowserPermissionCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ContentSetting content_setting =
      profile_->GetHostContentSettingsMap()->GetContentSetting(
          requesting_frame, embedder, permission_type_, std::string());
  switch (content_setting) {
    case CONTENT_SETTING_BLOCK:
      PermissionDecided(id, requesting_frame, embedder, callback, false);
      break;
    case CONTENT_SETTING_ALLOW:
      PermissionDecided(id, requesting_frame, embedder, callback, true);
      break;
    default:
      // TODO(miguelg): implement bubble support.

      // TODO(gbillock): Delete this and the infobar delegate when
      // we're using only bubbles. crbug.com/337458
      GetQueueController()->CreateInfoBarRequest(
          id,
          requesting_frame,
          embedder,
          std::string(),
          base::Bind(&PermissionContextBase::NotifyPermissionSet,
                     weak_factory_.GetWeakPtr(),
                     id,
                     requesting_frame,
                     callback));
  }
}

void PermissionContextBase::PermissionDecided(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    const BrowserPermissionCallback& callback,
    bool allowed) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  NotifyPermissionSet(id, requesting_frame, callback, allowed);
}

PermissionQueueController* PermissionContextBase::GetQueueController() {
  return permission_queue_controller_.get();
}

void PermissionContextBase::NotifyPermissionSet(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const BrowserPermissionCallback& callback,
    bool allowed) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  UpdateTabContext(id, requesting_frame, allowed);

  callback.Run(allowed);
}

}  // namespace gcm
