// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_manager.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/background_sync/background_sync_permission_context.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/midi_permission_context.h"
#include "chrome/browser/media/webrtc/media_stream_device_permission_context.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/storage/durable_storage_permission_context.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "ppapi/features/features.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/flash_permission_context.h"
#endif

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#endif

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/geolocation/geolocation_permission_context_android.h"
#else
#include "chrome/browser/geolocation/geolocation_permission_context.h"
#endif

using blink::mojom::PermissionStatus;
using content::PermissionType;

namespace {

// Helper method to convert ContentSetting to PermissionStatus.
PermissionStatus ContentSettingToPermissionStatus(ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
    case CONTENT_SETTING_SESSION_ONLY:
      return PermissionStatus::GRANTED;
    case CONTENT_SETTING_BLOCK:
      return PermissionStatus::DENIED;
    case CONTENT_SETTING_ASK:
      return PermissionStatus::ASK;
    case CONTENT_SETTING_DETECT_IMPORTANT_CONTENT:
    case CONTENT_SETTING_DEFAULT:
    case CONTENT_SETTING_NUM_SETTINGS:
      break;
  }

  NOTREACHED();
  return PermissionStatus::DENIED;
}

// Wrap a callback taking a PermissionStatus to pass it as a callback taking a
// ContentSetting.
void ContentSettingToPermissionStatusCallbackWrapper(
    const base::Callback<void(PermissionStatus)>& callback,
    ContentSetting setting) {
  callback.Run(ContentSettingToPermissionStatus(setting));
}

