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
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"
#include "chrome/browser/push_messaging/push_messaging_constants.h"
#include "chrome/browser/push_messaging/push_messaging_service_factory.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/rappor/rappor_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/push_messaging_status.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(ENABLE_BACKGROUND)
#include "chrome/browser/background/background_mode_manager.h"
#endif

namespace {
const int kMaxRegistrations = 1000000;

// Chrome does not yet support silent push messages, and requires websites to
// indicate that they will only send user-visible messages.
const char kSilentPushUnsupportedMessage[] =
    "Chrome currently only supports the Push API for subscriptions that will "
    "result in user-visible messages. You can indicate this by calling "
    "pushManager.subscribe({userVisibleOnly: true}) instead. See "
    "https://goo.gl/yqv4Q4 for more details.";

void RecordDeliveryStatus(content::PushDeliveryStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.DeliveryStatus",
                            status,
                            content::PUSH_DELIVERY_STATUS_LAST + 1);
}

blink::WebPushPermissionStatus ToPushPermission(
    content::PermissionStatus permission_status) {
  switch (permission_status) {
    case content::PERMISSION_STATUS_GRANTED:
      return blink::WebPushPermissionStatusGranted;
    case content::PERMISSION_STATUS_DENIED:
      return blink::WebPushPermissionStatusDenied;
    case content::PERMISSION_STATUS_ASK:
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

#if defined(ENABLE_BACKGROUND)
bool UseBackgroundMode() {
  // Note: if push is ever enabled in incognito, the background mode integration
  // should not be enabled for it.
  std::string group_name =
      base::FieldTrialList::FindFullName("PushApiBackgroundMode");
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisablePushApiBackgroundMode))
    return false;
  if (command_line->HasSwitch(switches::kEnablePushApiBackgroundMode))
    return true;
  return group_name == "Enabled";
}
#endif  // defined(ENABLE_BACKGROUND)

}  // namespace

// static
void PushMessagingServiceImpl::InitializeForProfile(Profile* profile) {
  // TODO(johnme): Consider whether push should be enabled in incognito.
  if (!profile || profile->IsOffTheRecord())
    return;

  int count = PushMessagingAppIdentifier::GetCount(profile);
  if (count <= 0)
    return;

  PushMessagingServiceImpl* push_service =
      PushMessagingServiceFactory::GetForProfile(profile);
  push_service->IncreasePushSubscriptionCount(count, false /* is_pending */);
}

PushMessagingServiceImpl::PushMessagingServiceImpl(Profile* profile)
    : profile_(profile),
      push_subscription_count_(0),
      pending_push_subscription_count_(0),
#if defined(ENABLE_NOTIFICATIONS)
      notification_manager_(profile),
#endif
      weak_factory_(this) {
  DCHECK(profile);
  HostContentSettingsMapFactory::GetForProfile(profile_)->AddObserver(this);
}

PushMessagingServiceImpl::~PushMessagingServiceImpl() {
  HostContentSettingsMapFactory::GetForProfile(profile_)->RemoveObserver(this);
}

void PushMessagingServiceImpl::IncreasePushSubscriptionCount(int add,
                                                             bool is_pending) {
  DCHECK(add > 0);
  if (push_subscription_count_ + pending_push_subscription_count_ == 0) {
    GetGCMDriver()->AddAppHandler(kPushMessagingAppIdentifierPrefix, this);
  }
  if (is_pending) {
    pending_push_subscription_count_ += add;
  } else {
#if defined(ENABLE_BACKGROUND)
    if (UseBackgroundMode() && g_browser_process->background_mode_manager() &&
        !push_subscription_count_) {
      g_browser_process->background_mode_manager()->RegisterTrigger(
          profile_, this, false /* should_notify_user */);
    }
#endif  // defined(ENABLE_BACKGROUND)
    push_subscription_count_ += add;
  }
}

