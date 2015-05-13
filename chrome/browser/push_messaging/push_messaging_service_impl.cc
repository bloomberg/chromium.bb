// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_service_impl.h"

#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"
#include "chrome/browser/push_messaging/push_messaging_constants.h"
#include "chrome/browser/push_messaging/push_messaging_permission_context.h"
#include "chrome/browser/push_messaging/push_messaging_permission_context_factory.h"
#include "chrome/browser/push_messaging/push_messaging_service_factory.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/permission_request_id.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/rappor/rappor_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/push_messaging_status.h"

namespace {
const int kMaxRegistrations = 1000000;

void RecordDeliveryStatus(content::PushDeliveryStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.DeliveryStatus",
                            status,
                            content::PUSH_DELIVERY_STATUS_LAST + 1);
}

blink::WebPushPermissionStatus ToPushPermission(ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      return blink::WebPushPermissionStatusGranted;
    case CONTENT_SETTING_BLOCK:
      return blink::WebPushPermissionStatusDenied;
    case CONTENT_SETTING_ASK:
      return blink::WebPushPermissionStatusPrompt;
    default:
      NOTREACHED();
      return blink::WebPushPermissionStatusDenied;
  }
}

void UnregisterCallbackToClosure(
    const base::Closure& closure, content::PushUnregistrationStatus status) {
  closure.Run();
}

}  // namespace

// static
void PushMessagingServiceImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kPushMessagingRegistrationCount, 0);
  PushMessagingAppIdentifier::RegisterProfilePrefs(registry);
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
#if defined(ENABLE_NOTIFICATIONS)
      notification_manager_(profile),
#endif
      weak_factory_(this) {
  DCHECK(profile);
  profile_->GetHostContentSettingsMap()->AddObserver(this);
}

PushMessagingServiceImpl::~PushMessagingServiceImpl() {
  profile_->GetHostContentSettingsMap()->RemoveObserver(this);
}

void PushMessagingServiceImpl::IncreasePushRegistrationCount(int add,
                                                             bool is_pending) {
  DCHECK(add > 0);
  if (push_registration_count_ + pending_push_registration_count_ == 0) {
    GetGCMDriver()->AddAppHandler(kPushMessagingAppIdentifierPrefix, this);
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
    GetGCMDriver()->RemoveAppHandler(kPushMessagingAppIdentifierPrefix);
  }
}

bool PushMessagingServiceImpl::CanHandle(const std::string& app_id) const {
  return !PushMessagingAppIdentifier::Get(profile_, app_id).is_null();
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
  base::Closure message_handled_closure =
      message_callback_for_testing_.is_null() ? base::Bind(&base::DoNothing)
                                              : message_callback_for_testing_;
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::Get(profile_, app_id);
  // Drop message and unregister if app_id was unknown (maybe recently deleted).
  if (app_identifier.is_null()) {
    DeliverMessageCallback(app_id, GURL::EmptyGURL(), -1, message,
                           message_handled_closure,
                           content::PUSH_DELIVERY_STATUS_UNKNOWN_APP_ID);
    return;
  }
  // Drop message and unregister if |origin| has lost push permission.
  if (!HasPermission(app_identifier.origin())) {
    DeliverMessageCallback(app_id, app_identifier.origin(),
                           app_identifier.service_worker_registration_id(),
                           message, message_handled_closure,
                           content::PUSH_DELIVERY_STATUS_PERMISSION_DENIED);
    return;
  }

  rappor::SampleDomainAndRegistryFromGURL(
      g_browser_process->rappor_service(),
      "PushMessaging.MessageReceived.Origin",
      app_identifier.origin());

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
      app_identifier.origin(),
      app_identifier.service_worker_registration_id(),
      data,
      base::Bind(&PushMessagingServiceImpl::DeliverMessageCallback,
                 weak_factory_.GetWeakPtr(),
                 app_identifier.app_id(), app_identifier.origin(),
                 app_identifier.service_worker_registration_id(), message,
                 message_handled_closure));
}