// Helper method to convert PermissionType to ContentSettingType.
ContentSettingsType PermissionTypeToContentSetting(PermissionType permission) {
  switch (permission) {
    case PermissionType::MIDI_SYSEX:
      return CONTENT_SETTINGS_TYPE_MIDI_SYSEX;
    case PermissionType::PUSH_MESSAGING:
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
    case PermissionType::BACKGROUND_SYNC:
      return CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC;
    case PermissionType::FLASH:
      return CONTENT_SETTINGS_TYPE_PLUGINS;
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
// return nullptr in PermissionManager::GetPermissionContext since they don't
// have a context.
bool IsConstantPermission(PermissionType type) {
  switch (type) {
    case PermissionType::MIDI:
      return true;
    default:
      return false;
  }
}

void PermissionRequestResponseCallbackWrapper(
    const base::Callback<void(PermissionStatus)>& callback,
    const std::vector<PermissionStatus>& vector) {
  DCHECK_EQ(vector.size(), 1ul);
  callback.Run(vector[0]);
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

class PermissionManager::PendingRequest {
 public:
  PendingRequest(content::RenderFrameHost* render_frame_host,
                 const std::vector<PermissionType> permissions,
                 const base::Callback<
                     void(const std::vector<PermissionStatus>&)>& callback)
      : render_process_id_(render_frame_host->GetProcess()->GetID()),
        render_frame_id_(render_frame_host->GetRoutingID()),
        callback_(callback),
        permissions_(permissions),
        results_(permissions.size(), PermissionStatus::DENIED),
        remaining_results_(permissions.size()) {}

  void SetPermissionStatus(int permission_id, PermissionStatus status) {
    DCHECK(!IsComplete());

    results_[permission_id] = status;
    --remaining_results_;
  }

  bool IsComplete() const {
    return remaining_results_ == 0;
  }

  int render_process_id() const { return render_process_id_; }
  int render_frame_id() const { return render_frame_id_; }

  const base::Callback<void(const std::vector<PermissionStatus>&)>
  callback() const {
    return callback_;
  }

  std::vector<PermissionType> permissions() const {
    return permissions_;
  }

  std::vector<PermissionStatus> results() const {
    return results_;
  }

 private:
  int render_process_id_;
  int render_frame_id_;
  const base::Callback<void(const std::vector<PermissionStatus>&)> callback_;
  std::vector<PermissionType> permissions_;
  std::vector<PermissionStatus> results_;
  size_t remaining_results_;
};

struct PermissionManager::Subscription {
  PermissionType permission;
  GURL requesting_origin;
  GURL embedding_origin;
  base::Callback<void(PermissionStatus)> callback;
  ContentSetting current_value;
};

// static
PermissionManager* PermissionManager::Get(Profile* profile) {
  return PermissionManagerFactory::GetForProfile(profile);
}

PermissionManager::PermissionManager(Profile* profile)
    : profile_(profile),
      weak_ptr_factory_(this) {
  permission_contexts_[PermissionType::MIDI_SYSEX] =
      base::MakeUnique<MidiPermissionContext>(profile);
  permission_contexts_[PermissionType::PUSH_MESSAGING] =
      base::MakeUnique<NotificationPermissionContext>(
          profile, PermissionType::PUSH_MESSAGING);
  permission_contexts_[PermissionType::NOTIFICATIONS] =
      base::MakeUnique<NotificationPermissionContext>(
          profile, PermissionType::NOTIFICATIONS);
#if !BUILDFLAG(ANDROID_JAVA_UI)
  permission_contexts_[PermissionType::GEOLOCATION] =
      base::MakeUnique<GeolocationPermissionContext>(profile);
#else
  permission_contexts_[PermissionType::GEOLOCATION] =
      base::MakeUnique<GeolocationPermissionContextAndroid>(profile);
#endif
#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  permission_contexts_[PermissionType::PROTECTED_MEDIA_IDENTIFIER] =
      base::MakeUnique<ProtectedMediaIdentifierPermissionContext>(profile);
#endif
  permission_contexts_[PermissionType::DURABLE_STORAGE] =
      base::MakeUnique<DurableStoragePermissionContext>(profile);
  permission_contexts_[PermissionType::AUDIO_CAPTURE] =
      base::MakeUnique<MediaStreamDevicePermissionContext>(
          profile, content::PermissionType::AUDIO_CAPTURE,
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
  permission_contexts_[PermissionType::VIDEO_CAPTURE] =
      base::MakeUnique<MediaStreamDevicePermissionContext>(
          profile, content::PermissionType::VIDEO_CAPTURE,
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
  permission_contexts_[PermissionType::BACKGROUND_SYNC] =
      base::MakeUnique<BackgroundSyncPermissionContext>(profile);
#if BUILDFLAG(ENABLE_PLUGINS)
  permission_contexts_[PermissionType::FLASH] =
      base::MakeUnique<FlashPermissionContext>(profile);
#endif
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
  return RequestPermissions(
      std::vector<PermissionType>(1, permission),
      render_frame_host,
      requesting_origin,
      user_gesture,
      base::Bind(&PermissionRequestResponseCallbackWrapper, callback));
}

int PermissionManager::RequestPermissions(
    const std::vector<PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<void(
        const std::vector<PermissionStatus>&)>& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (permissions.empty()) {
    callback.Run(std::vector<PermissionStatus>());
    return kNoPendingOperation;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  GURL embedding_origin = web_contents->GetLastCommittedURL().GetOrigin();

  int request_id = pending_requests_.Add(base::MakeUnique<PendingRequest>(
      render_frame_host, permissions, callback));

  const PermissionRequestID request(render_frame_host, request_id);

  for (size_t i = 0; i < permissions.size(); ++i) {
    const PermissionType permission = permissions[i];

    if (IsConstantPermission(permission) ||
        !GetPermissionContext(permission)) {
      // Track permission request usages even for constant permissions.
      PermissionUmaUtil::PermissionRequested(permission, requesting_origin,
                                             embedding_origin, profile_);
      OnPermissionsRequestResponseStatus(request_id, i,
          GetPermissionStatus(permission, requesting_origin, embedding_origin));
      continue;
    }

    PermissionContextBase* context = GetPermissionContext(permission);
    context->RequestPermission(
        web_contents, request, requesting_origin, user_gesture,
        base::Bind(&ContentSettingToPermissionStatusCallbackWrapper,
            base::Bind(&PermissionManager::OnPermissionsRequestResponseStatus,
                weak_ptr_factory_.GetWeakPtr(), request_id, i)));
  }

  // The request might have been resolved already.
  if (!pending_requests_.Lookup(request_id))
    return kNoPendingOperation;

  return request_id;
}

PermissionContextBase* PermissionManager::GetPermissionContext(
    PermissionType type) {
  const auto& it = permission_contexts_.find(type);
  return it == permission_contexts_.end() ? nullptr : it->second.get();
}

void PermissionManager::OnPermissionsRequestResponseStatus(
    int request_id,
    int permission_id,
    PermissionStatus status) {
  PendingRequest* pending_request = pending_requests_.Lookup(request_id);
  pending_request->SetPermissionStatus(permission_id, status);

  if (!pending_request->IsComplete())
    return;

  pending_request->callback().Run(pending_request->results());
  pending_requests_.Remove(request_id);
}

void PermissionManager::CancelPermissionRequest(int request_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PendingRequest* pending_request = pending_requests_.Lookup(request_id);
  if (!pending_request)
    return;

  content::WebContents* web_contents = tab_util::GetWebContentsByFrameID(
      pending_request->render_process_id(), pending_request->render_frame_id());
  DCHECK(web_contents);

  const PermissionRequestID request(pending_request->render_process_id(),
                                    pending_request->render_frame_id(),
                                    request_id);
  for (PermissionType permission : pending_request->permissions()) {
    PermissionContextBase* context = GetPermissionContext(permission);
    if (!context)
      continue;
    context->CancelPermissionRequest(web_contents, request);
  }
  pending_requests_.Remove(request_id);
}

void PermissionManager::ResetPermission(PermissionType permission,
                                        const GURL& requesting_origin,
                                        const GURL& embedding_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PermissionContextBase* context = GetPermissionContext(permission);
  if (!context)
    return;

  context->ResetPermission(requesting_origin.GetOrigin(),
                           embedding_origin.GetOrigin());
}

PermissionStatus PermissionManager::GetPermissionStatus(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (IsConstantPermission(permission))
    return GetPermissionStatusForConstantPermission(permission);

  PermissionContextBase* context = GetPermissionContext(permission);
  if (!context)
    return PermissionStatus::DENIED;

  return ContentSettingToPermissionStatus(context->GetPermissionStatus(
      requesting_origin.GetOrigin(), embedding_origin.GetOrigin()));
}

void PermissionManager::RegisterPermissionUsage(PermissionType permission,
                                                const GURL& requesting_origin,
                                                const GURL& embedding_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (subscriptions_.IsEmpty())
    HostContentSettingsMapFactory::GetForProfile(profile_)->AddObserver(this);

  auto subscription = base::MakeUnique<Subscription>();
  subscription->permission = permission;
  subscription->requesting_origin = requesting_origin;
  subscription->embedding_origin = embedding_origin;
  subscription->callback = callback;

  if (IsConstantPermission(permission)) {
    subscription->current_value = GetContentSettingForConstantPermission(
        permission);
  } else {
    subscription->current_value =
        GetPermissionContext(permission)
            ->GetPermissionStatus(subscription->requesting_origin,
                                  subscription->embedding_origin);
  }

  return subscriptions_.Add(std::move(subscription));
}

void PermissionManager::UnsubscribePermissionStatusChange(int subscription_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Whether |subscription_id| is known will be checked by the Remove() call.
  subscriptions_.Remove(subscription_id);

  if (subscriptions_.IsEmpty())
    HostContentSettingsMapFactory::GetForProfile(profile_)
        ->RemoveObserver(this);
}

void PermissionManager::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    std::string resource_identifier) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::list<base::Closure> callbacks;

  for (SubscriptionsMap::iterator iter(&subscriptions_);
       !iter.IsAtEnd(); iter.Advance()) {
    Subscription* subscription = iter.GetCurrentValue();
    if (IsConstantPermission(subscription->permission))
      continue;
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
        GetPermissionContext(subscription->permission)
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
