// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/push_messaging_service.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"
#include "chrome/browser/push_messaging/push_messaging_permission_context.h"
#include "chrome/browser/push_messaging/push_messaging_permission_context_factory.h"
#include "chrome/browser/push_messaging/push_messaging_service_factory.h"
#include "chrome/browser/push_messaging/push_messaging_service_impl.h"
#include "chrome/browser/services/gcm/fake_gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/gcm_driver/crypto/gcm_crypto_test_helpers.h"
#include "components/gcm_driver/fake_gcm_client_factory.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/push_event_payload.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestOrigin[] = "https://example.com";
const char kTestSenderId[] = "1234567890";
const int64_t kTestServiceWorkerId = 42;
const char kTestPayload[] = "Hello, world!";

// Implementation of the TestingProfile that provides the Push Messaging Service
// and the Permission Manager, both of which are required for the tests.
class PushMessagingTestingProfile : public TestingProfile {
 public:
  PushMessagingTestingProfile() {}
  ~PushMessagingTestingProfile() override {}

  PushMessagingServiceImpl* GetPushMessagingService() override {
    return PushMessagingServiceFactory::GetForProfile(this);
  }

  PermissionManager* GetPermissionManager() override {
    return PermissionManagerFactory::GetForProfile(this);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PushMessagingTestingProfile);
};

// Implementation of the PushMessagingPermissionContext that always allows usage
// of Push Messaging, regardless of the actual implementation.
class FakePushMessagingPermissionContext
    : public PushMessagingPermissionContext {
 public:
  explicit FakePushMessagingPermissionContext(Profile* profile)
      : PushMessagingPermissionContext(profile) {}
  ~FakePushMessagingPermissionContext() override {}

  ContentSetting GetPermissionStatus(
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override {
    return CONTENT_SETTING_ALLOW;
  }
};

scoped_ptr<KeyedService> BuildFakePushMessagingPermissionContext(
    content::BrowserContext* context) {
  return scoped_ptr<KeyedService>(
      new FakePushMessagingPermissionContext(static_cast<Profile*>(context)));
}

scoped_ptr<KeyedService> BuildFakeGCMProfileService(
    content::BrowserContext* context) {
  return gcm::FakeGCMProfileService::Build(static_cast<Profile*>(context));
}

}  // namespace

class PushMessagingServiceTest : public ::testing::Test {
 public:
  PushMessagingServiceTest() {
    // Override the permission context factory to always allow Push Messaging.
    PushMessagingPermissionContextFactory::GetInstance()->SetTestingFactory(
        &profile_, &BuildFakePushMessagingPermissionContext);

    // Override the GCM Profile service so that we can send fake messages.
    gcm::GCMProfileServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, &BuildFakeGCMProfileService);

    // Force-enable encrypted payloads for incoming push messages.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  ~PushMessagingServiceTest() override {}

  // Callback to use when the subscription may have been subscribed.
  void DidRegister(std::string* subscription_id_out,
                   std::vector<uint8_t>* p256dh_out,
                   std::vector<uint8_t>* auth_out,
                   const std::string& registration_id,
                   const std::vector<uint8_t>& p256dh,
                   const std::vector<uint8_t>& auth,
                   content::PushRegistrationStatus status) {
    ASSERT_EQ(content::PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE,
              status);

    *subscription_id_out = registration_id;
    *p256dh_out = p256dh;
    *auth_out = auth;
  }

  // Callback to use when observing messages dispatched by the push service.
  void DidDispatchMessage(std::string* app_id_out,
                          GURL* origin_out,
                          int64_t* service_worker_registration_id_out,
                          content::PushEventPayload* payload_out,
                          const std::string& app_id,
                          const GURL& origin,
                          int64_t service_worker_registration_id,
                          const content::PushEventPayload& payload) {
    *app_id_out = app_id;
    *origin_out = origin;
    *service_worker_registration_id_out = service_worker_registration_id;
    *payload_out = payload;
  }

 protected:
  PushMessagingTestingProfile* profile() { return &profile_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  PushMessagingTestingProfile profile_;
};

TEST_F(PushMessagingServiceTest, PayloadEncryptionTest) {
  PushMessagingServiceImpl* push_service = profile()->GetPushMessagingService();
  ASSERT_TRUE(push_service);

  const GURL origin(kTestOrigin);

  // (1) Make sure that |kExampleOrigin| has access to use Push Messaging.
  ASSERT_EQ(blink::WebPushPermissionStatusGranted,
            push_service->GetPermissionStatus(origin, origin, true));

  std::string subscription_id;
  std::vector<uint8_t> p256dh, auth;

  // (2) Subscribe for Push Messaging, and verify that we've got the required
  // information in order to be able to create encrypted messages.
  push_service->SubscribeFromWorker(
      origin, kTestServiceWorkerId, kTestSenderId, true /* user_visible */,
      base::Bind(&PushMessagingServiceTest::DidRegister, base::Unretained(this),
                 &subscription_id, &p256dh, &auth));

  EXPECT_EQ(0u, subscription_id.size());  // this must be asynchronous

  base::RunLoop().RunUntilIdle();

  ASSERT_GT(subscription_id.size(), 0u);
  ASSERT_GT(p256dh.size(), 0u);
  ASSERT_GT(auth.size(), 0u);

  // (3) Encrypt a message using the public key and authentication secret that
  // are associated with the subscription.

  gcm::IncomingMessage message;
  message.sender_id = kTestSenderId;

  ASSERT_TRUE(gcm::CreateEncryptedPayloadForTesting(
      kTestPayload,
      base::StringPiece(reinterpret_cast<char*>(p256dh.data()), p256dh.size()),
      base::StringPiece(reinterpret_cast<char*>(auth.data()), auth.size()),
      &message));

  ASSERT_GT(message.raw_data.size(), 0u);
  ASSERT_NE(kTestPayload, message.raw_data);
  ASSERT_FALSE(message.decrypted);

  // (4) Find the app_id that has been associated with the subscription.
  PushMessagingAppIdentifier app_identifier =
      PushMessagingAppIdentifier::FindByServiceWorker(profile(), origin,
                                                      kTestServiceWorkerId);

  ASSERT_FALSE(app_identifier.is_null());

  std::string app_id;
  GURL dispatched_origin;
  int64_t service_worker_registration_id;
  content::PushEventPayload payload;

  // (5) Observe message dispatchings from the Push Messaging service, and then
  // dispatch the |message| on the GCM driver as if it had actually been
  // received by Google Cloud Messaging.
  push_service->SetMessageDispatchedCallbackForTesting(base::Bind(
      &PushMessagingServiceTest::DidDispatchMessage, base::Unretained(this),
      &app_id, &dispatched_origin, &service_worker_registration_id, &payload));

  gcm::FakeGCMProfileService* fake_profile_service =
      static_cast<gcm::FakeGCMProfileService*>(
          gcm::GCMProfileServiceFactory::GetForProfile(profile()));

  fake_profile_service->DispatchMessage(app_identifier.app_id(), message);

  base::RunLoop().RunUntilIdle();

  // (6) Verify that the message, as received by the Push Messaging Service, has
  // indeed been decrypted by the GCM Driver, and has been forwarded to the
  // Service Worker that has been associated with the subscription.
  EXPECT_EQ(app_identifier.app_id(), app_id);
  EXPECT_EQ(origin, dispatched_origin);
  EXPECT_EQ(service_worker_registration_id, kTestServiceWorkerId);

  EXPECT_FALSE(payload.is_null);
  EXPECT_EQ(kTestPayload, payload.data);
}