void PushMessagingServiceImpl::DecreasePushSubscriptionCount(int subtract,
                                                             bool was_pending) {
  DCHECK(subtract > 0);
  if (was_pending) {
    pending_push_subscription_count_ -= subtract;
    DCHECK(pending_push_subscription_count_ >= 0);
  } else {
    push_subscription_count_ -= subtract;
    DCHECK(push_subscription_count_ >= 0);
  }
  if (push_subscription_count_ + pending_push_subscription_count_ == 0) {
    GetGCMDriver()->RemoveAppHandler(kPushMessagingAppIdentifierPrefix);

#if defined(ENABLE_BACKGROUND)
    if (UseBackgroundMode() && g_browser_process->background_mode_manager()) {
      g_browser_process->background_mode_manager()->UnregisterTrigger(profile_,
                                                                      this);
    }
#endif  // defined(ENABLE_BACKGROUND)
  }
}

bool PushMessagingServiceImpl::CanHandle(const std::string& app_id) const {
  return !PushMessagingAppIdentifier::FindByAppId(profile_, app_id).is_null();
}

void PushMessagingServiceImpl::ShutdownHandler() {
  // Shutdown() should come before and it removes us from the list of app
  // handlers of gcm::GCMDriver so this shouldn't ever been called.
  NOTREACHED();
}

// OnMessage methods -----------------------------------------------------------

