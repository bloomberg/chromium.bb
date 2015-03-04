// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_service_impl.h"

#include <bitset>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/push_messaging_application_id.h"
#include "chrome/browser/push_messaging/push_messaging_constants.h"
#include "chrome/browser/push_messaging/push_messaging_permission_context.h"
#include "chrome/browser/push_messaging/push_messaging_permission_context_factory.h"
#include "chrome/browser/push_messaging/push_messaging_service_factory.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/permission_request_id.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/platform_notification_data.h"
#include "content/public/common/push_messaging_status.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace {
const int kMaxRegistrations = 1000000;

void RecordDeliveryStatus(content::PushDeliveryStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.DeliveryStatus",
                            status,
                            content::PUSH_DELIVERY_STATUS_LAST + 1);
}

void RecordUserVisibleStatus(content::PushUserVisibleStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.UserVisibleStatus",
                            status,
                            content::PUSH_USER_VISIBLE_STATUS_LAST + 1);
}

blink::WebPushPermissionStatus ToPushPermission(ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      return blink::WebPushPermissionStatusGranted;
    case CONTENT_SETTING_BLOCK:
      return blink::WebPushPermissionStatusDenied;
    case CONTENT_SETTING_ASK:
      return blink::WebPushPermissionStatusDefault;
    default:
      NOTREACHED();
      return blink::WebPushPermissionStatusDenied;
  }
}

}  // namespace

// static
void PushMessagingServiceImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kPushMessagingRegistrationCount,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  PushMessagingApplicationId::RegisterProfilePrefs(registry);
}

// static
void PushMessagingServiceImpl::InitializeForProfile(Profile* profile) {
  // TODO(johnme): Consider whether push should be enabled in incognito.
  if (!profile || profile->IsOffTheRecord())
    return;

  // TODO(johnme): If push becomes enabled in incognito (and this still uses a
  // pref), be careful that this pref is read from the right profile, as prefs
  // defined in a regular profile are visible in the corresponding incognito
  // profile unless overridden.
  // TODO(johnme): Make sure this pref doesn't get out of sync after crashes.
  int count = profile->GetPrefs()->GetInteger(
      prefs::kPushMessagingRegistrationCount);
  if (count <= 0)
    return;

  PushMessagingServiceImpl* push_service =
      PushMessagingServiceFactory::GetForProfile(profile);
  push_service->IncreasePushRegistrationCount(count, false /* is_pending */);
}

PushMessagingServiceImpl::PushMessagingServiceImpl(Profile* profile)
    : profile_(profile),
      push_registration_count_(0),
      pending_push_registration_count_(0),
      weak_factory_(this) {
  // In some tests, we might end up with |profile_| being null at this point.
  // When that is the case |profile_| will be set in SetProfileForTesting(), at
  // which point the service will start to observe HostContentSettingsMap.
  if (profile_)
    profile_->GetHostContentSettingsMap()->AddObserver(this);
}

PushMessagingServiceImpl::~PushMessagingServiceImpl() {
  profile_->GetHostContentSettingsMap()->RemoveObserver(this);
}

void PushMessagingServiceImpl::IncreasePushRegistrationCount(int add,
                                                             bool is_pending) {
  DCHECK(add > 0);
  if (push_registration_count_ + pending_push_registration_count_ == 0) {
    GetGCMDriver()->AddAppHandler(kPushMessagingApplicationIdPrefix, this);
  }
  if (is_pending) {
    pending_push_registration_count_ += add;
  } else {
    push_registration_count_ += add;
    profile_->GetPrefs()->SetInteger(prefs::kPushMessagingRegistrationCount,
                                     push_registration_count_);
  }
}

void PushMessagingServiceImpl::DecreasePushRegistrationCount(int subtract,
                                                             bool was_pending) {
  DCHECK(subtract > 0);
  if (was_pending) {
    pending_push_registration_count_ -= subtract;
    DCHECK(pending_push_registration_count_ >= 0);
  } else {
    push_registration_count_ -= subtract;
    DCHECK(push_registration_count_ >= 0);
    profile_->GetPrefs()->SetInteger(prefs::kPushMessagingRegistrationCount,
                                     push_registration_count_);
  }
  if (push_registration_count_ + pending_push_registration_count_ == 0) {
    GetGCMDriver()->RemoveAppHandler(kPushMessagingApplicationIdPrefix);
  }
}

