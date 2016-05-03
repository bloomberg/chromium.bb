// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_IMPL_H_
#define CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_IMPL_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/background/background_trigger.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/gcm_driver/common/gcm_messages.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/permissions/permission_status.mojom.h"
#include "content/public/browser/push_messaging_service.h"
#include "content/public/common/push_event_payload.h"
#include "content/public/common/push_messaging_status.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushPermissionStatus.h"

#if defined(ENABLE_NOTIFICATIONS)
#include "chrome/browser/push_messaging/push_messaging_notification_manager.h"
#endif

class Profile;
class PushMessagingAppIdentifier;
class PushMessagingServiceObserver;
struct PushSubscriptionOptions;

namespace gcm {
class GCMDriver;
class GCMProfileService;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class PushMessagingServiceImpl : public content::PushMessagingService,
                                 public gcm::GCMAppHandler,
                                 public content_settings::Observer,
                                 public KeyedService,
                                 public BackgroundTrigger {
 public:
  // If any Service Workers are using push, starts GCM and adds an app handler.
  static void InitializeForProfile(Profile* profile);

  explicit PushMessagingServiceImpl(Profile* profile);
  ~PushMessagingServiceImpl() override;

  // gcm::GCMAppHandler implementation.
  void ShutdownHandler() override;
  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnSendError(
      const std::string& app_id,
      const gcm::GCMClient::SendErrorDetails& send_error_details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;
  bool CanHandle(const std::string& app_id) const override;

  // content::PushMessagingService implementation:
  GURL GetPushEndpoint() override;
  void SubscribeFromDocument(
      const GURL& requesting_origin,
      int64_t service_worker_registration_id,
      int renderer_id,
      int render_frame_id,
      const content::PushSubscriptionOptions& options,
      const content::PushMessagingService::RegisterCallback& callback) override;
  void SubscribeFromWorker(
      const GURL& requesting_origin,
      int64_t service_worker_registration_id,
      const content::PushSubscriptionOptions& options,
      const content::PushMessagingService::RegisterCallback& callback) override;
  void GetEncryptionInfo(
      const GURL& origin,
      int64_t service_worker_registration_id,
      const content::PushMessagingService::EncryptionInfoCallback& callback)
      override;
  void Unsubscribe(
      const GURL& requesting_origin,
      int64_t service_worker_registration_id,
      const std::string& sender_id,
      const content::PushMessagingService::UnregisterCallback&) override;
  blink::WebPushPermissionStatus GetPermissionStatus(
      const GURL& origin,
      bool user_visible) override;
  bool SupportNonVisibleMessages() override;

  // content_settings::Observer implementation.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               std::string resource_identifier) override;

  // KeyedService implementation.
  void Shutdown() override;

  // BackgroundTrigger implementation.
  base::string16 GetName() override;
  gfx::ImageSkia* GetIcon() override;
  void OnMenuClick() override;

  void SetMessageCallbackForTesting(const base::Closure& callback);
  void SetContentSettingChangedCallbackForTesting(
      const base::Closure& callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(PushMessagingServiceTest, NormalizeSenderInfo);
  FRIEND_TEST_ALL_PREFIXES(PushMessagingServiceTest, PayloadEncryptionTest);

  // A subscription is pending until it has succeeded or failed.
  void IncreasePushSubscriptionCount(int add, bool is_pending);
  void DecreasePushSubscriptionCount(int subtract, bool was_pending);

  // OnMessage methods ---------------------------------------------------------

  void DeliverMessageCallback(const std::string& app_id,
                              const GURL& requesting_origin,
                              int64_t service_worker_registration_id,
                              const gcm::IncomingMessage& message,
                              const base::Closure& message_handled_closure,
                              content::PushDeliveryStatus status);

  void DidHandleMessage(const std::string& app_id,
                        const base::Closure& completion_closure);

  // Subscribe methods ---------------------------------------------------------

  void SubscribeEnd(
      const content::PushMessagingService::RegisterCallback& callback,
      const std::string& subscription_id,
      const std::vector<uint8_t>& p256dh,
      const std::vector<uint8_t>& auth,
      content::PushRegistrationStatus status);

  void SubscribeEndWithError(
      const content::PushMessagingService::RegisterCallback& callback,
      content::PushRegistrationStatus status);

  void DidSubscribe(
      const PushMessagingAppIdentifier& app_identifier,
      const content::PushMessagingService::RegisterCallback& callback,
      const std::string& subscription_id,
      gcm::GCMClient::Result result);

  void DidSubscribeWithEncryptionInfo(
      const PushMessagingAppIdentifier& app_identifier,
      const content::PushMessagingService::RegisterCallback& callback,
      const std::string& subscription_id,
      const std::string& p256dh,
      const std::string& auth_secret);

  void DidRequestPermission(
      const PushMessagingAppIdentifier& app_identifier,
      const content::PushSubscriptionOptions& options,
      const content::PushMessagingService::RegisterCallback& callback,
      permissions::mojom::PermissionStatus permission_status);

  // GetEncryptionInfo method --------------------------------------------------

  void DidGetEncryptionInfo(
      const PushMessagingService::EncryptionInfoCallback& callback,
      const std::string& p256dh,
      const std::string& auth_secret) const;

  // Unsubscribe methods -------------------------------------------------------

  void Unsubscribe(const std::string& app_id,
                   const std::string& sender_id,
                   const content::PushMessagingService::UnregisterCallback&);

  void DidUnsubscribe(bool was_subscribed,
                      const content::PushMessagingService::UnregisterCallback&,
                      gcm::GCMClient::Result result);

  // OnContentSettingChanged methods -------------------------------------------

  void UnsubscribeBecausePermissionRevoked(
      const PushMessagingAppIdentifier& app_identifier,
      const base::Closure& closure,
      const std::string& sender_id,
      bool success,
      bool not_found);

  // Helper methods ------------------------------------------------------------

  // Normalizes the |sender_info|. In most cases the |sender_info| will be
  // passed through to the GCM Driver as-is, but NIST P-256 application server
  // keys have to be encoded using the URL-safe variant of the base64 encoding.
  std::string NormalizeSenderInfo(const std::string& sender_info) const;

  // Checks if a given origin is allowed to use Push.
  bool IsPermissionSet(const GURL& origin);

  gcm::GCMDriver* GetGCMDriver() const;

  // Testing methods -----------------------------------------------------------

  // Callback to be invoked when a message has been dispatched. Enables tests to
  // observe message delivery before it's dispatched to the Service Worker.
  using MessageDispatchedCallback =
      base::Callback<void(const std::string& app_id,
                          const GURL& origin,
                          int64_t service_worker_registration_id,
                          const content::PushEventPayload& payload)>;

  void SetMessageDispatchedCallbackForTesting(
      const MessageDispatchedCallback& callback) {
    message_dispatched_callback_for_testing_ = callback;
  }

  Profile* profile_;

  int push_subscription_count_;
  int pending_push_subscription_count_;

  base::Closure message_callback_for_testing_;
  base::Closure content_setting_changed_callback_for_testing_;

#if defined(ENABLE_NOTIFICATIONS)
  PushMessagingNotificationManager notification_manager_;
#endif

  // A multiset containing one entry for each in-flight push message delivery,
  // keyed by the receiver's app id.
  std::multiset<std::string> in_flight_message_deliveries_;

  MessageDispatchedCallback message_dispatched_callback_for_testing_;

  std::unique_ptr<PushMessagingServiceObserver>
      push_messaging_service_observer_;

  base::WeakPtrFactory<PushMessagingServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingServiceImpl);
};

#endif  // CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_IMPL_H_
