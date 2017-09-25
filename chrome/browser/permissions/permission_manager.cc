// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_manager.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/accessibility_permission_context.h"
#include "chrome/browser/background_sync/background_sync_permission_context.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/generic_sensor/sensor_permission_context.h"
#include "chrome/browser/media/midi_permission_context.h"
#include "chrome/browser/media/midi_sysex_permission_context.h"
#include "chrome/browser/media/webrtc/media_stream_device_permission_context.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/storage/durable_storage_permission_context.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/features.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "device/vr/features/features.h"
#include "ppapi/features/features.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/flash_permission_context.h"
#endif

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#endif

#if defined(OS_ANDROID)
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
      return PermissionStatus::GRANTED;
    case CONTENT_SETTING_BLOCK:
      return PermissionStatus::DENIED;
    case CONTENT_SETTING_ASK:
      return PermissionStatus::ASK;
    case CONTENT_SETTING_SESSION_ONLY:
    case CONTENT_SETTING_DETECT_IMPORTANT_CONTENT:
    case CONTENT_SETTING_DEFAULT:
    case CONTENT_SETTING_NUM_SETTINGS:
      break;
  }

  NOTREACHED();
  return PermissionStatus::DENIED;
}

// Helper method to convert PermissionType to ContentSettingType.
ContentSettingsType PermissionTypeToContentSetting(PermissionType permission) {
  switch (permission) {
    case PermissionType::MIDI:
      return CONTENT_SETTINGS_TYPE_MIDI;
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
    case PermissionType::AUDIO_CAPTURE:
      return CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC;
    case PermissionType::VIDEO_CAPTURE:
      return CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA;
    case PermissionType::BACKGROUND_SYNC:
      return CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC;
    case PermissionType::FLASH:
      return CONTENT_SETTINGS_TYPE_PLUGINS;
    case PermissionType::SENSORS:
      return CONTENT_SETTINGS_TYPE_SENSORS;
    case PermissionType::ACCESSIBILITY_EVENTS:
      return CONTENT_SETTINGS_TYPE_ACCESSIBILITY_EVENTS;
    case PermissionType::NUM:
      // This will hit the NOTREACHED below.
      break;
  }

  NOTREACHED() << "Unknown content setting for permission "
               << static_cast<int>(permission);
  return CONTENT_SETTINGS_TYPE_DEFAULT;
}

void SubscriptionCallbackWrapper(
    const base::Callback<void(PermissionStatus)>& callback,
    ContentSetting content_setting) {
  callback.Run(ContentSettingToPermissionStatus(content_setting));
}

void PermissionStatusCallbackWrapper(
    const base::Callback<void(PermissionStatus)>& callback,
    const std::vector<ContentSetting>& vector) {
  DCHECK_EQ(1ul, vector.size());
  callback.Run(ContentSettingToPermissionStatus(vector[0]));
}

void PermissionStatusVectorCallbackWrapper(
    const base::Callback<void(const std::vector<PermissionStatus>&)>& callback,
    const std::vector<ContentSetting>& content_settings) {
  std::vector<PermissionStatus> permission_statuses;
  std::transform(content_settings.begin(), content_settings.end(),
                 back_inserter(permission_statuses),
                 ContentSettingToPermissionStatus);
  callback.Run(permission_statuses);
}

void ContentSettingCallbackWraper(
    const base::Callback<void(ContentSetting)>& callback,
    const std::vector<ContentSetting>& vector) {
  DCHECK_EQ(1ul, vector.size());
  callback.Run(vector[0]);
}

}  // anonymous namespace

class PermissionManager::PendingRequest {
 public:
  PendingRequest(
      content::RenderFrameHost* render_frame_host,
      const std::vector<ContentSettingsType>& permissions,
      const base::Callback<void(const std::vector<ContentSetting>&)>& callback)
      : render_process_id_(render_frame_host->GetProcess()->GetID()),
        render_frame_id_(render_frame_host->GetRoutingID()),
        callback_(callback),
        permissions_(permissions),
        results_(permissions.size(), CONTENT_SETTING_BLOCK),
        remaining_results_(permissions.size()) {}

  void SetContentSetting(int permission_id, ContentSetting content_setting) {
    DCHECK(!IsComplete());

    results_[permission_id] = content_setting;
    --remaining_results_;
  }

  bool IsComplete() const {
    return remaining_results_ == 0;
  }

  int render_process_id() const { return render_process_id_; }
  int render_frame_id() const { return render_frame_id_; }