void PushMessagingServiceImpl::DeliverMessageCallback(
    const std::string& app_id,
    const GURL& requesting_origin,
    int64 service_worker_registration_id,
    const gcm::GCMClient::IncomingMessage& message,
    const base::Closure& message_handled_closure,
    content::PushDeliveryStatus status) {
  // TODO(mvanouwerkerk): Show a warning in the developer console of the
  // Service Worker corresponding to app_id (and/or on an internals page).
  // TODO(mvanouwerkerk): Is there a way to recover from failure?
  switch (status) {
    // Call EnforceUserVisibleOnlyRequirements if the message was delivered to
    // the Service Worker JavaScript, even if the website's event handler failed
    // (to prevent sites deliberately failing in order to avoid having to show
    // notifications).
    case content::PUSH_DELIVERY_STATUS_SUCCESS:
    case content::PUSH_DELIVERY_STATUS_EVENT_WAITUNTIL_REJECTED:
#if defined(ENABLE_NOTIFICATIONS)
      notification_manager_.EnforceUserVisibleOnlyRequirements(
          requesting_origin, service_worker_registration_id,
          message_handled_closure);
#else
      message_handled_closure.Run();
#endif
      break;
    case content::PUSH_DELIVERY_STATUS_INVALID_MESSAGE:
    case content::PUSH_DELIVERY_STATUS_SERVICE_WORKER_ERROR:
      message_handled_closure.Run();
      break;
    case content::PUSH_DELIVERY_STATUS_UNKNOWN_APP_ID:
    case content::PUSH_DELIVERY_STATUS_PERMISSION_DENIED:
    case content::PUSH_DELIVERY_STATUS_NO_SERVICE_WORKER:
      Unregister(app_id, message.sender_id,
                 base::Bind(&UnregisterCallbackToClosure,
                 message_handled_closure));
      break;
  }
  RecordDeliveryStatus(status);
}

void PushMessagingServiceImpl::SetMessageCallbackForTesting(
      const base::Closure& callback) {
  message_callback_for_testing_ = callback;
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
    bool user_visible,
    const content::PushMessagingService::RegisterCallback& callback) {
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::Generate(requesting_origin,
                                           service_worker_registration_id);

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

  if (permission_context == NULL || !user_visible) {
    RegisterEnd(callback,
                std::string(),
                content::PUSH_REGISTRATION_STATUS_PERMISSION_DENIED);
    return;
  }

  // TODO(miguelg): Consider the value of |user_visible| when making the
  // permission request.
  // TODO(mlamouri): Move requesting Push permission over to using Mojo, and
  // re-introduce the ability of |user_gesture| when bubbles require this.
  // https://crbug.com/423770.
  permission_context->RequestPermission(
      web_contents, id, requesting_origin, true /* user_gesture */,
      base::Bind(&PushMessagingServiceImpl::DidRequestPermission,
                 weak_factory_.GetWeakPtr(), app_identifier, sender_id,
                 callback));
}

void PushMessagingServiceImpl::RegisterFromWorker(
    const GURL& requesting_origin,
    int64 service_worker_registration_id,
    const std::string& sender_id,
    bool user_visible,
    const content::PushMessagingService::RegisterCallback& register_callback) {
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::Generate(requesting_origin,
                                           service_worker_registration_id);

  if (profile_->GetPrefs()->GetInteger(
          prefs::kPushMessagingRegistrationCount) >= kMaxRegistrations) {
    RegisterEnd(register_callback, std::string(),
                content::PUSH_REGISTRATION_STATUS_LIMIT_REACHED);
    return;
  }

  // TODO(peter): Consider |user_visible| when getting the permission status
  // for registering from a worker.

  GURL embedding_origin = requesting_origin;
  blink::WebPushPermissionStatus permission_status =
      PushMessagingServiceImpl::GetPermissionStatus(requesting_origin,
                                                    embedding_origin,
                                                    user_visible);
  if (permission_status != blink::WebPushPermissionStatusGranted) {
    RegisterEnd(register_callback, std::string(),
                content::PUSH_REGISTRATION_STATUS_PERMISSION_DENIED);
    return;
  }

  IncreasePushRegistrationCount(1, true /* is_pending */);
  std::vector<std::string> sender_ids(1, sender_id);
  GetGCMDriver()->Register(
      app_identifier.app_id(), sender_ids,
      base::Bind(&PushMessagingServiceImpl::DidRegister,
                 weak_factory_.GetWeakPtr(),
                 app_identifier, register_callback));
}

