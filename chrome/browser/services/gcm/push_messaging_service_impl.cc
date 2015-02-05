// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/push_messaging_service_impl.h"

#include <bitset>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/services/gcm/push_messaging_application_id.h"
#include "chrome/browser/services/gcm/push_messaging_constants.h"
#include "chrome/browser/services/gcm/push_messaging_permission_context.h"
#include "chrome/browser/services/gcm/push_messaging_permission_context_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/permission_request_id.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/platform_notification_data.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
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

namespace gcm {

namespace {
const int kMaxRegistrations = 1000000;

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
}

// static
void PushMessagingServiceImpl::InitializeForProfile(Profile* profile) {
  // TODO(mvanouwerkerk): Make sure to remove this check at the same time as
  // push graduates from experimental in Blink.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    return;
  }

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

  // Create the GCMProfileService, and hence instantiate this class.
  GCMProfileService* gcm_service =
      GCMProfileServiceFactory::GetForProfile(profile);
  PushMessagingServiceImpl* push_service =
      static_cast<PushMessagingServiceImpl*>(
          gcm_service->push_messaging_service());

  push_service->IncreasePushRegistrationCount(count);
}

PushMessagingServiceImpl::PushMessagingServiceImpl(
    GCMProfileService* gcm_profile_service,
    Profile* profile)
    : gcm_profile_service_(gcm_profile_service),
      profile_(profile),
      push_registration_count_(0),
      weak_factory_(this) {
}

PushMessagingServiceImpl::~PushMessagingServiceImpl() {
  // TODO(johnme): If it's possible for this to be destroyed before GCMDriver,
  // then we should call RemoveAppHandler.
}

void PushMessagingServiceImpl::IncreasePushRegistrationCount(int add) {
  DCHECK(add > 0);
  if (push_registration_count_ == 0) {
    gcm_profile_service_->driver()->AddAppHandler(
        kPushMessagingApplicationIdPrefix, this);
  }
  push_registration_count_ += add;
  profile_->GetPrefs()->SetInteger(prefs::kPushMessagingRegistrationCount,
                                   push_registration_count_);
}

void PushMessagingServiceImpl::DecreasePushRegistrationCount(int subtract) {
  DCHECK(subtract > 0);
  push_registration_count_ -= subtract;
  DCHECK(push_registration_count_ >= 0);
  profile_->GetPrefs()->SetInteger(prefs::kPushMessagingRegistrationCount,
                                   push_registration_count_);
  if (push_registration_count_ == 0) {
    gcm_profile_service_->driver()->RemoveAppHandler(
        kPushMessagingApplicationIdPrefix);
  }
}

bool PushMessagingServiceImpl::CanHandle(const std::string& app_id) const {
  return PushMessagingApplicationId::Parse(app_id).IsValid();
}

void PushMessagingServiceImpl::ShutdownHandler() {
  // TODO(johnme): Do any necessary cleanup.
}

void PushMessagingServiceImpl::OnMessage(
    const std::string& app_id,
    const GCMClient::IncomingMessage& message) {
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
  PushMessagingApplicationId application_id =
      PushMessagingApplicationId::Parse(app_id);

  // Drop messages whose application is is invalid.
  if (!application_id.IsValid()) {
    DeliverMessageCallback(application_id, message,
                           content::PUSH_DELIVERY_STATUS_INVALID_MESSAGE);
    return;
  }

  // |origin| may have lost push permission. Unregister and drop this message.
  if (!HasPermission(application_id.origin)) {
    Unregister(application_id, UnregisterCallback());
    return;
  }

  std::string data;

  // TODO(peter): Message payloads are disabled pending mandatory encryption.
  // https://crbug.com/449184
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePushMessagePayload)) {
    GCMClient::MessageData::const_iterator it = message.data.find("data");
    if (it != message.data.end())
      data = it->second;
  }

  content::BrowserContext::DeliverPushMessage(
      profile_,
      application_id.origin,
      application_id.service_worker_registration_id,
      data,
      base::Bind(&PushMessagingServiceImpl::DeliverMessageCallback,
                 weak_factory_.GetWeakPtr(),
                 application_id,
                 message));
}

void PushMessagingServiceImpl::SetProfileForTesting(Profile* profile) {
  profile_ = profile;
}

void PushMessagingServiceImpl::DeliverMessageCallback(
    const PushMessagingApplicationId& application_id,
    const GCMClient::IncomingMessage& message,
    content::PushDeliveryStatus status) {
  // TODO(mvanouwerkerk): UMA logging.
  // TODO(mvanouwerkerk): Show a warning in the developer console of the
  // Service Worker corresponding to app_id (and/or on an internals page).
  // TODO(mvanouwerkerk): Is there a way to recover from failure?
  switch (status) {
    // Call RequireUserVisibleUX if the message was delivered to the Service
    // Worker JS, even if the website's event handler failed (to prevent sites
    // deliberately failing in order to avoid having to show notifications).
    case content::PUSH_DELIVERY_STATUS_SUCCESS:
    case content::PUSH_DELIVERY_STATUS_EVENT_WAITUNTIL_REJECTED:
      RequireUserVisibleUX(application_id);
      break;
    case content::PUSH_DELIVERY_STATUS_INVALID_MESSAGE:
    case content::PUSH_DELIVERY_STATUS_SERVICE_WORKER_ERROR:
      break;
    case content::PUSH_DELIVERY_STATUS_NO_SERVICE_WORKER:
      Unregister(application_id, UnregisterCallback());
      break;
  }
}