  const base::Callback<void(const std::vector<ContentSetting>&)> callback()
      const {
    return callback_;
  }

  std::vector<ContentSettingsType> permissions() const {
    return permissions_;
  }

  std::vector<ContentSetting> results() const { return results_; }

 private:
  int render_process_id_;
  int render_frame_id_;
  const base::Callback<void(const std::vector<ContentSetting>&)> callback_;
  std::vector<ContentSettingsType> permissions_;
  std::vector<ContentSetting> results_;
  size_t remaining_results_;
};

// Object to track the callback passed to
// PermissionContextBase::RequestPermission. The callback passed in will never
// be run when a permission prompt has been ignored, but it's important that we
// know when a prompt is ignored to clean up |pending_requests_| correctly.
// If the callback is destroyed without being run, the destructor here will
// cancel the request to clean up.
class PermissionManager::PermissionResponseCallback {
 public:
  PermissionResponseCallback(
      const base::WeakPtr<PermissionManager>& permission_manager,
      int request_id,
      int permission_id)
      : permission_manager_(permission_manager),
        request_id_(request_id),
        permission_id_(permission_id),
        request_answered_(false) {}

  ~PermissionResponseCallback() {
    if (!request_answered_ &&
        permission_manager_->pending_requests_.Lookup(request_id_)) {
      permission_manager_->pending_requests_.Remove(request_id_);
    }
  }

  void OnPermissionsRequestResponseStatus(ContentSetting content_setting) {
    request_answered_ = true;
    permission_manager_->OnPermissionsRequestResponseStatus(
        request_id_, permission_id_, content_setting);
  }

 private:
  base::WeakPtr<PermissionManager> permission_manager_;
  int request_id_;
  int permission_id_;

  bool request_answered_;

  DISALLOW_COPY_AND_ASSIGN(PermissionResponseCallback);
};

struct PermissionManager::Subscription {
  ContentSettingsType permission;
  GURL requesting_origin;
  GURL embedding_origin;
  base::Callback<void(ContentSetting)> callback;
  ContentSetting current_value;
};

// static
PermissionManager* PermissionManager::Get(Profile* profile) {
  return PermissionManagerFactory::GetForProfile(profile);
}