blink::WebPushPermissionStatus PushMessagingServiceImpl::GetPermissionStatus(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_visible) {
  // TODO(peter): Consider |user_visible| when checking Push permission.

  PushMessagingPermissionContext* permission_context =
      PushMessagingPermissionContextFactory::GetForProfile(profile_);
  return ToPushPermission(permission_context->GetPermissionStatus(
      requesting_origin, embedding_origin));
}

bool PushMessagingServiceImpl::SupportNonVisibleMessages() {
  return false;
}

void PushMessagingServiceImpl::RegisterEnd(
    const content::PushMessagingService::RegisterCallback& callback,
    const std::string& registration_id,
    content::PushRegistrationStatus status) {
  callback.Run(registration_id, status);
}

void PushMessagingServiceImpl::DidRegister(
    const PushMessagingAppIdentifier& app_identifier,
    const content::PushMessagingService::RegisterCallback& callback,
    const std::string& registration_id,
    gcm::GCMClient::Result result) {
  content::PushRegistrationStatus status =
      content::PUSH_REGISTRATION_STATUS_SERVICE_ERROR;
  switch (result) {
    case gcm::GCMClient::SUCCESS:
      status = content::PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE;
      app_identifier.PersistToPrefs(profile_);
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
    const PushMessagingAppIdentifier& app_identifier,
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
      app_identifier.app_id(),
      sender_ids,
      base::Bind(&PushMessagingServiceImpl::DidRegister,
                 weak_factory_.GetWeakPtr(),
                 app_identifier, register_callback));
}

// Unregister methods ----------------------------------------------------------

void PushMessagingServiceImpl::Unregister(
    const GURL& requesting_origin,
    int64 service_worker_registration_id,
    const std::string& sender_id,
    const content::PushMessagingService::UnregisterCallback& callback) {
  PushMessagingAppIdentifier app_identifier = PushMessagingAppIdentifier::Get(
      profile_, requesting_origin, service_worker_registration_id);
  if (app_identifier.is_null()) {
    if (!callback.is_null()) {
      callback.Run(
          content::PUSH_UNREGISTRATION_STATUS_SUCCESS_WAS_NOT_REGISTERED);
    }
    return;
  }

  Unregister(app_identifier.app_id(), sender_id, callback);
}

void PushMessagingServiceImpl::Unregister(
    const std::string& app_id,
    const std::string& sender_id,
    const content::PushMessagingService::UnregisterCallback& callback) {
  // Delete the mapping for this app_id, to guarantee that no messages get
  // delivered in future (even if unregistration fails).
  // TODO(johnme): Instead of deleting these app ids, store them elsewhere, and
  // retry unregistration if it fails due to network errors (crbug.com/465399).
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::Get(profile_, app_id);
  bool was_registered = !app_identifier.is_null();
  if (was_registered)
    app_identifier.DeleteFromPrefs(profile_);

  const auto& unregister_callback =
      base::Bind(&PushMessagingServiceImpl::DidUnregister,
                 weak_factory_.GetWeakPtr(),
                 was_registered, callback);
#if defined(OS_ANDROID)
  // On Android the backend is different, and requires the original sender_id.
  // UnregisterBecausePermissionRevoked sometimes calls us with an empty one.
  if (sender_id.empty())
    unregister_callback.Run(gcm::GCMClient::INVALID_PARAMETER);
  else
    GetGCMDriver()->UnregisterWithSenderId(app_id, sender_id,
                                           unregister_callback);
#else
  GetGCMDriver()->Unregister(app_id, unregister_callback);
#endif
}

