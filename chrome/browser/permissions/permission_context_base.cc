// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_context_base.h"

#include <stddef.h>
#include <utility>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"

#if defined(OS_ANDROID)
#include "chrome/browser/permissions/permission_queue_controller.h"
#else
#include "chrome/browser/permissions/permission_bubble_request_impl.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#endif

// static
const char PermissionContextBase::kPermissionsKillSwitchFieldStudy[] =
    "PermissionsKillSwitch";
// static
const char PermissionContextBase::kPermissionsKillSwitchBlockedValue[] =
    "blocked";

PermissionContextBase::PermissionContextBase(
    Profile* profile,
    const content::PermissionType permission_type,
    const ContentSettingsType content_settings_type)
    : profile_(profile),
      permission_type_(permission_type),
      content_settings_type_(content_settings_type),
      weak_factory_(this) {
#if defined(OS_ANDROID)
  permission_queue_controller_.reset(new PermissionQueueController(
      profile_, permission_type_, content_settings_type_));
#endif
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

  // First check if this permission has been disabled.
  if (IsPermissionKillSwitchOn()) {
    // Log to the developer console.
    web_contents->GetMainFrame()->AddMessageToConsole(
        content::CONSOLE_MESSAGE_LEVEL_LOG,
        base::StringPrintf(
            "%s permission has been blocked.",
            PermissionUtil::GetPermissionString(permission_type_).c_str()));
    // The kill switch is enabled for this permission; Block all requests.
    callback.Run(CONTENT_SETTING_BLOCK);
    return;
  }

  GURL requesting_origin = requesting_frame.GetOrigin();
  GURL embedding_origin = web_contents->GetLastCommittedURL().GetOrigin();

  if (!requesting_origin.is_valid() || !embedding_origin.is_valid()) {
    std::string type_name =
        content_settings::WebsiteSettingsRegistry::GetInstance()
            ->Get(content_settings_type_)
            ->name();

    DVLOG(1) << "Attempt to use " << type_name
             << " from an invalid URL: " << requesting_origin << ","
             << embedding_origin << " (" << type_name
             << " is not supported in popups)";
    NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                        false /* persist */, CONTENT_SETTING_BLOCK);
    return;
  }

  ContentSetting content_setting =
      GetPermissionStatus(requesting_origin, embedding_origin);
  if (content_setting == CONTENT_SETTING_ALLOW) {
    HostContentSettingsMapFactory::GetForProfile(profile_)->UpdateLastUsage(
        requesting_origin, embedding_origin, content_settings_type_);
  }
  if (content_setting == CONTENT_SETTING_ALLOW ||
      content_setting == CONTENT_SETTING_BLOCK) {
    NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                        false /* persist */, content_setting);
    return;
  }

  PermissionUmaUtil::PermissionRequested(permission_type_, requesting_origin,
                                         embedding_origin, profile_);

  DecidePermission(web_contents, id, requesting_origin, embedding_origin,
                   user_gesture, callback);
}

ContentSetting PermissionContextBase::GetPermissionStatus(
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {

  // If the permission has been disabled through Finch, block all requests.
  if (IsPermissionKillSwitchOn())
    return CONTENT_SETTING_BLOCK;

  if (IsRestrictedToSecureOrigins() &&
      !content::IsOriginSecure(requesting_origin)) {
    return CONTENT_SETTING_BLOCK;
  }

  return HostContentSettingsMapFactory::GetForProfile(profile_)
      ->GetContentSetting(requesting_origin, embedding_origin,
                          content_settings_type_, std::string());
}

void PermissionContextBase::ResetPermission(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  HostContentSettingsMapFactory::GetForProfile(profile_)->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(requesting_origin),
      ContentSettingsPattern::FromURLNoWildcard(embedding_origin),
      content_settings_type_, std::string(), CONTENT_SETTING_DEFAULT);
}

void PermissionContextBase::CancelPermissionRequest(
    content::WebContents* web_contents,
    const PermissionRequestID& id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if defined(OS_ANDROID)
  GetQueueController()->CancelInfoBarRequest(id);
#else
  PermissionBubbleRequest* cancelling = pending_bubbles_.get(id.ToString());
  if (cancelling != NULL && web_contents != NULL &&
      PermissionBubbleManager::FromWebContents(web_contents) != NULL) {
    PermissionBubbleManager::FromWebContents(web_contents)
        ->CancelRequest(cancelling);
  }
#endif
}

void PermissionContextBase::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    const BrowserPermissionCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if !defined(OS_ANDROID)
  PermissionBubbleManager* bubble_manager =
      PermissionBubbleManager::FromWebContents(web_contents);
  // TODO(felt): sometimes |bubble_manager| is null. This check is meant to
  // prevent crashes. See crbug.com/457091.
  if (!bubble_manager)
    return;
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

  bool inserted =
      pending_bubbles_.add(id.ToString(), std::move(request_ptr)).second;
  DCHECK(inserted) << "Duplicate id " << id.ToString();
  bubble_manager->AddRequest(request);
#else
  GetQueueController()->CreateInfoBarRequest(
      id, requesting_origin, embedding_origin,
      base::Bind(&PermissionContextBase::PermissionDecided,
                 weak_factory_.GetWeakPtr(), id, requesting_origin,
                 embedding_origin, callback,
                 // the queue controller takes care of persisting the
                 // permission
                 false));
#endif
}

void PermissionContextBase::PermissionDecided(
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const BrowserPermissionCallback& callback,
    bool persist,
    ContentSetting content_setting) {
#if !defined(OS_ANDROID)
  // Infobar persistence and its related UMA is tracked on the infobar
  // controller directly.
  if (persist) {
    DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
           content_setting == CONTENT_SETTING_BLOCK);
    if (content_setting == CONTENT_SETTING_ALLOW)
      PermissionUmaUtil::PermissionGranted(permission_type_, requesting_origin);
    else
      PermissionUmaUtil::PermissionDenied(permission_type_, requesting_origin);
  } else {
    DCHECK_EQ(content_setting, CONTENT_SETTING_DEFAULT);
    PermissionUmaUtil::PermissionDismissed(permission_type_, requesting_origin);
  }
#endif

  NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                      persist, content_setting);
}

#if defined(OS_ANDROID)
PermissionQueueController* PermissionContextBase::GetQueueController() {
  return permission_queue_controller_.get();
}
#endif

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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (persist)
    UpdateContentSetting(requesting_origin, embedding_origin, content_setting);

  UpdateTabContext(id, requesting_origin,
                   content_setting == CONTENT_SETTING_ALLOW);

  if (content_setting == CONTENT_SETTING_DEFAULT) {
    content_setting =
        HostContentSettingsMapFactory::GetForProfile(profile_)
            ->GetDefaultContentSetting(content_settings_type_, nullptr);
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

  HostContentSettingsMapFactory::GetForProfile(profile_)->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(requesting_origin),
      ContentSettingsPattern::FromURLNoWildcard(embedding_origin),
      content_settings_type_, std::string(), content_setting);
}

bool PermissionContextBase::IsPermissionKillSwitchOn() const {
  const std::string param = variations::GetVariationParamValue(
      kPermissionsKillSwitchFieldStudy,
      PermissionUtil::GetPermissionString(permission_type_));

  return param == kPermissionsKillSwitchBlockedValue;
}
