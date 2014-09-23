// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/permission_context_base.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/permission_bubble_request_impl.h"
#include "chrome/browser/content_settings/permission_context_uma_util.h"
#include "chrome/browser/content_settings/permission_queue_controller.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/permission_request_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

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
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  DecidePermission(web_contents,
                   id,
                   requesting_frame.GetOrigin(),
                   web_contents->GetLastCommittedURL().GetOrigin(),
                   user_gesture,
                   callback);
}

void PermissionContextBase::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedder_origin,
    bool user_gesture,
    const BrowserPermissionCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ContentSetting content_setting =
      profile_->GetHostContentSettingsMap()->GetContentSetting(
          requesting_origin, embedder_origin, permission_type_, std::string());
  switch (content_setting) {
    case CONTENT_SETTING_BLOCK:
      NotifyPermissionSet(id, requesting_origin, embedder_origin,
                          callback, false /* persist */, false /* granted */);
      return;
    case CONTENT_SETTING_ALLOW:
      NotifyPermissionSet(id, requesting_origin, embedder_origin,
                          callback, false /* persist */, true /* granted */);
      return;
    default:
      break;
  }

  PermissionContextUmaUtil::PermissionRequested(permission_type_);

  if (PermissionBubbleManager::Enabled()) {
    PermissionBubbleManager* bubble_manager =
        PermissionBubbleManager::FromWebContents(web_contents);
    DCHECK(bubble_manager);
    scoped_ptr<PermissionBubbleRequest> request_ptr(
        new PermissionBubbleRequestImpl(
            requesting_origin,
            user_gesture,
            permission_type_,
            profile_->GetPrefs()->GetString(prefs::kAcceptLanguages),
            base::Bind(&PermissionContextBase::PermissionDecided,
                       weak_factory_.GetWeakPtr(),
                       id,
                       requesting_origin,
                       embedder_origin,
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
      requesting_origin,
      embedder_origin,
      base::Bind(&PermissionContextBase::PermissionDecided,
                 weak_factory_.GetWeakPtr(),
                 id,
                 requesting_origin,
                 embedder_origin,
                 callback,
                 // the queue controller takes care of persisting the
                 // permission
                 false));
}

void PermissionContextBase::PermissionDecided(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedder_origin,
    const BrowserPermissionCallback& callback,
    bool persist,
    bool allowed) {

  // Infobar persistance and its related UMA is tracked on the infobar
  // controller directly.
  if (PermissionBubbleManager::Enabled()) {
    if (persist) {
      if (allowed)
        PermissionContextUmaUtil::PermissionGranted(permission_type_);
      else
        PermissionContextUmaUtil::PermissionDenied(permission_type_);
    } else {
      PermissionContextUmaUtil::PermissionDismissed(permission_type_);
    }
  }

  NotifyPermissionSet(
      id, requesting_origin, embedder_origin, callback, persist, allowed);
}

PermissionQueueController* PermissionContextBase::GetQueueController() {
  return permission_queue_controller_.get();
}

void PermissionContextBase::NotifyPermissionSet(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedder_origin,
    const BrowserPermissionCallback& callback,
    bool persist,
    bool allowed) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (persist)
    UpdateContentSetting(requesting_origin, embedder_origin, allowed);

  UpdateTabContext(id, requesting_origin, allowed);
  callback.Run(allowed);
}

void PermissionContextBase::CleanUpBubble(const PermissionRequestID& id) {
  size_t success = pending_bubbles_.erase(id.ToString());
  DCHECK(success == 1) << "Missing request " << id.ToString();
}

void PermissionContextBase::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedder_origin,
    bool allowed) {
  DCHECK_EQ(requesting_origin, requesting_origin.GetOrigin());
  DCHECK_EQ(embedder_origin, embedder_origin.GetOrigin());
  ContentSetting content_setting =
      allowed ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  profile_->GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(requesting_origin),
      ContentSettingsPattern::FromURLNoWildcard(embedder_origin),
      permission_type_,
      std::string(),
      content_setting);
}
