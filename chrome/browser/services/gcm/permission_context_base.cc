// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/permission_context_base.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/permission_queue_controller.h"
#include "chrome/browser/content_settings/permission_request_id.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/permission_bubble_request_impl.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

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
      if (PermissionBubbleManager::Enabled()) {
        PermissionBubbleManager* bubble_manager =
            PermissionBubbleManager::FromWebContents(web_contents);
        DCHECK(bubble_manager);
        scoped_ptr<PermissionBubbleRequest> request_ptr(
            new PermissionBubbleRequestImpl(
                requesting_frame,
                user_gesture,
                permission_type_,
                profile_->GetPrefs()->GetString(prefs::kAcceptLanguages),
                base::Bind(&PermissionContextBase::NotifyPermissionSet,
                           weak_factory_.GetWeakPtr(),
                           id,
                           requesting_frame,
                           embedder,
                           callback),
                           base::Bind(&PermissionContextBase::CleanUpBubble,
                                      weak_factory_.GetWeakPtr(), id)));
        PermissionBubbleRequest* request = request_ptr.get();

        bool inserted = pending_bubbles_.add(
            id.ToString(), request_ptr.Pass()).second;
        DCHECK(inserted) << "Duplicate id " << id.ToString();
        bubble_manager->AddRequest(request);
        return;
      }

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
                     embedder,
                     callback,
                     // the queue controller takes care of persisting the
                     // permission
                     false));
  }
}

void PermissionContextBase::PermissionDecided(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    const BrowserPermissionCallback& callback,
    bool allowed) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  NotifyPermissionSet(id, requesting_frame, embedder, callback, false, allowed);
}

PermissionQueueController* PermissionContextBase::GetQueueController() {
  return permission_queue_controller_.get();
}

void PermissionContextBase::NotifyPermissionSet(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    const BrowserPermissionCallback& callback,
    bool persist,
    bool allowed) {
  if (persist) {
    UpdateContentSetting(requesting_frame, embedder, allowed);
  }
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  UpdateTabContext(id, requesting_frame, allowed);

  callback.Run(allowed);
}

void PermissionContextBase::CleanUpBubble(const PermissionRequestID& id) {
  base::ScopedPtrHashMap<std::string, PermissionBubbleRequest>::iterator it;
  it = pending_bubbles_.find(id.ToString());
  if (it != pending_bubbles_.end()) {
    pending_bubbles_.take_and_erase(it);
    return;
  }

  NOTREACHED() << "Missing request";
}

void PermissionContextBase::UpdateContentSetting(
    const GURL& requesting_frame,
    const GURL& embedder,
    bool allowed) {
  ContentSetting content_setting =
      allowed ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  profile_->GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(requesting_frame.GetOrigin()),
      ContentSettingsPattern::FromURLNoWildcard(embedder.GetOrigin()),
      permission_type_,
      std::string(),
      content_setting);
}

}  // namespace gcm
