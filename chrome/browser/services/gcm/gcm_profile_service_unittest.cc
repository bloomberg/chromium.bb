// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_profile_service.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/services/gcm/fake_gcm_client_factory.h"
#include "chrome/browser/services/gcm/fake_signin_manager.h"
#include "chrome/browser/services/gcm/gcm_client_factory.h"
#include "chrome/browser/services/gcm/gcm_client_mock.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gcm/gcm_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const char kTestAccountID[] = "user@example.com";
const char kTestAppID[] = "TestApp";
const char kUserID[] = "user";

KeyedService* BuildGCMProfileService(content::BrowserContext* context) {
  return new GCMProfileService(Profile::FromBrowserContext(context));
}

}  // namespace

class GCMProfileServiceTest : public testing::Test {
 protected:
  GCMProfileServiceTest();
  virtual ~GCMProfileServiceTest();

  // testing::Test:
  virtual void SetUp() OVERRIDE;

  GCMClientMock* GetGCMClient() const;

  void RegisterAndWaitForCompletion(const std::vector<std::string>& sender_ids);
  void SendAndWaitForCompletion(const GCMClient::OutgoingMessage& message);

  void RegisterCompleted(const base::Closure& callback,
                         const std::string& registration_id,
                         GCMClient::Result result);
  void SendCompleted(const base::Closure& callback,
                     const std::string& message_id,
                     GCMClient::Result result);

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;
  GCMProfileService* gcm_profile_service_;

  std::string registration_id_;
  GCMClient::Result registration_result_;
  std::string send_message_id_;
  GCMClient::Result send_result_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GCMProfileServiceTest);
};

GCMProfileServiceTest::GCMProfileServiceTest()
    : gcm_profile_service_(NULL),
      registration_result_(GCMClient::UNKNOWN_ERROR),
      send_result_(GCMClient::UNKNOWN_ERROR) {
}

GCMProfileServiceTest::~GCMProfileServiceTest() {
}

GCMClientMock* GCMProfileServiceTest::GetGCMClient() const {
  return static_cast<GCMClientMock*>(
      gcm_profile_service_->GetGCMClientForTesting());
}

void GCMProfileServiceTest::SetUp() {
  TestingProfile::Builder builder;
  builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                            FakeSigninManager::Build);
  profile_ = builder.Build();

  gcm_profile_service_ = static_cast<GCMProfileService*>(
      GCMProfileServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile_.get(),
          &BuildGCMProfileService));
  gcm_profile_service_->Initialize(scoped_ptr<GCMClientFactory>(
      new FakeGCMClientFactory(GCMClientMock::NO_DELAY_LOADING)));

  FakeSigninManager* signin_manager = static_cast<FakeSigninManager*>(
      SigninManagerFactory::GetInstance()->GetForProfile(profile_.get()));
  signin_manager->SignIn(kTestAccountID);
  base::RunLoop().RunUntilIdle();
}

void GCMProfileServiceTest::RegisterAndWaitForCompletion(
    const std::vector<std::string>& sender_ids) {
  base::RunLoop run_loop;
  gcm_profile_service_->Register(
      kTestAppID,
      sender_ids,
      base::Bind(&GCMProfileServiceTest::RegisterCompleted,
                 base::Unretained(this),
                 run_loop.QuitClosure()));
  run_loop.Run();
}

void GCMProfileServiceTest::SendAndWaitForCompletion(
    const GCMClient::OutgoingMessage& message) {
  base::RunLoop run_loop;
  gcm_profile_service_->Send(kTestAppID,
                             kUserID,
                             message,
                             base::Bind(&GCMProfileServiceTest::SendCompleted,
                                        base::Unretained(this),
                                        run_loop.QuitClosure()));
  run_loop.Run();
}

void GCMProfileServiceTest::RegisterCompleted(
     const base::Closure& callback,
     const std::string& registration_id,
     GCMClient::Result result) {
  registration_id_ = registration_id;
  registration_result_ = result;
  callback.Run();
}

void GCMProfileServiceTest::SendCompleted(
    const base::Closure& callback,
    const std::string& message_id,
    GCMClient::Result result) {
  send_message_id_ = message_id;
  send_result_ = result;
  callback.Run();
}

TEST_F(GCMProfileServiceTest, RegisterUnderNeutralChannelSignal) {
  // GCMClient should not be checked in.
  EXPECT_FALSE(gcm_profile_service_->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::UNINITIALIZED, GetGCMClient()->status());

  // Invoking register will make GCMClient checked in.
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender");
  RegisterAndWaitForCompletion(sender_ids);

  // GCMClient should be checked in.
  EXPECT_TRUE(gcm_profile_service_->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::LOADED, GetGCMClient()->status());

  // Registration should succeed.
  std::string expected_registration_id =
      GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids);
  EXPECT_EQ(expected_registration_id, registration_id_);
  EXPECT_EQ(GCMClient::SUCCESS, registration_result_);
}

TEST_F(GCMProfileServiceTest, SendUnderNeutralChannelSignal) {
  // GCMClient should not be checked in.
  EXPECT_FALSE(gcm_profile_service_->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::UNINITIALIZED, GetGCMClient()->status());

  // Invoking send will make GCMClient checked in.
  GCMClient::OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  SendAndWaitForCompletion( message);

  // GCMClient should be checked in.
  EXPECT_TRUE(gcm_profile_service_->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::LOADED, GetGCMClient()->status());

  // Sending should succeed.
  EXPECT_EQ(message.id, send_message_id_);
  EXPECT_EQ(GCMClient::SUCCESS, send_result_);
}

}  // namespace gcm