bool PushMessagingServiceImpl::CanHandle(const std::string& app_id) const {
  return PushMessagingApplicationId::Get(profile_, app_id).IsValid();
}

void PushMessagingServiceImpl::ShutdownHandler() {
  // Shutdown() should come before and it removes us from the list of app
  // handlers of gcm::GCMDriver so this shouldn't ever been called.
  NOTREACHED();
}

// OnMessage methods -----------------------------------------------------------

void PushMessagingServiceImpl::OnMessage(
    const std::string& app_id,
    const gcm::GCMClient::IncomingMessage& message) {
  PushMessagingApplicationId application_id =
      PushMessagingApplicationId::Get(profile_, app_id);
  // Drop message and unregister if app id was unknown (maybe recently deleted).
  if (!application_id.IsValid()) {
    DeliverMessageCallback(app_id, GURL::EmptyGURL(), -1, message,
                           content::PUSH_DELIVERY_STATUS_UNKNOWN_APP_ID);
    return;
  }
  // Drop message and unregister if |origin| has lost push permission.
  if (!HasPermission(application_id.origin())) {
    DeliverMessageCallback(app_id, application_id.origin(),
                           application_id.service_worker_registration_id(),
                           message,
                           content::PUSH_DELIVERY_STATUS_PERMISSION_DENIED);
    return;
  }

  // The Push API only exposes a single string of data in the push event fired
  // on the Service Worker. When developers send messages using GCM to the Push
  // API and want to include a message payload, they must pass a single key-
  // value pair, where the key is "data" and the value is the string they want
  // to be passed to their Service Worker. For example, they could send the
  // following JSON using the HTTPS GCM API:
  // {
  //     "registration_ids": ["FOO", "BAR"],
  //     "data": {
  //         "data": "BAZ",
  //     },
  //     "delay_while_idle": true,
  // }
  // TODO(johnme): Make sure this is clearly documented for developers.
  std::string data;
  // TODO(peter): Message payloads are disabled pending mandatory encryption.
  // https://crbug.com/449184
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePushMessagePayload)) {
    gcm::GCMClient::MessageData::const_iterator it = message.data.find("data");
    if (it != message.data.end())
      data = it->second;
  }

  content::BrowserContext::DeliverPushMessage(
      profile_,
      application_id.origin(),
      application_id.service_worker_registration_id(),
      data,
      base::Bind(&PushMessagingServiceImpl::DeliverMessageCallback,
                 weak_factory_.GetWeakPtr(),
                 application_id.app_id_guid(), application_id.origin(),
                 application_id.service_worker_registration_id(), message));
}

void PushMessagingServiceImpl::DeliverMessageCallback(
    const std::string& app_id_guid,
    const GURL& requesting_origin,
    int64 service_worker_registration_id,
    const gcm::GCMClient::IncomingMessage& message,
    content::PushDeliveryStatus status) {
  // TODO(mvanouwerkerk): Show a warning in the developer console of the
  // Service Worker corresponding to app_id (and/or on an internals page).
  // TODO(mvanouwerkerk): Is there a way to recover from failure?
  switch (status) {
    // Call RequireUserVisibleUX if the message was delivered to the Service
    // Worker JS, even if the website's event handler failed (to prevent sites
    // deliberately failing in order to avoid having to show notifications).
    case content::PUSH_DELIVERY_STATUS_SUCCESS:
    case content::PUSH_DELIVERY_STATUS_EVENT_WAITUNTIL_REJECTED:
      RequireUserVisibleUX(requesting_origin, service_worker_registration_id);
      break;
    case content::PUSH_DELIVERY_STATUS_INVALID_MESSAGE:
    case content::PUSH_DELIVERY_STATUS_SERVICE_WORKER_ERROR:
      break;
    case content::PUSH_DELIVERY_STATUS_UNKNOWN_APP_ID:
    case content::PUSH_DELIVERY_STATUS_PERMISSION_DENIED:
    case content::PUSH_DELIVERY_STATUS_NO_SERVICE_WORKER:
      Unregister(app_id_guid, message.sender_id, true /* retry_on_failure */,
                 UnregisterCallback());
      break;
  }
  RecordDeliveryStatus(status);
}

