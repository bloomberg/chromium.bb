// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_manager.h"

#include "base/callback.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_context.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#endif

using content::PermissionStatus;
using content::PermissionType;

namespace {

// Helper method to convert ContentSetting to PermissionStatus.
PermissionStatus ContentSettingToPermissionStatus(ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
    case CONTENT_SETTING_SESSION_ONLY:
      return content::PERMISSION_STATUS_GRANTED;
    case CONTENT_SETTING_BLOCK:
      return content::PERMISSION_STATUS_DENIED;
    case CONTENT_SETTING_ASK:
      return content::PERMISSION_STATUS_ASK;
    case CONTENT_SETTING_DETECT_IMPORTANT_CONTENT:
    case CONTENT_SETTING_DEFAULT:
    case CONTENT_SETTING_NUM_SETTINGS:
      break;
  }

  NOTREACHED();
  return content::PERMISSION_STATUS_DENIED;
}

// Helper method to convert PermissionType to ContentSettingType.
ContentSettingsType PermissionTypeToContentSetting(PermissionType permission) {
  switch (permission) {
    case PermissionType::MIDI_SYSEX:
      return CONTENT_SETTINGS_TYPE_MIDI_SYSEX;
    case PermissionType::PUSH_MESSAGING:
      return CONTENT_SETTINGS_TYPE_PUSH_MESSAGING;
    case PermissionType::NOTIFICATIONS:
      return CONTENT_SETTINGS_TYPE_NOTIFICATIONS;
    case PermissionType::GEOLOCATION:
      return CONTENT_SETTINGS_TYPE_GEOLOCATION;
    case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
      return CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER;
#else
      NOTIMPLEMENTED();
      break;
#endif
    case PermissionType::DURABLE_STORAGE:
      return CONTENT_SETTINGS_TYPE_DURABLE_STORAGE;
    case PermissionType::MIDI:
      // This will hit the NOTREACHED below.
      break;
    case PermissionType::AUDIO_CAPTURE:
      return CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC;
    case PermissionType::VIDEO_CAPTURE:
      return CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA;
    case PermissionType::NUM:
      // This will hit the NOTREACHED below.
      break;
  }

  NOTREACHED() << "Unknown content setting for permission "
               << static_cast<int>(permission);
  return CONTENT_SETTINGS_TYPE_DEFAULT;
}

// Returns whether the permission has a constant PermissionStatus value (i.e.
// always approved or always denied)
// The PermissionTypes for which true is returned should be exactly those which
// return nullptr in PermissionContext::Get since they don't have a context.
bool IsConstantPermission(PermissionType type) {
  switch (type) {
    case PermissionType::MIDI:
      return true;
    default:
      return false;
  }
}

// Function used for handling permission types which do not change their
// value i.e. they are always approved or always denied etc.
// CONTENT_SETTING_DEFAULT is returned if the permission needs further handling.
// This function should only be called when IsConstantPermission has returned
// true for the PermissionType.
ContentSetting GetContentSettingForConstantPermission(PermissionType type) {
  DCHECK(IsConstantPermission(type));
  switch (type) {
    case PermissionType::MIDI:
      return CONTENT_SETTING_ALLOW;
    default:
      return CONTENT_SETTING_DEFAULT;
  }
}

PermissionStatus GetPermissionStatusForConstantPermission(PermissionType type) {
  return ContentSettingToPermissionStatus(
      GetContentSettingForConstantPermission(type));
}

}  // anonymous namespace

struct PermissionManager::PendingRequest {
  PendingRequest(PermissionType permission,
                 content::RenderFrameHost* render_frame_host)
    : permission(permission),
      render_process_id(render_frame_host->GetProcess()->GetID()),
      render_frame_id(render_frame_host->GetRoutingID()) {
  }

  PermissionType permission;
  int render_process_id;
  int render_frame_id;
};

struct PermissionManager::Subscription {
  PermissionType permission;
  GURL requesting_origin;
  GURL embedding_origin;
  base::Callback<void(PermissionStatus)> callback;
  ContentSetting current_value;
};

PermissionManager::PermissionManager(Profile* profile)
    : profile_(profile),
      weak_ptr_factory_(this) {
}

PermissionManager::~PermissionManager() {
  if (!subscriptions_.IsEmpty())
    HostContentSettingsMapFactory::GetForProfile(profile_)
        ->RemoveObserver(this);
}