void PushMessagingServiceImpl::RequireUserVisibleUX(
    const PushMessagingApplicationId& application_id) {
#if defined(ENABLE_NOTIFICATIONS)
  // TODO(johnme): Relax this heuristic slightly.
  PlatformNotificationServiceImpl* notification_service =
      PlatformNotificationServiceImpl::GetInstance();
  // Can't use g_browser_process->notification_ui_manager(), since the test uses
  // PlatformNotificationServiceImpl::SetNotificationUIManagerForTesting.
  // TODO(peter): Remove the need to use both APIs here once Notification.get()
  // is supported.
  int notification_count = notification_service->GetNotificationUIManager()->
      GetAllIdsByProfileAndSourceOrigin(profile_, application_id.origin).size();
  // TODO(johnme): Hiding an existing notification should also count as a useful
  // user-visible action done in response to a push message - but make sure that
  // sending two messages in rapid succession which show then hide a
  // notification doesn't count.
  bool notification_shown = notification_count > 0;

  bool notification_needed = true;
  if (!notification_shown) {
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
      if (!active_web_contents)
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

      // Allow https://foo.example.com Service Worker to not show notification
      // if an https://bar.example.com tab is visible (and hence might
      // conceivably be showing UI in response to the push message); but http://
      // doesn't count as the Service Worker can't talk to it, even with
      // navigator.connect.
      if (application_id.origin.scheme() != active_url.scheme())
        continue;
      if (net::registry_controlled_domains::SameDomainOrHost(
          application_id.origin, active_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
        notification_needed = false;
        break;
      }
    }
  }

  // Don't track push messages that didn't show a notification but were exempt
  // from needing to do so.
  if (notification_shown || notification_needed) {
    content::ServiceWorkerContext* service_worker_context =
        content::BrowserContext::GetStoragePartitionForSite(
            profile_, application_id.origin)->GetServiceWorkerContext();

    PushMessagingService::GetNotificationsShownByLastFewPushes(
        service_worker_context, application_id.service_worker_registration_id,
        base::Bind(&PushMessagingServiceImpl::DidGetNotificationsShown,
                   weak_factory_.GetWeakPtr(),
                   application_id, notification_shown, notification_needed));
  }
#endif  // defined(ENABLE_NOTIFICATIONS)
}

static void IgnoreResult(bool unused) {
}

void PushMessagingServiceImpl::DidGetNotificationsShown(
    const PushMessagingApplicationId& application_id,
    bool notification_shown, bool notification_needed,
    const std::string& data, bool success, bool not_found) {
  content::ServiceWorkerContext* service_worker_context =
      content::BrowserContext::GetStoragePartitionForSite(
          profile_, application_id.origin)->GetServiceWorkerContext();

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
      service_worker_context, application_id.service_worker_registration_id,
      application_id.origin, updated_data,
      base::Bind(&IgnoreResult));  // This is a heuristic; ignore failure.

  if (needed_but_not_shown && missed_notifications.count() >= 2) {
    // The site failed to show a notification when one was needed, and they have
    // already failed once in the previous 10 push messages, so we will show a
    // generic notification. See https://crbug.com/437277.
    // TODO(johnme): The generic notification should probably automatically
    // close itself when the next push message arrives?
    content::PlatformNotificationData notification_data;
    // TODO(johnme): Switch to FormatOriginForDisplay from crbug.com/402698
    notification_data.title = l10n_util::GetStringFUTF16(
        IDS_PUSH_MESSAGING_GENERIC_NOTIFICATION_TITLE,
        base::UTF8ToUTF16(application_id.origin.host()));
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
        application_id.service_worker_registration_id,
        application_id.origin,
        SkBitmap() /* icon */,
        notification_data,
        content::ChildProcessHost::kInvalidUniqueID /* render_process_id */);
  }
}

void PushMessagingServiceImpl::OnMessagesDeleted(const std::string& app_id) {
  // TODO(mvanouwerkerk): Fire push error event on the Service Worker
  // corresponding to app_id.
}

void PushMessagingServiceImpl::OnSendError(
    const std::string& app_id,
    const GCMClient::SendErrorDetails& send_error_details) {
  NOTREACHED() << "The Push API shouldn't have sent messages upstream";
}

void PushMessagingServiceImpl::OnSendAcknowledged(
    const std::string& app_id,
    const std::string& message_id) {
  NOTREACHED() << "The Push API shouldn't have sent messages upstream";
}

GURL PushMessagingServiceImpl::GetPushEndpoint() {
  return GURL(std::string(kPushMessagingEndpoint));
}