void PushMessagingServiceImpl::RequireUserVisibleUX(
    const GURL& requesting_origin, int64 service_worker_registration_id) {
#if defined(ENABLE_NOTIFICATIONS)
  // TODO(johnme): Relax this heuristic slightly.
  PlatformNotificationServiceImpl* notification_service =
      PlatformNotificationServiceImpl::GetInstance();
  // Can't use g_browser_process->notification_ui_manager(), since the test uses
  // PlatformNotificationServiceImpl::SetNotificationUIManagerForTesting.
  // TODO(peter): Remove the need to use both APIs here once Notification.get()
  // is supported.
  int notification_count = notification_service->GetNotificationUIManager()->
      GetAllIdsByProfileAndSourceOrigin(profile_, requesting_origin).size();
  // TODO(johnme): Hiding an existing notification should also count as a useful
  // user-visible action done in response to a push message - but make sure that
  // sending two messages in rapid succession which show then hide a
  // notification doesn't count.
  bool notification_shown = notification_count > 0;

  bool notification_needed = true;
  // Sites with a currently visible tab don't need to show notifications.
#if defined(OS_ANDROID)
  for (auto it = TabModelList::begin(); it != TabModelList::end(); ++it) {
    Profile* profile = (*it)->GetProfile();
    content::WebContents* active_web_contents =
        (*it)->GetActiveWebContents();
#else
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    Profile* profile = it->profile();
    content::WebContents* active_web_contents =
        it->tab_strip_model()->GetActiveWebContents();
#endif
    if (!active_web_contents || !active_web_contents->GetMainFrame())
      continue;

    // Don't leak information from other profiles.
    if (profile != profile_)
      continue;

    // Ignore minimized windows etc.
    switch (active_web_contents->GetMainFrame()->GetVisibilityState()) {
     case blink::WebPageVisibilityStateHidden:
     case blink::WebPageVisibilityStatePrerender:
      continue;
     case blink::WebPageVisibilityStateVisible:
      break;
    }

    // Use the visible URL since that's the one the user is aware of (and it
    // doesn't matter whether the page loaded successfully).
    const GURL& active_url = active_web_contents->GetVisibleURL();
    if (requesting_origin == active_url.GetOrigin()) {
      notification_needed = false;
      break;
    }
#if defined(OS_ANDROID)
  }
#else
  }
#endif

  // Don't track push messages that didn't show a notification but were exempt
  // from needing to do so.
  if (notification_shown || notification_needed) {
    content::ServiceWorkerContext* service_worker_context =
        content::BrowserContext::GetStoragePartitionForSite(
            profile_, requesting_origin)->GetServiceWorkerContext();

    PushMessagingService::GetNotificationsShownByLastFewPushes(
        service_worker_context, service_worker_registration_id,
        base::Bind(&PushMessagingServiceImpl::DidGetNotificationsShown,
                   weak_factory_.GetWeakPtr(),
                   requesting_origin, service_worker_registration_id,
                   notification_shown, notification_needed));
  } else {
    RecordUserVisibleStatus(
        content::PUSH_USER_VISIBLE_STATUS_NOT_REQUIRED_AND_NOT_SHOWN);
  }
#endif  // defined(ENABLE_NOTIFICATIONS)
}

static void IgnoreResult(bool unused) {
}

