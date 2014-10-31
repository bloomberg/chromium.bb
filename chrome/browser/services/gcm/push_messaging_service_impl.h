// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_SERVICE_IMPL_H_
#define CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_SERVICE_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/gcm_client.h"
#include "content/public/browser/push_messaging_service.h"
#include "content/public/common/push_messaging_status.h"
#include "third_party/WebKit/public/platform/WebPushPermissionStatus.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace gcm {

class GCMProfileService;
struct PushMessagingApplicationId;

class PushMessagingServiceImpl : public content::PushMessagingService,
                                 public GCMAppHandler {
 public:
  // Register profile-specific prefs for GCM.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // If any Service Workers are using push, starts GCM and adds an app handler.
  static void InitializeForProfile(Profile* profile);

  PushMessagingServiceImpl(GCMProfileService* gcm_profile_service,
                           Profile* profile);
  ~PushMessagingServiceImpl() override;

  // GCMAppHandler implementation.
  void ShutdownHandler() override;
  void OnMessage(const std::string& app_id,
                 const GCMClient::IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnSendError(
      const std::string& app_id,
      const GCMClient::SendErrorDetails& send_error_details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;
  bool CanHandle(const std::string& app_id) const override;

  // content::PushMessagingService implementation:
  void Register(
      const GURL& origin,
      int64 service_worker_registration_id,
      const std::string& sender_id,
      int renderer_id,
      int render_frame_id,
      bool user_gesture,
      const content::PushMessagingService::RegisterCallback& callback) override;
  blink::WebPushPermissionStatus GetPermissionStatus(
      const GURL& requesting_origin,
      int renderer_id,
      int render_frame_id) override;

  void SetProfileForTesting(Profile* profile);

 private:
  void DeliverMessageCallback(const PushMessagingApplicationId& application_id,
                              const GCMClient::IncomingMessage& message,
                              content::PushDeliveryStatus status);

  void RegisterEnd(
      const content::PushMessagingService::RegisterCallback& callback,
      const std::string& registration_id,
      content::PushRegistrationStatus status);

  void DidRegister(
      const content::PushMessagingService::RegisterCallback& callback,
      const std::string& registration_id,
      GCMClient::Result result);

  void DidRequestPermission(
      const PushMessagingApplicationId& application_id,
      const std::string& sender_id,
      const content::PushMessagingService::RegisterCallback& callback,
      bool allow);

  GCMProfileService* gcm_profile_service_;  // It owns us.

  Profile* profile_;  // It owns our owner.

  base::WeakPtrFactory<PushMessagingServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingServiceImpl);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_SERVICE_IMPL_H_