void PushMessagingServiceImpl::RegisterFromDocument(
    const GURL& requesting_origin,
    int64 service_worker_registration_id,
    const std::string& sender_id,
    int renderer_id,
    int render_frame_id,
    bool user_visible_only,
    const content::PushMessagingService::RegisterCallback& callback) {
  if (!gcm_profile_service_->driver()) {
    NOTREACHED() << "There is no GCMDriver. Has GCMProfileService shut down?";
    return;
  }

  PushMessagingApplicationId application_id = PushMessagingApplicationId(
      requesting_origin, service_worker_registration_id);
  DCHECK(application_id.IsValid());

  if (push_registration_count_ >= kMaxRegistrations) {
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

  gcm::PushMessagingPermissionContext* permission_context =
      gcm::PushMessagingPermissionContextFactory::GetForProfile(profile_);

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
  if (!gcm_profile_service_->driver()) {
    NOTREACHED() << "There is no GCMDriver. Has GCMProfileService shut down?";
    return;
  }

  PushMessagingApplicationId application_id = PushMessagingApplicationId(
      requesting_origin, service_worker_registration_id);
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

  std::vector<std::string> sender_ids(1, sender_id);
  gcm_profile_service_->driver()->Register(
      application_id.ToString(), sender_ids,
      base::Bind(&PushMessagingServiceImpl::DidRegister,
                 weak_factory_.GetWeakPtr(), register_callback));
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
  if (status == content::PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE)
    IncreasePushRegistrationCount(1);
}

void PushMessagingServiceImpl::DidRegister(
    const content::PushMessagingService::RegisterCallback& callback,
    const std::string& registration_id,
    GCMClient::Result result) {
  content::PushRegistrationStatus status =
      result == GCMClient::SUCCESS
          ? content::PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE
          : content::PUSH_REGISTRATION_STATUS_SERVICE_ERROR;
  RegisterEnd(callback, registration_id, status);
}

void PushMessagingServiceImpl::DidRequestPermission(
    const PushMessagingApplicationId& application_id,
    const std::string& sender_id,
    const content::PushMessagingService::RegisterCallback& register_callback,
    bool allow) {
  if (!allow) {
    RegisterEnd(register_callback,
                std::string(),
                content::PUSH_REGISTRATION_STATUS_PERMISSION_DENIED);
    return;
  }

  // The GCMDriver could be NULL if GCMProfileService has been shut down.
  if (!gcm_profile_service_->driver())
    return;

  std::vector<std::string> sender_ids(1, sender_id);

  gcm_profile_service_->driver()->Register(
      application_id.ToString(),
      sender_ids,
      base::Bind(&PushMessagingServiceImpl::DidRegister,
                 weak_factory_.GetWeakPtr(),
                 register_callback));
}

void PushMessagingServiceImpl::Unregister(
    const GURL& requesting_origin,
    int64 service_worker_registration_id,
    const content::PushMessagingService::UnregisterCallback& callback) {
  DCHECK(gcm_profile_service_->driver());

  PushMessagingApplicationId application_id = PushMessagingApplicationId(
      requesting_origin, service_worker_registration_id);
  DCHECK(application_id.IsValid());

  Unregister(application_id, callback);
}

void PushMessagingServiceImpl::Unregister(
    const PushMessagingApplicationId& application_id,
    const content::PushMessagingService::UnregisterCallback& callback) {
  DCHECK(gcm_profile_service_->driver());

  gcm_profile_service_->driver()->Unregister(
      application_id.ToString(),
      base::Bind(&PushMessagingServiceImpl::DidUnregister,
                 weak_factory_.GetWeakPtr(), callback));
}

void PushMessagingServiceImpl::DidUnregister(
    const content::PushMessagingService::UnregisterCallback& callback,
    GCMClient::Result result) {
  // Internal calls pass a null callback.
  if (!callback.is_null()) {
    switch (result) {
    case GCMClient::SUCCESS:
      callback.Run(content::PUSH_UNREGISTRATION_STATUS_SUCCESS_UNREGISTER);
      break;
    case GCMClient::NETWORK_ERROR:
    case GCMClient::TTL_EXCEEDED:
      callback.Run(content::PUSH_UNREGISTRATION_STATUS_NETWORK_ERROR);
      break;
    case GCMClient::SERVER_ERROR:
    case GCMClient::INVALID_PARAMETER:
    case GCMClient::GCM_DISABLED:
    case GCMClient::ASYNC_OPERATION_PENDING:
    case GCMClient::UNKNOWN_ERROR:
      callback.Run(content::PUSH_UNREGISTRATION_STATUS_UNKNOWN_ERROR);
      break;
    default:
      NOTREACHED() << "Unexpected GCMClient::Result value.";
      callback.Run(content::PUSH_UNREGISTRATION_STATUS_UNKNOWN_ERROR);
      break;
    }
  }

  if (result == GCMClient::SUCCESS)
    DecreasePushRegistrationCount(1);
}

bool PushMessagingServiceImpl::HasPermission(const GURL& origin) {
  gcm::PushMessagingPermissionContext* permission_context =
      gcm::PushMessagingPermissionContextFactory::GetForProfile(profile_);
  DCHECK(permission_context);

  return permission_context->GetPermissionStatus(origin, origin) ==
      CONTENT_SETTING_ALLOW;
}

}  // namespace gcm