void PushMessagingServiceImpl::DidGetNotificationsShown(
    const GURL& requesting_origin, int64 service_worker_registration_id,
    bool notification_shown, bool notification_needed,
    const std::string& data, bool success, bool not_found) {
  content::ServiceWorkerContext* service_worker_context =
      content::BrowserContext::GetStoragePartitionForSite(
          profile_, requesting_origin)->GetServiceWorkerContext();

  // We remember whether the last (up to) 10 pushes showed notifications.
  const size_t MISSED_NOTIFICATIONS_LENGTH = 10;
  // data is a string like "0001000", where '0' means shown, and '1' means
  // needed but not shown. We manipulate it in bitset form.
  std::bitset<MISSED_NOTIFICATIONS_LENGTH> missed_notifications(data);

  bool needed_but_not_shown = notification_needed && !notification_shown;

  // New entries go at the end, and old ones are shifted off the beginning once
  // the history length is exceeded.
  missed_notifications <<= 1;
  missed_notifications[0] = needed_but_not_shown;
  std::string updated_data(missed_notifications.
      to_string<char, std::string::traits_type, std::string::allocator_type>());
  PushMessagingService::SetNotificationsShownByLastFewPushes(
      service_worker_context, service_worker_registration_id,
      requesting_origin, updated_data,
      base::Bind(&IgnoreResult));  // This is a heuristic; ignore failure.

  if (notification_shown) {
    RecordUserVisibleStatus(
        notification_needed
        ? content::PUSH_USER_VISIBLE_STATUS_REQUIRED_AND_SHOWN
        : content::PUSH_USER_VISIBLE_STATUS_NOT_REQUIRED_BUT_SHOWN);
    return;
  }
  if (needed_but_not_shown) {
    if (missed_notifications.count() <= 1) {
      RecordUserVisibleStatus(
        content::PUSH_USER_VISIBLE_STATUS_REQUIRED_BUT_NOT_SHOWN_USED_GRACE);
      return;
    }
    RecordUserVisibleStatus(
        content::
            PUSH_USER_VISIBLE_STATUS_REQUIRED_BUT_NOT_SHOWN_GRACE_EXCEEDED);
    // The site failed to show a notification when one was needed, and they have
    // already failed once in the previous 10 push messages, so we will show a
    // generic notification. See https://crbug.com/437277.
    // TODO(johnme): The generic notification should probably automatically
    // close itself when the next push message arrives?
    content::PlatformNotificationData notification_data;
    // TODO(johnme): Switch to FormatOriginForDisplay from crbug.com/402698
    notification_data.title = base::UTF8ToUTF16(requesting_origin.host());
    notification_data.direction =
        content::PlatformNotificationData::NotificationDirectionLeftToRight;
    notification_data.body =
        l10n_util::GetStringUTF16(IDS_PUSH_MESSAGING_GENERIC_NOTIFICATION_BODY);
    notification_data.tag =
        base::ASCIIToUTF16(kPushMessagingForcedNotificationTag);
    notification_data.icon = GURL();  // TODO(johnme): Better icon?
    PlatformNotificationServiceImpl* notification_service =
        PlatformNotificationServiceImpl::GetInstance();
    notification_service->DisplayPersistentNotification(
        profile_,
        service_worker_registration_id,
        requesting_origin,
        SkBitmap() /* icon */,
        notification_data);
  }
}

// Other gcm::GCMAppHandler methods -------------------------------------------

void PushMessagingServiceImpl::OnMessagesDeleted(const std::string& app_id) {
  // TODO(mvanouwerkerk): Fire push error event on the Service Worker
  // corresponding to app_id.
}

void PushMessagingServiceImpl::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& send_error_details) {
  NOTREACHED() << "The Push API shouldn't have sent messages upstream";
}

void PushMessagingServiceImpl::OnSendAcknowledged(
    const std::string& app_id,
    const std::string& message_id) {
  NOTREACHED() << "The Push API shouldn't have sent messages upstream";
}

// GetPushEndpoint method ------------------------------------------------------

GURL PushMessagingServiceImpl::GetPushEndpoint() {
  return GURL(std::string(kPushMessagingEndpoint));
}

// Register and GetPermissionStatus methods ------------------------------------