PermissionManager::PermissionManager(Profile* profile)
    : profile_(profile),
      weak_ptr_factory_(this) {
  permission_contexts_[CONTENT_SETTINGS_TYPE_MIDI_SYSEX] =
      base::MakeUnique<MidiSysexPermissionContext>(profile);
  permission_contexts_[CONTENT_SETTINGS_TYPE_MIDI] =
      base::MakeUnique<MidiPermissionContext>(profile);
  permission_contexts_[CONTENT_SETTINGS_TYPE_PUSH_MESSAGING] =
      base::MakeUnique<NotificationPermissionContext>(
          profile, CONTENT_SETTINGS_TYPE_PUSH_MESSAGING);
  permission_contexts_[CONTENT_SETTINGS_TYPE_NOTIFICATIONS] =
      base::MakeUnique<NotificationPermissionContext>(
          profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
#if !defined(OS_ANDROID)
  permission_contexts_[CONTENT_SETTINGS_TYPE_GEOLOCATION] =
      base::MakeUnique<GeolocationPermissionContext>(profile);
#else
  permission_contexts_[CONTENT_SETTINGS_TYPE_GEOLOCATION] =
      base::MakeUnique<GeolocationPermissionContextAndroid>(profile);
#endif
#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  permission_contexts_[CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER] =
      base::MakeUnique<ProtectedMediaIdentifierPermissionContext>(profile);
#endif
  permission_contexts_[CONTENT_SETTINGS_TYPE_DURABLE_STORAGE] =
      base::MakeUnique<DurableStoragePermissionContext>(profile);
  permission_contexts_[CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC] =
      base::MakeUnique<MediaStreamDevicePermissionContext>(
          profile, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
  permission_contexts_[CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA] =
      base::MakeUnique<MediaStreamDevicePermissionContext>(
          profile, CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
  permission_contexts_[CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC] =
      base::MakeUnique<BackgroundSyncPermissionContext>(profile);
#if BUILDFLAG(ENABLE_PLUGINS)
  permission_contexts_[CONTENT_SETTINGS_TYPE_PLUGINS] =
      base::MakeUnique<FlashPermissionContext>(profile);
#endif
  permission_contexts_[CONTENT_SETTINGS_TYPE_SENSORS] =
      base::MakeUnique<SensorPermissionContext>(profile);
  permission_contexts_[CONTENT_SETTINGS_TYPE_ACCESSIBILITY_EVENTS] =
      base::MakeUnique<AccessibilityPermissionContext>(profile);
}

PermissionManager::~PermissionManager() {
  DCHECK(pending_requests_.IsEmpty());
  DCHECK(subscriptions_.IsEmpty());
}

void PermissionManager::Shutdown() {
  if (!subscriptions_.IsEmpty()) {
    HostContentSettingsMapFactory::GetForProfile(profile_)
        ->RemoveObserver(this);
    subscriptions_.Clear();
  }
}

GURL PermissionManager::GetCanonicalOrigin(const GURL& url) const {
  if (url.GetOrigin() == GURL(chrome::kChromeSearchLocalNtpUrl).GetOrigin())
    return GURL(UIThreadSearchTermsData(profile_).GoogleBaseURLValue());

  return url;
}

int PermissionManager::RequestPermission(
    ContentSettingsType content_settings_type,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<void(ContentSetting)>& callback) {
  return RequestPermissions(
      std::vector<ContentSettingsType>(1, content_settings_type),
      render_frame_host, requesting_origin, user_gesture,
      base::Bind(&ContentSettingCallbackWraper, callback));
}

int PermissionManager::RequestPermissions(
    const std::vector<ContentSettingsType>& permissions,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<void(const std::vector<ContentSetting>&)>& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (permissions.empty()) {
    callback.Run(std::vector<ContentSetting>());
    return kNoPendingOperation;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);

  if (vr::VrTabHelper::IsInVr(web_contents)) {
    vr::VrTabHelper::UISuppressed(vr::UiSuppressedElement::kPermissionRequest);
    callback.Run(
        std::vector<ContentSetting>(permissions.size(), CONTENT_SETTING_BLOCK));
    return kNoPendingOperation;
  }

  GURL embedding_origin = web_contents->GetLastCommittedURL().GetOrigin();
  GURL canonical_requesting_origin = GetCanonicalOrigin(requesting_origin);

  int request_id = pending_requests_.Add(base::MakeUnique<PendingRequest>(
      render_frame_host, permissions, callback));

  const PermissionRequestID request(render_frame_host, request_id);

  for (size_t i = 0; i < permissions.size(); ++i) {
    const ContentSettingsType permission = permissions[i];

    PermissionContextBase* context = GetPermissionContext(permission);
    DCHECK(context);
    auto callback = base::MakeUnique<PermissionResponseCallback>(
        weak_ptr_factory_.GetWeakPtr(), request_id, i);
    context->RequestPermission(
        web_contents, request, canonical_requesting_origin, user_gesture,
        base::Bind(
            &PermissionResponseCallback::OnPermissionsRequestResponseStatus,
            base::Passed(&callback)));
  }

  // The request might have been resolved already.
  if (!pending_requests_.Lookup(request_id))
    return kNoPendingOperation;

  return request_id;
}

PermissionResult PermissionManager::GetPermissionStatus(
    ContentSettingsType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  return GetPermissionStatusHelper(permission, nullptr /* render_frame_host */,
                                   requesting_origin, embedding_origin);
}

PermissionResult PermissionManager::GetPermissionStatusForFrame(
    ContentSettingsType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  GURL embedding_origin = web_contents->GetLastCommittedURL().GetOrigin();
  return GetPermissionStatusHelper(permission, render_frame_host,
                                   requesting_origin, embedding_origin);
}

int PermissionManager::RequestPermission(
    PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<void(PermissionStatus)>& callback) {
  ContentSettingsType content_settings_type =
      PermissionTypeToContentSetting(permission);
  return RequestPermissions(
      std::vector<ContentSettingsType>(1, content_settings_type),
      render_frame_host, requesting_origin, user_gesture,
      base::Bind(&PermissionStatusCallbackWrapper, callback));
}

int PermissionManager::RequestPermissions(
    const std::vector<PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<void(const std::vector<PermissionStatus>&)>&
        callback) {
  std::vector<ContentSettingsType> content_settings_types;
  std::transform(permissions.begin(), permissions.end(),
                 back_inserter(content_settings_types),
                 PermissionTypeToContentSetting);
  return RequestPermissions(
      content_settings_types, render_frame_host, requesting_origin,
      user_gesture,
      base::Bind(&PermissionStatusVectorCallbackWrapper, callback));
}

PermissionContextBase* PermissionManager::GetPermissionContext(
    ContentSettingsType type) {
  const auto& it = permission_contexts_.find(type);
  return it == permission_contexts_.end() ? nullptr : it->second.get();
}

void PermissionManager::OnPermissionsRequestResponseStatus(
    int request_id,
    int permission_id,
    ContentSetting content_setting) {
  PendingRequest* pending_request = pending_requests_.Lookup(request_id);
  if (!pending_request)
    return;

  pending_request->SetContentSetting(permission_id, content_setting);

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
  for (ContentSettingsType permission : pending_request->permissions()) {
    PermissionContextBase* context = GetPermissionContext(permission);
    if (!context)
      continue;
    context->CancelPermissionRequest(web_contents, request);
  }

  // The request should be automatically removed from |pending_requests_| as a
  // result of it being cancelled but not necessarily immediately.
  // TODO(timloh): It would be nice to DCHECK that the request is removed, but
  // currently the PermissionUpdateInfobar (and maybe other places) does this
  // asynchronously.
}

void PermissionManager::ResetPermission(PermissionType permission,
                                        const GURL& requesting_origin,
                                        const GURL& embedding_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PermissionContextBase* context =
      GetPermissionContext(PermissionTypeToContentSetting(permission));
  if (!context)
    return;
  context->ResetPermission(GetCanonicalOrigin(requesting_origin).GetOrigin(),
                           embedding_origin.GetOrigin());
}

PermissionStatus PermissionManager::GetPermissionStatus(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PermissionResult result =
      GetPermissionStatus(PermissionTypeToContentSetting(permission),
                          requesting_origin, embedding_origin);

  // TODO(benwells): split this into two functions, GetPermissionStatus and
  // GetPermissionStatusForPermissionsAPI.
  PermissionContextBase* context =
      GetPermissionContext(PermissionTypeToContentSetting(permission));
  if (context) {
    result = context->UpdatePermissionStatusWithDeviceStatus(
        result, GetCanonicalOrigin(requesting_origin), embedding_origin);
  }

  return ContentSettingToPermissionStatus(result.content_setting);
}

int PermissionManager::SubscribePermissionStatusChange(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const base::Callback<void(PermissionStatus)>& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (subscriptions_.IsEmpty())
    HostContentSettingsMapFactory::GetForProfile(profile_)->AddObserver(this);

  ContentSettingsType content_type = PermissionTypeToContentSetting(permission);
  auto subscription = base::MakeUnique<Subscription>();
  subscription->permission = content_type;
  subscription->requesting_origin = GetCanonicalOrigin(requesting_origin);
  subscription->embedding_origin = embedding_origin;
  subscription->callback = base::Bind(&SubscriptionCallbackWrapper, callback);

  subscription->current_value =
      GetPermissionStatus(content_type, requesting_origin, embedding_origin)
          .content_setting;

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

bool PermissionManager::IsPermissionKillSwitchOn(
    ContentSettingsType permission) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return GetPermissionContext(permission)->IsPermissionKillSwitchOn();
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
    if (subscription->permission != content_type)
      continue;

    if (primary_pattern.IsValid() &&
        !primary_pattern.Matches(subscription->requesting_origin))
      continue;
    if (secondary_pattern.IsValid() &&
        !secondary_pattern.Matches(subscription->embedding_origin))
      continue;

    ContentSetting new_value =
        GetPermissionStatus(subscription->permission,
                            subscription->requesting_origin,
                            subscription->embedding_origin)
            .content_setting;
    if (subscription->current_value == new_value)
      continue;

    subscription->current_value = new_value;

    // Add the callback to |callbacks| which will be run after the loop to
    // prevent re-entrance issues.
    callbacks.push_back(base::Bind(subscription->callback, new_value));
  }

  for (const auto& callback : callbacks)
    callback.Run();
}

PermissionResult PermissionManager::GetPermissionStatusHelper(
    ContentSettingsType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  GURL canonical_requesting_origin = GetCanonicalOrigin(requesting_origin);
  PermissionContextBase* context = GetPermissionContext(permission);
  PermissionResult result = context->GetPermissionStatus(
      render_frame_host, canonical_requesting_origin.GetOrigin(),
      embedding_origin.GetOrigin());
  DCHECK(result.content_setting == CONTENT_SETTING_ALLOW ||
         result.content_setting == CONTENT_SETTING_ASK ||
         result.content_setting == CONTENT_SETTING_BLOCK);
  return result;
}