void PushMessagingServiceImpl::DidUnregister(
    bool was_registered,
    const content::PushMessagingService::UnregisterCallback& callback,
    gcm::GCMClient::Result result) {
  if (was_registered)
    DecreasePushRegistrationCount(1, false /* was_pending */);

  // Internal calls pass a null callback.
  if (callback.is_null())
    return;

  if (!was_registered) {
    callback.Run(
        content::PUSH_UNREGISTRATION_STATUS_SUCCESS_WAS_NOT_REGISTERED);
    return;
  }
  switch (result) {
    case gcm::GCMClient::SUCCESS:
      callback.Run(content::PUSH_UNREGISTRATION_STATUS_SUCCESS_UNREGISTERED);
      break;
    case gcm::GCMClient::INVALID_PARAMETER:
    case gcm::GCMClient::GCM_DISABLED:
    case gcm::GCMClient::SERVER_ERROR:
    case gcm::GCMClient::UNKNOWN_ERROR:
      callback.Run(content::PUSH_UNREGISTRATION_STATUS_PENDING_SERVICE_ERROR);
      break;
    case gcm::GCMClient::ASYNC_OPERATION_PENDING:
    case gcm::GCMClient::NETWORK_ERROR:
    case gcm::GCMClient::TTL_EXCEEDED:
      callback.Run(content::PUSH_UNREGISTRATION_STATUS_PENDING_NETWORK_ERROR);
      break;
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

  std::vector<PushMessagingAppIdentifier> all_app_identifiers =
      PushMessagingAppIdentifier::GetAll(profile_);

  base::Closure barrier_closure = base::BarrierClosure(
      all_app_identifiers.size(),
      content_setting_changed_callback_for_testing_.is_null()
          ? base::Bind(&base::DoNothing)
          : content_setting_changed_callback_for_testing_);

  for (const PushMessagingAppIdentifier& app_identifier : all_app_identifiers) {
    // If |primary_pattern| is not valid, we should always check for a
    // permission change because it can happen for example when the entire
    // Push or Notifications permissions are cleared.
    // Otherwise, the permission should be checked if the pattern matches the
    // origin.
    if (primary_pattern.IsValid() &&
        !primary_pattern.Matches(app_identifier.origin())) {
      barrier_closure.Run();
      continue;
    }

    if (HasPermission(app_identifier.origin())) {
      barrier_closure.Run();
      continue;
    }

    GetSenderId(
        profile_, app_identifier.origin(),
        app_identifier.service_worker_registration_id(),
        base::Bind(
            &PushMessagingServiceImpl::UnregisterBecausePermissionRevoked,
            weak_factory_.GetWeakPtr(), app_identifier, barrier_closure));
  }
}

void PushMessagingServiceImpl::UnregisterBecausePermissionRevoked(
    const PushMessagingAppIdentifier& app_identifier,
    const base::Closure& closure, const std::string& sender_id,
    bool success, bool not_found) {
  base::Closure barrier_closure = base::BarrierClosure(2, closure);

  // Unregister the PushMessagingAppIdentifier with the push service.
  // It's possible for GetSenderId to have failed and sender_id to be empty, if
  // cookies (and the SW database) for an origin got cleared before permissions
  // are cleared for the origin. In that case Unregister will just delete the
  // app identifier to block future messages.
  // TODO(johnme): Auto-unregister before SW DB is cleared
  // (https://crbug.com/402458).
  Unregister(app_identifier.app_id(), sender_id,
             base::Bind(&UnregisterCallbackToClosure, barrier_closure));

  // Clear the associated service worker push registration id.
  ClearPushRegistrationID(profile_, app_identifier.origin(),
                          app_identifier.service_worker_registration_id(),
                          barrier_closure);
}

void PushMessagingServiceImpl::SetContentSettingChangedCallbackForTesting(
      const base::Closure& callback) {
  content_setting_changed_callback_for_testing_ = callback;
}

// KeyedService methods -------------------------------------------------------

void PushMessagingServiceImpl::Shutdown() {
  GetGCMDriver()->RemoveAppHandler(kPushMessagingAppIdentifierPrefix);
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