void PushMessagingServiceImpl::RegisterFromDocument(
    const GURL& requesting_origin,
    int64 service_worker_registration_id,
    const std::string& sender_id,
    int renderer_id,
    int render_frame_id,
    bool user_visible_only,
    const content::PushMessagingService::RegisterCallback& callback) {
  PushMessagingApplicationId application_id =
      PushMessagingApplicationId::Generate(requesting_origin,
                                           service_worker_registration_id);
  DCHECK(application_id.IsValid());

  if (push_registration_count_ + pending_push_registration_count_
      >= kMaxRegistrations) {
    RegisterEnd(callback,
                std::string(),
                content::PUSH_REGISTRATION_STATUS_LIMIT_REACHED);
    return;
  }

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(renderer_id, render_frame_id);
  if (!render_frame_host)
    return;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;

  // TODO(miguelg) need to send this over IPC when bubble support is
  // implemented.
  int bridge_id = -1;

  const PermissionRequestID id(
      renderer_id, web_contents->GetRoutingID(), bridge_id, GURL());

  PushMessagingPermissionContext* permission_context =
      PushMessagingPermissionContextFactory::GetForProfile(profile_);

  if (permission_context == NULL || !user_visible_only) {
    RegisterEnd(callback,
                std::string(),
                content::PUSH_REGISTRATION_STATUS_PERMISSION_DENIED);
    return;
  }

  // TODO(miguelg): Consider the value of |user_visible_only| when making
  // the permission request.
  // TODO(mlamouri): Move requesting Push permission over to using Mojo, and
  // re-introduce the ability of |user_gesture| when bubbles require this.
  // https://crbug.com/423770.
  permission_context->RequestPermission(
      web_contents, id, requesting_origin, true /* user_gesture */,
      base::Bind(&PushMessagingServiceImpl::DidRequestPermission,
                 weak_factory_.GetWeakPtr(), application_id, sender_id,
                 callback));
}

void PushMessagingServiceImpl::RegisterFromWorker(
    const GURL& requesting_origin,
    int64 service_worker_registration_id,
    const std::string& sender_id,
    const content::PushMessagingService::RegisterCallback& register_callback) {
  PushMessagingApplicationId application_id =
      PushMessagingApplicationId::Generate(requesting_origin,
                                           service_worker_registration_id);
  DCHECK(application_id.IsValid());

  if (profile_->GetPrefs()->GetInteger(
          prefs::kPushMessagingRegistrationCount) >= kMaxRegistrations) {
    RegisterEnd(register_callback, std::string(),
                content::PUSH_REGISTRATION_STATUS_LIMIT_REACHED);
    return;
  }

  GURL embedding_origin = requesting_origin;
  blink::WebPushPermissionStatus permission_status =
      PushMessagingServiceImpl::GetPermissionStatus(requesting_origin,
                                                    embedding_origin);
  if (permission_status != blink::WebPushPermissionStatusGranted) {
    RegisterEnd(register_callback, std::string(),
                content::PUSH_REGISTRATION_STATUS_PERMISSION_DENIED);
    return;
  }

  IncreasePushRegistrationCount(1, true /* is_pending */);
  std::vector<std::string> sender_ids(1, sender_id);
  GetGCMDriver()->Register(
      application_id.app_id_guid(), sender_ids,
      base::Bind(&PushMessagingServiceImpl::DidRegister,
                 weak_factory_.GetWeakPtr(),
                 application_id, register_callback));
}

blink::WebPushPermissionStatus PushMessagingServiceImpl::GetPermissionStatus(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  PushMessagingPermissionContext* permission_context =
      PushMessagingPermissionContextFactory::GetForProfile(profile_);
  return ToPushPermission(permission_context->GetPermissionStatus(
      requesting_origin, embedding_origin));
}

void PushMessagingServiceImpl::RegisterEnd(
    const content::PushMessagingService::RegisterCallback& callback,
    const std::string& registration_id,
    content::PushRegistrationStatus status) {
  callback.Run(registration_id, status);
}

