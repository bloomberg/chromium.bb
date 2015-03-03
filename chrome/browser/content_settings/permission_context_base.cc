// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/permission_context_base.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/content_settings/permission_bubble_request_impl.h"
#include "chrome/browser/content_settings/permission_context_uma_util.h"
#include "chrome/browser/content_settings/permission_queue_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void PermissionContextBase::RequestPermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool user_gesture,
    const BrowserPermissionCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DecidePermission(web_contents,
                   id,
                   requesting_frame.GetOrigin(),
                   web_contents->GetLastCommittedURL().GetOrigin(),
                   user_gesture,
                   callback);
}

ContentSetting PermissionContextBase::GetPermissionStatus(
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  return profile_->GetHostContentSettingsMap()->GetContentSetting(
      requesting_origin, embedding_origin, permission_type_, std::string());
}

void PermissionContextBase::ResetPermission(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  profile_->GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(requesting_origin),
      ContentSettingsPattern::FromURLNoWildcard(embedding_origin),
      permission_type_, std::string(), CONTENT_SETTING_DEFAULT);
}

void PermissionContextBase::CancelPermissionRequest(
    content::WebContents* web_contents,
    const PermissionRequestID& id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (PermissionBubbleManager::Enabled()) {
    PermissionBubbleRequest* cancelling =
        pending_bubbles_.get(id.ToString());
    if (cancelling != NULL && web_contents != NULL &&
        PermissionBubbleManager::FromWebContents(web_contents) != NULL) {
      PermissionBubbleManager::FromWebContents(web_contents)->
          CancelRequest(cancelling);
    }
    return;
  }

  GetQueueController()->CancelInfoBarRequest(id);
}

void PermissionContextBase::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    const BrowserPermissionCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (!requesting_origin.is_valid() || !embedding_origin.is_valid()) {
    DVLOG(1)
        << "Attempt to use " << content_settings::GetTypeName(permission_type_)
        << " from an invalid URL: " << requesting_origin
        << "," << embedding_origin
        << " (" << content_settings::GetTypeName(permission_type_)
        << " is not supported in popups)";
    NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                        false /* persist */, CONTENT_SETTING_BLOCK);
    return;
  }

  ContentSetting content_setting =
      profile_->GetHostContentSettingsMap()
          ->GetContentSettingAndMaybeUpdateLastUsage(
              requesting_origin, embedding_origin, permission_type_,
              std::string());

  if (content_setting == CONTENT_SETTING_ALLOW ||
      content_setting == CONTENT_SETTING_BLOCK) {
    NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                        false /* persist */, content_setting);
    return;
  }

  PermissionContextUmaUtil::PermissionRequested(
      permission_type_, requesting_origin);

  if (PermissionBubbleManager::Enabled()) {
    if (pending_bubbles_.get(id.ToString()) != NULL)
      return;
    PermissionBubbleManager* bubble_manager =
        PermissionBubbleManager::FromWebContents(web_contents);
    DCHECK(bubble_manager);
    scoped_ptr<PermissionBubbleRequest> request_ptr(
        new PermissionBubbleRequestImpl(
            requesting_origin, user_gesture, permission_type_,
            profile_->GetPrefs()->GetString(prefs::kAcceptLanguages),
            base::Bind(&PermissionContextBase::PermissionDecided,
                       weak_factory_.GetWeakPtr(), id, requesting_origin,
                       embedding_origin, callback),
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
      id, requesting_origin, embedding_origin,
      base::Bind(&PermissionContextBase::PermissionDecided,
                 weak_factory_.GetWeakPtr(), id, requesting_origin,
                 embedding_origin, callback,
                 // the queue controller takes care of persisting the
                 // permission
                 false));
}

void PermissionContextBase::PermissionDecided(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const BrowserPermissionCallback& callback,
    bool persist,
    ContentSetting content_setting) {
  // Infobar persistance and its related UMA is tracked on the infobar
  // controller directly.
  if (PermissionBubbleManager::Enabled()) {
    if (persist) {
      DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
             content_setting == CONTENT_SETTING_BLOCK);
      if (CONTENT_SETTING_ALLOW)
        PermissionContextUmaUtil::PermissionGranted(permission_type_,
                                                    requesting_origin);
      else
        PermissionContextUmaUtil::PermissionDenied(permission_type_,
                                                   requesting_origin);
    } else {
      DCHECK_EQ(content_setting, CONTENT_SETTING_DEFAULT);
      PermissionContextUmaUtil::PermissionDismissed(permission_type_,
                                                    requesting_origin);
    }
  }

  NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                      persist, content_setting);
}

PermissionQueueController* PermissionContextBase::GetQueueController() {
  return permission_queue_controller_.get();
}

Profile* PermissionContextBase::profile() const {
  return profile_;
}

void PermissionContextBase::NotifyPermissionSet(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const BrowserPermissionCallback& callback,
    bool persist,
    ContentSetting content_setting) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (persist)
    UpdateContentSetting(requesting_origin, embedding_origin, content_setting);

  UpdateTabContext(id, requesting_origin,
                   content_setting == CONTENT_SETTING_ALLOW);

  if (content_setting == CONTENT_SETTING_DEFAULT) {
    content_setting =
        profile_->GetHostContentSettingsMap()->GetDefaultContentSetting(
            permission_type_, nullptr);
  }

  DCHECK_NE(content_setting, CONTENT_SETTING_DEFAULT);
  callback.Run(content_setting);
}

void PermissionContextBase::CleanUpBubble(const PermissionRequestID& id) {
  size_t success = pending_bubbles_.erase(id.ToString());
  DCHECK(success == 1) << "Missing request " << id.ToString();
}

void PermissionContextBase::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    ContentSetting content_setting) {
  DCHECK_EQ(requesting_origin, requesting_origin.GetOrigin());
  DCHECK_EQ(embedding_origin, embedding_origin.GetOrigin());
  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
         content_setting == CONTENT_SETTING_BLOCK);

  profile_->GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(requesting_origin),
      ContentSettingsPattern::FromURLNoWildcard(embedding_origin),
      permission_type_, std::string(), content_setting);
}