void PushMessagingServiceImpl::OnMessage(const std::string& app_id,
                                         const gcm::IncomingMessage& message) {
  in_flight_message_deliveries_.insert(app_id);

  base::Closure message_handled_closure =
      message_callback_for_testing_.is_null() ? base::Bind(&base::DoNothing)
                                              : message_callback_for_testing_;
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByAppId(profile_, app_id);
  // Drop message and unregister if app_id was unknown (maybe recently deleted).
  if (app_identifier.is_null()) {
    DeliverMessageCallback(app_id, GURL::EmptyGURL(), -1, message,
                           message_handled_closure,
                           content::PUSH_DELIVERY_STATUS_UNKNOWN_APP_ID);
    return;
  }
  // Drop message and unregister if |origin| has lost push permission.
  if (!IsPermissionSet(app_identifier.origin())) {
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

  std::string data;
  if (AreMessagePayloadsEnabled() && message.decrypted)
    data = message.raw_data;

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
    const gcm::IncomingMessage& message,
    const base::Closure& message_handled_closure,
    content::PushDeliveryStatus status) {
  // Remove a single in-flight delivery for |app_id|. This has to be done using
  // an iterator rather than by value, as the latter removes all entries.
  DCHECK(in_flight_message_deliveries_.find(app_id) !=
         in_flight_message_deliveries_.end());

  in_flight_message_deliveries_.erase(
      in_flight_message_deliveries_.find(app_id));

  // TODO(mvanouwerkerk): Show a warning in the developer console of the
  // Service Worker corresponding to app_id (and/or on an internals page).
  // See https://crbug.com/508516 for options.
  switch (status) {
    // Call EnforceUserVisibleOnlyRequirements if the message was delivered to
    // the Service Worker JavaScript, even if the website's event handler failed
    // (to prevent sites deliberately failing in order to avoid having to show
    // notifications).
    case content::PUSH_DELIVERY_STATUS_SUCCESS:
    case content::PUSH_DELIVERY_STATUS_EVENT_WAITUNTIL_REJECTED:
#if defined(ENABLE_NOTIFICATIONS)
      // Only enforce the user visible requirements after the entire queue of
      // incoming messages for |app_id| has been flushed.
      if (!in_flight_message_deliveries_.count(app_id)) {
        notification_manager_.EnforceUserVisibleOnlyRequirements(
            requesting_origin, service_worker_registration_id,
            message_handled_closure);
      } else {
        message_handled_closure.Run();
      }
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
      Unsubscribe(
          app_id, message.sender_id,
          base::Bind(&UnregisterCallbackToClosure, message_handled_closure));
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

// Subscribe and GetPermissionStatus methods -----------------------------------

void PushMessagingServiceImpl::SubscribeFromDocument(
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

  if (push_subscription_count_ + pending_push_subscription_count_ >=
      kMaxRegistrations) {
    SubscribeEndWithError(callback,
                          content::PUSH_REGISTRATION_STATUS_LIMIT_REACHED);
    return;
  }

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(renderer_id, render_frame_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;

  if (!user_visible) {
    web_contents->GetMainFrame()->AddMessageToConsole(
        content::CONSOLE_MESSAGE_LEVEL_ERROR,
        kSilentPushUnsupportedMessage);

    SubscribeEndWithError(callback,
                          content::PUSH_REGISTRATION_STATUS_PERMISSION_DENIED);
    return;
  }

  // Push does not allow permission requests from iframes.
  profile_->GetPermissionManager()->RequestPermission(
      content::PermissionType::PUSH_MESSAGING, web_contents->GetMainFrame(),
      requesting_origin, true /* user_gesture */,
      base::Bind(&PushMessagingServiceImpl::DidRequestPermission,
                 weak_factory_.GetWeakPtr(), app_identifier, sender_id,
                 callback));
}

void PushMessagingServiceImpl::SubscribeFromWorker(
    const GURL& requesting_origin,
    int64 service_worker_registration_id,
    const std::string& sender_id,
    bool user_visible,
    const content::PushMessagingService::RegisterCallback& register_callback) {
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::Generate(requesting_origin,
                                           service_worker_registration_id);

  if (push_subscription_count_ + pending_push_subscription_count_ >=
      kMaxRegistrations) {
    SubscribeEndWithError(register_callback,
                          content::PUSH_REGISTRATION_STATUS_LIMIT_REACHED);
    return;
  }

  GURL embedding_origin = requesting_origin;
  blink::WebPushPermissionStatus permission_status =
      PushMessagingServiceImpl::GetPermissionStatus(requesting_origin,
                                                    embedding_origin,
                                                    user_visible);
  if (permission_status != blink::WebPushPermissionStatusGranted) {
    SubscribeEndWithError(register_callback,
                          content::PUSH_REGISTRATION_STATUS_PERMISSION_DENIED);
    return;
  }

  IncreasePushSubscriptionCount(1, true /* is_pending */);
  std::vector<std::string> sender_ids(1, sender_id);
  GetGCMDriver()->Register(app_identifier.app_id(), sender_ids,
                           base::Bind(&PushMessagingServiceImpl::DidSubscribe,
                                      weak_factory_.GetWeakPtr(),
                                      app_identifier, register_callback));
}

blink::WebPushPermissionStatus PushMessagingServiceImpl::GetPermissionStatus(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_visible) {
  if (!user_visible)
    return blink::WebPushPermissionStatusDenied;

  return ToPushPermission(profile_->GetPermissionManager()->GetPermissionStatus(
      content::PermissionType::PUSH_MESSAGING, requesting_origin,
      embedding_origin));
}

bool PushMessagingServiceImpl::SupportNonVisibleMessages() {
  return false;
}

void PushMessagingServiceImpl::SubscribeEnd(
    const content::PushMessagingService::RegisterCallback& callback,
    const std::string& subscription_id,
    const std::vector<uint8_t>& p256dh,
    content::PushRegistrationStatus status) {
  callback.Run(subscription_id, p256dh, status);
}

void PushMessagingServiceImpl::SubscribeEndWithError(
    const content::PushMessagingService::RegisterCallback& callback,
    content::PushRegistrationStatus status) {
  SubscribeEnd(callback, std::string() /* subscription_id */,
               std::vector<uint8_t>() /* p256dh */, status);
}

void PushMessagingServiceImpl::DidSubscribe(
    const PushMessagingAppIdentifier& app_identifier,
    const content::PushMessagingService::RegisterCallback& callback,
    const std::string& subscription_id,
    gcm::GCMClient::Result result) {
  DecreasePushSubscriptionCount(1, true /* was_pending */);

  content::PushRegistrationStatus status =
      content::PUSH_REGISTRATION_STATUS_SERVICE_ERROR;

  switch (result) {
    case gcm::GCMClient::SUCCESS:
      // Do not get a certificate if message payloads have not been enabled.
      if (!AreMessagePayloadsEnabled()) {
        DidSubscribeWithPublicKey(
            app_identifier, callback, subscription_id,
            std::string() /* public_key */);
        return;
      }

      // Make sure that this subscription has associated encryption keys prior
      // to returning it to the developer - they'll need this information in
      // order to send payloads to the user.
      GetGCMDriver()->GetPublicKey(
          app_identifier.app_id(),
          base::Bind(
              &PushMessagingServiceImpl::DidSubscribeWithPublicKey,
              weak_factory_.GetWeakPtr(), app_identifier, callback,
              subscription_id));

      return;
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

  SubscribeEndWithError(callback, status);
}

void PushMessagingServiceImpl::DidSubscribeWithPublicKey(
    const PushMessagingAppIdentifier& app_identifier,
    const content::PushMessagingService::RegisterCallback& callback,
    const std::string& subscription_id,
    const std::string& public_key) {
  if (!public_key.size() && AreMessagePayloadsEnabled()) {
    SubscribeEndWithError(
        callback, content::PUSH_REGISTRATION_STATUS_PUBLIC_KEY_UNAVAILABLE);
    return;
  }

  app_identifier.PersistToPrefs(profile_);

  IncreasePushSubscriptionCount(1, false /* is_pending */);

  SubscribeEnd(callback, subscription_id,
               std::vector<uint8_t>(public_key.begin(), public_key.end()),
               content::PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE);
}

void PushMessagingServiceImpl::DidRequestPermission(
    const PushMessagingAppIdentifier& app_identifier,
    const std::string& sender_id,
    const content::PushMessagingService::RegisterCallback& register_callback,
    content::PermissionStatus permission_status) {
  if (permission_status != content::PERMISSION_STATUS_GRANTED) {
    SubscribeEndWithError(register_callback,
                          content::PUSH_REGISTRATION_STATUS_PERMISSION_DENIED);
    return;
  }

  IncreasePushSubscriptionCount(1, true /* is_pending */);
  std::vector<std::string> sender_ids(1, sender_id);
  GetGCMDriver()->Register(app_identifier.app_id(), sender_ids,
                           base::Bind(&PushMessagingServiceImpl::DidSubscribe,
                                      weak_factory_.GetWeakPtr(),
                                      app_identifier, register_callback));
}

// GetPublicEncryptionKey methods ----------------------------------------------

void PushMessagingServiceImpl::GetPublicEncryptionKey(
    const GURL& origin,
    int64_t service_worker_registration_id,
    const PushMessagingService::PublicKeyCallback& callback) {
  // An empty public key will be returned if payloads are not enabled.
  if (!AreMessagePayloadsEnabled()) {
    callback.Run(true /* success */, std::vector<uint8_t>());
    return;
  }

  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(
          profile_, origin, service_worker_registration_id);

  DCHECK(!app_identifier.is_null());

  GetGCMDriver()->GetPublicKey(
      app_identifier.app_id(),
      base::Bind(&PushMessagingServiceImpl::DidGetPublicKey,
                 weak_factory_.GetWeakPtr(), callback));
}

void PushMessagingServiceImpl::DidGetPublicKey(
    const PushMessagingService::PublicKeyCallback& callback,
    const std::string& public_key) const {
  // I/O errors might prevent the GCM Driver from retrieving a key-pair.
  const bool success = !!public_key.size();

  callback.Run(success, std::vector<uint8_t>(public_key.begin(),
                                             public_key.end()));
}

// Unsubscribe methods ---------------------------------------------------------

void PushMessagingServiceImpl::Unsubscribe(
    const GURL& requesting_origin,
    int64 service_worker_registration_id,
    const std::string& sender_id,
    const content::PushMessagingService::UnregisterCallback& callback) {
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(
          profile_, requesting_origin, service_worker_registration_id);
  if (app_identifier.is_null()) {
    if (!callback.is_null()) {
      callback.Run(
          content::PUSH_UNREGISTRATION_STATUS_SUCCESS_WAS_NOT_REGISTERED);
    }
    return;
  }

  Unsubscribe(app_identifier.app_id(), sender_id, callback);
}

void PushMessagingServiceImpl::Unsubscribe(
    const std::string& app_id,
    const std::string& sender_id,
    const content::PushMessagingService::UnregisterCallback& callback) {
  // Delete the mapping for this app_id, to guarantee that no messages get
  // delivered in future (even if unregistration fails).
  // TODO(johnme): Instead of deleting these app ids, store them elsewhere, and
  // retry unregistration if it fails due to network errors (crbug.com/465399).
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByAppId(profile_, app_id);
  bool was_registered = !app_identifier.is_null();
  if (was_registered)
    app_identifier.DeleteFromPrefs(profile_);

  const auto& unregister_callback =
      base::Bind(&PushMessagingServiceImpl::DidUnsubscribe,
                 weak_factory_.GetWeakPtr(), was_registered, callback);
#if defined(OS_ANDROID)
  // On Android the backend is different, and requires the original sender_id.
  // UnsubscribeBecausePermissionRevoked sometimes calls us with an empty one.
  if (sender_id.empty())
    unregister_callback.Run(gcm::GCMClient::INVALID_PARAMETER);
  else
    GetGCMDriver()->UnregisterWithSenderId(app_id, sender_id,
                                           unregister_callback);
#else
  GetGCMDriver()->Unregister(app_id, unregister_callback);
#endif
}

void PushMessagingServiceImpl::DidUnsubscribe(
    bool was_subscribed,
    const content::PushMessagingService::UnregisterCallback& callback,
    gcm::GCMClient::Result result) {
  if (was_subscribed)
    DecreasePushSubscriptionCount(1, false /* was_pending */);

  // Internal calls pass a null callback.
  if (callback.is_null())
    return;

  if (!was_subscribed) {
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

    if (IsPermissionSet(app_identifier.origin())) {
      barrier_closure.Run();
      continue;
    }

    GetSenderId(
        profile_, app_identifier.origin(),
        app_identifier.service_worker_registration_id(),
        base::Bind(
            &PushMessagingServiceImpl::UnsubscribeBecausePermissionRevoked,
            weak_factory_.GetWeakPtr(), app_identifier, barrier_closure));
  }
}

void PushMessagingServiceImpl::UnsubscribeBecausePermissionRevoked(
    const PushMessagingAppIdentifier& app_identifier,
    const base::Closure& closure,
    const std::string& sender_id,
    bool success,
    bool not_found) {
  base::Closure barrier_closure = base::BarrierClosure(2, closure);

  // Unsubscribe the PushMessagingAppIdentifier with the push service.
  // It's possible for GetSenderId to have failed and sender_id to be empty, if
  // cookies (and the SW database) for an origin got cleared before permissions
  // are cleared for the origin. In that case Unsubscribe will just delete the
  // app identifier to block future messages.
  // TODO(johnme): Auto-unregister before SW DB is cleared
  // (https://crbug.com/402458).
  Unsubscribe(app_identifier.app_id(), sender_id,
              base::Bind(&UnregisterCallbackToClosure, barrier_closure));

  // Clear the associated service worker push registration id.
  ClearPushSubscriptionID(profile_, app_identifier.origin(),
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

// BackgroundTrigger methods ---------------------------------------------------
base::string16 PushMessagingServiceImpl::GetName() {
  return l10n_util::GetStringUTF16(IDS_NOTIFICATIONS_BACKGROUND_SERVICE_NAME);
}

gfx::ImageSkia* PushMessagingServiceImpl::GetIcon() {
  return nullptr;
}

void PushMessagingServiceImpl::OnMenuClick() {
#if defined(ENABLE_BACKGROUND)
  chrome::ShowContentSettings(
      BackgroundModeManager::GetBrowserWindowForProfile(profile_),
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
#endif  // defined(ENABLE_BACKGROUND)
}

// Helper methods --------------------------------------------------------------

// Assumes user_visible always since this is just meant to check
// if the permission was previously granted and not revoked.
bool PushMessagingServiceImpl::IsPermissionSet(const GURL& origin) {
  return GetPermissionStatus(origin, origin, true /* user_visible */) ==
         blink::WebPushPermissionStatusGranted;
}

bool PushMessagingServiceImpl::AreMessagePayloadsEnabled() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePushMessagePayload);
}

gcm::GCMDriver* PushMessagingServiceImpl::GetGCMDriver() const {
  gcm::GCMProfileService* gcm_profile_service =
      gcm::GCMProfileServiceFactory::GetForProfile(profile_);
  CHECK(gcm_profile_service);
  CHECK(gcm_profile_service->driver());
  return gcm_profile_service->driver();
}