void PushMessagingServiceImpl::DidRegister(
    const PushMessagingApplicationId& application_id,
    const content::PushMessagingService::RegisterCallback& callback,
    const std::string& registration_id,
    gcm::GCMClient::Result result) {
  content::PushRegistrationStatus status =
      content::PUSH_REGISTRATION_STATUS_SERVICE_ERROR;
  switch (result) {
    case gcm::GCMClient::SUCCESS:
      status = content::PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE;
      application_id.PersistToDisk(profile_);
      IncreasePushRegistrationCount(1, false /* is_pending */);
      break;
    case gcm::GCMClient::INVALID_PARAMETER:
    case gcm::GCMClient::GCM_DISABLED:
    case gcm::GCMClient::ASYNC_OPERATION_PENDING:
    case gcm::GCMClient::SERVER_ERROR:
    case gcm::GCMClient::UNKNOWN_ERROR:
      status = content::PUSH_REGISTRATION_STATUS_SERVICE_ERROR;
      break;
    case gcm::GCMClient::NETWORK_ERROR:
    case gcm::GCMClient::TTL_EXCEEDED:
      status = content::PUSH_REGISTRATION_STATUS_NETWORK_ERROR;
      break;
  }
  RegisterEnd(callback, registration_id, status);
  DecreasePushRegistrationCount(1, true /* was_pending */);
}

void PushMessagingServiceImpl::DidRequestPermission(
    const PushMessagingApplicationId& application_id,
    const std::string& sender_id,
    const content::PushMessagingService::RegisterCallback& register_callback,
    ContentSetting content_setting) {
  if (content_setting != CONTENT_SETTING_ALLOW) {
    RegisterEnd(register_callback,
                std::string(),
                content::PUSH_REGISTRATION_STATUS_PERMISSION_DENIED);
    return;
  }

  IncreasePushRegistrationCount(1, true /* is_pending */);
  std::vector<std::string> sender_ids(1, sender_id);
  GetGCMDriver()->Register(
      application_id.app_id_guid(),
      sender_ids,
      base::Bind(&PushMessagingServiceImpl::DidRegister,
                 weak_factory_.GetWeakPtr(),
                 application_id, register_callback));
}

// Unregister methods ----------------------------------------------------------

void PushMessagingServiceImpl::Unregister(
    const GURL& requesting_origin,
    int64 service_worker_registration_id,
    const std::string& sender_id,
    bool retry_on_failure,
    const content::PushMessagingService::UnregisterCallback& callback) {
  PushMessagingApplicationId application_id = PushMessagingApplicationId::Get(
      profile_, requesting_origin, service_worker_registration_id);
  if (!application_id.IsValid()) {
    if (!callback.is_null()) {
      callback.Run(
          content::PUSH_UNREGISTRATION_STATUS_SUCCESS_WAS_NOT_REGISTERED);
    }
    return;
  }

  Unregister(application_id.app_id_guid(), sender_id, retry_on_failure,
             callback);
}

void PushMessagingServiceImpl::Unregister(
    const std::string& app_id_guid,
    const std::string& sender_id,
    bool retry_on_failure,
    const content::PushMessagingService::UnregisterCallback& callback) {
  if (retry_on_failure) {
    // Delete the mapping for this app id, to guarantee that no messages get
    // delivered in future (even if unregistration fails).
    // TODO(johnme): Instead of deleting these app ids, store them elsewhere,
    // and retry unregistration if it fails due to network errors.
    PushMessagingApplicationId application_id =
        PushMessagingApplicationId::Get(profile_, app_id_guid);
    if (application_id.IsValid())
      application_id.DeleteFromDisk(profile_);
  }

  const auto& unregister_callback =
      base::Bind(&PushMessagingServiceImpl::DidUnregister,
                 weak_factory_.GetWeakPtr(),
                 app_id_guid, retry_on_failure, callback);
#if defined(OS_ANDROID)
  // On Android the backend is different, and requires the original sender_id.
  GetGCMDriver()->UnregisterWithSenderId(app_id_guid, sender_id,
                                         unregister_callback);
#else
  GetGCMDriver()->Unregister(app_id_guid, unregister_callback);
#endif
}