int PermissionManager::RequestPermission(
    PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<void(PermissionStatus)>& callback) {
  if (IsConstantPermission(permission)) {
    callback.Run(GetPermissionStatusForConstantPermission(permission));
    return kNoPendingOperation;
  }

  PermissionContextBase* context = PermissionContext::Get(profile_, permission);
  if (!context) {
    callback.Run(content::PERMISSION_STATUS_DENIED);
    return kNoPendingOperation;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (IsPermissionBubbleManagerMissing(web_contents)) {
    callback.Run(
        GetPermissionStatus(permission, requesting_origin,
                            web_contents->GetLastCommittedURL().GetOrigin()));
    return kNoPendingOperation;
  }

  PendingRequest* pending_request = new PendingRequest(
      permission, render_frame_host);
  int request_id = pending_requests_.Add(pending_request);
  const PermissionRequestID request(pending_request->render_process_id,
                                    pending_request->render_frame_id,
                                    request_id);

  context->RequestPermission(
      web_contents, request, requesting_origin, user_gesture,
      base::Bind(&PermissionManager::OnPermissionRequestResponse,
                 weak_ptr_factory_.GetWeakPtr(),
                 request_id,
                 callback));
  return request_id;
}

void PermissionManager::OnPermissionRequestResponse(
    int request_id,
    const base::Callback<void(PermissionStatus)>& callback,
    ContentSetting content_setting) {
  pending_requests_.Remove(request_id);
  callback.Run(ContentSettingToPermissionStatus(content_setting));
}

void PermissionManager::CancelPermissionRequest(int request_id) {
  PendingRequest* pending_request = pending_requests_.Lookup(request_id);
  if (!pending_request)
    return;

  PermissionContextBase* context = PermissionContext::Get(
      profile_, pending_request->permission);
  if (!context)
    return;

  content::WebContents* web_contents = tab_util::GetWebContentsByFrameID(
      pending_request->render_process_id, pending_request->render_frame_id);
  DCHECK(web_contents);
  if (IsPermissionBubbleManagerMissing(web_contents)) {
    pending_requests_.Remove(request_id);
    return;
  }

  const PermissionRequestID request(pending_request->render_process_id,
                                    pending_request->render_frame_id,
                                    request_id);
  context->CancelPermissionRequest(web_contents, request);
  pending_requests_.Remove(request_id);
}

void PermissionManager::ResetPermission(PermissionType permission,
                                        const GURL& requesting_origin,
                                        const GURL& embedding_origin) {
  PermissionContextBase* context = PermissionContext::Get(profile_, permission);
  if (!context)
    return;

  context->ResetPermission(requesting_origin.GetOrigin(),
                           embedding_origin.GetOrigin());
}

PermissionStatus PermissionManager::GetPermissionStatus(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  if (IsConstantPermission(permission))
    return GetPermissionStatusForConstantPermission(permission);

  PermissionContextBase* context = PermissionContext::Get(profile_, permission);
  if (!context)
    return content::PERMISSION_STATUS_DENIED;

  return ContentSettingToPermissionStatus(context->GetPermissionStatus(
      requesting_origin.GetOrigin(), embedding_origin.GetOrigin()));
}

void PermissionManager::RegisterPermissionUsage(PermissionType permission,
                                                const GURL& requesting_origin,
                                                const GURL& embedding_origin) {
  // This is required because constant permissions don't have a
  // ContentSettingsType.
  if (IsConstantPermission(permission))
    return;

  HostContentSettingsMapFactory::GetForProfile(profile_)->UpdateLastUsage(
      requesting_origin,
      embedding_origin,
      PermissionTypeToContentSetting(permission));
}

int PermissionManager::SubscribePermissionStatusChange(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const base::Callback<void(PermissionStatus)>& callback) {
  if (subscriptions_.IsEmpty())
    HostContentSettingsMapFactory::GetForProfile(profile_)->AddObserver(this);

  Subscription* subscription = new Subscription();
  subscription->permission = permission;
  subscription->requesting_origin = requesting_origin;
  subscription->embedding_origin = embedding_origin;
  subscription->callback = callback;

  if (IsConstantPermission(permission)) {
    subscription->current_value = GetContentSettingForConstantPermission(
        permission);
  } else {
    subscription->current_value = PermissionContext::Get(profile_, permission)
        ->GetPermissionStatus(subscription->requesting_origin,
                              subscription->embedding_origin);
  }

  return subscriptions_.Add(subscription);
}

void PermissionManager::UnsubscribePermissionStatusChange(int subscription_id) {
  // Whether |subscription_id| is known will be checked by the Remove() call.
  subscriptions_.Remove(subscription_id);

  if (subscriptions_.IsEmpty())
    HostContentSettingsMapFactory::GetForProfile(profile_)
        ->RemoveObserver(this);
}

bool PermissionManager::IsPermissionBubbleManagerMissing(
    content::WebContents* web_contents) {
  // TODO(felt): Remove this method entirely. Leaving it to make a minimal
  // last-minute merge to 46. See crbug.com/457091 and crbug.com/534631.
  return false;
}

void PermissionManager::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    std::string resource_identifier) {
  std::list<base::Closure> callbacks;

  for (SubscriptionsMap::iterator iter(&subscriptions_);
       !iter.IsAtEnd(); iter.Advance()) {
    Subscription* subscription = iter.GetCurrentValue();
    if (PermissionTypeToContentSetting(subscription->permission) !=
        content_type) {
      continue;
    }

    if (primary_pattern.IsValid() &&
        !primary_pattern.Matches(subscription->requesting_origin))
      continue;
    if (secondary_pattern.IsValid() &&
        !secondary_pattern.Matches(subscription->embedding_origin))
      continue;

    ContentSetting new_value =
        PermissionContext::Get(profile_, subscription->permission)
            ->GetPermissionStatus(subscription->requesting_origin,
                                  subscription->embedding_origin);
    if (subscription->current_value == new_value)
      continue;

    subscription->current_value = new_value;

    // Add the callback to |callbacks| which will be run after the loop to
    // prevent re-entrance issues.
    callbacks.push_back(
        base::Bind(subscription->callback,
                   ContentSettingToPermissionStatus(new_value)));
  }

  for (const auto& callback : callbacks)
    callback.Run();
}
