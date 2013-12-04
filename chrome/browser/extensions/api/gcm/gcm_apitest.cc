// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/extensions/api/gcm/gcm_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/fake_gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/test/base/ui_test_utils.h"

namespace {

const char kFunctionsTestExtension[] = "gcm/functions";
const char kEventsExtension[] = "gcm/events";

}  // namespace

namespace extensions {

class GcmApiTest : public ExtensionApiTest {
 public:
  GcmApiTest() : fake_gcm_profile_service_(NULL) {}

 protected:
  void SetUpFakeService(bool collect);

  const Extension* LoadTestExtension(const std::string& extension_path,
                                     const std::string& page_name);

  void WaitUntilIdle();

  gcm::FakeGCMProfileService* service() const;

  gcm::FakeGCMProfileService* fake_gcm_profile_service_;
};

void GcmApiTest::SetUpFakeService(bool collect) {
  gcm::GCMProfileServiceFactory::GetInstance()->SetTestingFactory(
      profile(), &gcm::FakeGCMProfileService::Build);

  fake_gcm_profile_service_ = static_cast<gcm::FakeGCMProfileService*>(
      gcm::GCMProfileServiceFactory::GetInstance()->GetForProfile(profile()));
  fake_gcm_profile_service_->set_collect(collect);
  gcm::FakeGCMProfileService::EnableGCMForTesting();
}

void GcmApiTest::WaitUntilIdle() {
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

gcm::FakeGCMProfileService* GcmApiTest::service() const {
  return fake_gcm_profile_service_;
}

const Extension* GcmApiTest::LoadTestExtension(
    const std::string& extension_path,
    const std::string& page_name) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(extension_path));
  if (extension) {
    ui_test_utils::NavigateToURL(
        browser(), extension->GetResourceURL(page_name));
  }
  return extension;
}

// http://crbug.com/177163 and http://crbug.com/324982
#if defined(OS_WIN)
#define MAYBE_RegisterValidation DISABLED_RegisterValidation
#else
#define MAYBE_RegisterValidation RegisterValidation
#endif
IN_PROC_BROWSER_TEST_F(GcmApiTest, MAYBE_RegisterValidation) {
  SetUpFakeService(false);
  EXPECT_TRUE(RunExtensionSubtest(kFunctionsTestExtension,
                                  "register_validation.html"));
}

// http://crbug.com/177163 and http://crbug.com/324982
#if defined(OS_WIN)
#define MAYBE_Register DISABLED_Register
#else
#define MAYBE_Register Register
#endif
IN_PROC_BROWSER_TEST_F(GcmApiTest, MAYBE_Register) {
  SetUpFakeService(true);
  const extensions::Extension* extension =
      LoadTestExtension(kFunctionsTestExtension, "register.html");
  ASSERT_TRUE(extension);

  WaitUntilIdle();

  EXPECT_EQ(extension->id(), service()->last_registered_app_id());
  // SHA1 of the public key provided in manifest.json.
  EXPECT_EQ("26469186F238EE08FA71C38311C6990F61D40DCA",
            service()->last_registered_cert());
  const std::vector<std::string>& sender_ids =
      service()->last_registered_sender_ids();
  EXPECT_TRUE(std::find(sender_ids.begin(), sender_ids.end(), "Sender1") !=
                  sender_ids.end());
  EXPECT_TRUE(std::find(sender_ids.begin(), sender_ids.end(), "Sender2") !=
                  sender_ids.end());
}

// http://crbug.com/177163 and http://crbug.com/324982
#if defined(OS_WIN)
#define MAYBE_SendValidation DISABLED_SendValidation
#else
#define MAYBE_SendValidation SendValidation
#endif
IN_PROC_BROWSER_TEST_F(GcmApiTest, MAYBE_SendValidation) {
  SetUpFakeService(false);
  EXPECT_TRUE(RunExtensionSubtest(kFunctionsTestExtension, "send.html"));
}

// http://crbug.com/177163 and http://crbug.com/324982
#if defined(OS_WIN)
#define MAYBE_SendMessageData DISABLED_SendMessageData
#else
#define MAYBE_SendMessageData SendMessageData
#endif
IN_PROC_BROWSER_TEST_F(GcmApiTest, MAYBE_SendMessageData) {
  SetUpFakeService(true);
  const extensions::Extension* extension =
      LoadTestExtension(kFunctionsTestExtension, "send_message_data.html");
  ASSERT_TRUE(extension);

  WaitUntilIdle();

  EXPECT_EQ("destination-id", service()->last_receiver_id());
  const gcm::GCMClient::OutgoingMessage& message =
      service()->last_sent_message();
  gcm::GCMClient::MessageData::const_iterator iter;

  EXPECT_TRUE((iter = message.data.find("key1")) != message.data.end());
  EXPECT_EQ("value1", iter->second);

  EXPECT_TRUE((iter = message.data.find("key2")) != message.data.end());
  EXPECT_EQ("value2", iter->second);
}

// http://crbug.com/177163 and http://crbug/324982
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_OnMessagesDeleted DISABLED_OnMessagesDeleted
#else
#define MAYBE_OnMessagesDeleted OnMessagesDeleted
#endif
IN_PROC_BROWSER_TEST_F(GcmApiTest, MAYBE_OnMessagesDeleted) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(profile());

  const extensions::Extension* extension =
      LoadTestExtension(kEventsExtension, "on_messages_deleted.html");
  ASSERT_TRUE(extension);

  GcmJsEventRouter router(profile());
  router.OnMessagesDeleted(extension->id());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// http://crbug.com/177163 and http://crbug/324982
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_OnMessage DISABLED_OnMessage
#else
#define MAYBE_OnMessage OnMessage
#endif
IN_PROC_BROWSER_TEST_F(GcmApiTest, MAYBE_OnMessage) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(profile());

  const extensions::Extension* extension =
      LoadTestExtension(kEventsExtension, "on_message.html");
  ASSERT_TRUE(extension);

  GcmJsEventRouter router(profile());

  gcm::GCMClient::IncomingMessage message;
  message.data["property1"] = "value1";
  message.data["property2"] = "value2";
  router.OnMessage(extension->id(), message);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// http://crbug.com/177163 and http://crbug/324982
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_OnSendError DISABLED_OnSendError
#else
#define MAYBE_OnSendError OnSendError
#endif
IN_PROC_BROWSER_TEST_F(GcmApiTest, MAYBE_OnSendError) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(profile());

  const extensions::Extension* extension =
      LoadTestExtension(kEventsExtension, "on_send_error.html");
  ASSERT_TRUE(extension);

  GcmJsEventRouter router(profile());
  router.OnSendError(extension->id(), "error_message_1",
      gcm::GCMClient::ASYNC_OPERATION_PENDING);
  router.OnSendError(extension->id(), "error_message_2",
      gcm::GCMClient::SERVER_ERROR);
  router.OnSendError(extension->id(), "error_message_3",
      gcm::GCMClient::NETWORK_ERROR);
  router.OnSendError(extension->id(), "error_message_4",
      gcm::GCMClient::UNKNOWN_ERROR);
  router.OnSendError(extension->id(), "error_message_5",
      gcm::GCMClient::TTL_EXCEEDED);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