void PushMessagingServiceImpl::DidUnregister(
    const std::string& app_id_guid,
    bool retry_on_failure,
    const content::PushMessagingService::UnregisterCallback& callback,
    gcm::GCMClient::Result result) {
  if (result == gcm::GCMClient::SUCCESS) {
    PushMessagingApplicationId application_id =
        PushMessagingApplicationId::Get(profile_, app_id_guid);
    if (!application_id.IsValid()) {
      if (!callback.is_null()) {
        callback.Run(
            content::PUSH_UNREGISTRATION_STATUS_SUCCESS_WAS_NOT_REGISTERED);
      }
      return;
    }

    application_id.DeleteFromDisk(profile_);
    DecreasePushRegistrationCount(1, false /* was_pending */);
  }

  // Internal calls pass a null callback.
  if (!callback.is_null()) {
    switch (result) {
      case gcm::GCMClient::SUCCESS:
        callback.Run(content::PUSH_UNREGISTRATION_STATUS_SUCCESS_UNREGISTERED);
        break;
      case gcm::GCMClient::INVALID_PARAMETER:
      case gcm::GCMClient::GCM_DISABLED:
      case gcm::GCMClient::ASYNC_OPERATION_PENDING:
      case gcm::GCMClient::SERVER_ERROR:
      case gcm::GCMClient::UNKNOWN_ERROR:
        callback.Run(content::PUSH_UNREGISTRATION_STATUS_SERVICE_ERROR);
        break;
      case gcm::GCMClient::NETWORK_ERROR:
      case gcm::GCMClient::TTL_EXCEEDED:
        callback.Run(
            retry_on_failure
            ? content::
              PUSH_UNREGISTRATION_STATUS_PENDING_WILL_RETRY_NETWORK_ERROR
            : content::PUSH_UNREGISTRATION_STATUS_NETWORK_ERROR);
        break;
    }
  }
}

// OnContentSettingChanged methods ---------------------------------------------

void PushMessagingServiceImpl::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    std::string resource_identifier) {
  if (content_type != CONTENT_SETTINGS_TYPE_PUSH_MESSAGING &&
      content_type != CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    return;
  }

  for (const auto& id : PushMessagingApplicationId::GetAll(profile_)) {
    // If |primary_pattern| is not valid, we should always check for a
    // permission change because it can happen for example when the entire
    // Push or Notifications permissions are cleared.
    // Otherwise, the permission should be checked if the pattern matches the
    // origin.
    if (primary_pattern.IsValid() && !primary_pattern.Matches(id.origin()))
      continue;

    if (HasPermission(id.origin()))
      continue;

    PushMessagingService::GetSenderId(
        profile_, id.origin(), id.service_worker_registration_id(),
        base::Bind(
            &PushMessagingServiceImpl::UnregisterBecausePermissionRevoked,
            weak_factory_.GetWeakPtr(), id));
  }
}

void PushMessagingServiceImpl::UnregisterBecausePermissionRevoked(
    const PushMessagingApplicationId& id,
    const std::string& sender_id, bool success, bool not_found) {
  // Unregister the PushMessagingApplicationId with the push service.
  Unregister(id.app_id_guid(), sender_id, true /* retry_on_failure */,
             UnregisterCallback());

  // Clear the associated service worker push registration id.
  PushMessagingService::ClearPushRegistrationID(
      profile_, id.origin(), id.service_worker_registration_id());
}

// KeyedService methods -------------------------------------------------------

void PushMessagingServiceImpl::Shutdown() {
  GetGCMDriver()->RemoveAppHandler(kPushMessagingApplicationIdPrefix);
}

// Helper methods --------------------------------------------------------------

bool PushMessagingServiceImpl::HasPermission(const GURL& origin) {
  PushMessagingPermissionContext* permission_context =
      PushMessagingPermissionContextFactory::GetForProfile(profile_);
  DCHECK(permission_context);

  return permission_context->GetPermissionStatus(origin, origin) ==
      CONTENT_SETTING_ALLOW;
}

gcm::GCMDriver* PushMessagingServiceImpl::GetGCMDriver() const {
  gcm::GCMProfileService* gcm_profile_service =
      gcm::GCMProfileServiceFactory::GetForProfile(profile_);
  CHECK(gcm_profile_service);
  CHECK(gcm_profile_service->driver());
  return gcm_profile_service->driver();
}
