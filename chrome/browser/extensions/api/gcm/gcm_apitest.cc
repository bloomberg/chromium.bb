// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/extensions/api/gcm/gcm_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/fake_gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_client_factory.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "chrome/test/base/ui_test_utils.h"

namespace {

const char kEventsExtension[] = "gcm/events";

}  // namespace

namespace extensions {

class GcmApiTest : public ExtensionApiTest {
 public:
  GcmApiTest() : fake_gcm_profile_service_(NULL) {}

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;
  virtual void SetUpOnMainThread() OVERRIDE;

  void StartCollecting();

  const Extension* LoadTestExtension(const std::string& extension_path,
                                     const std::string& page_name);
  gcm::FakeGCMProfileService* service() const;
  bool ShouldSkipTest() const;

 private:
  gcm::FakeGCMProfileService* fake_gcm_profile_service_;
};

void GcmApiTest::SetUpCommandLine(CommandLine* command_line) {
  // We now always create the GCMProfileService instance in
  // ProfileSyncServiceFactory that is called when a profile is being
  // initialized. In order to prevent it from being created, we add the switch
  // to disable the sync logic.
  command_line->AppendSwitch(switches::kDisableSync);

  ExtensionApiTest::SetUpCommandLine(command_line);
}

void GcmApiTest::SetUpOnMainThread() {
  gcm::GCMProfileServiceFactory::GetInstance()->SetTestingFactory(
      browser()->profile(), &gcm::FakeGCMProfileService::Build);
  fake_gcm_profile_service_ = static_cast<gcm::FakeGCMProfileService*>(
      gcm::GCMProfileServiceFactory::GetInstance()->GetForProfile(
          browser()->profile()));

  ExtensionApiTest::SetUpOnMainThread();
}

void GcmApiTest::StartCollecting() {
  service()->set_collect(true);
}

gcm::FakeGCMProfileService* GcmApiTest::service() const {
  return fake_gcm_profile_service_;
}

const Extension* GcmApiTest::LoadTestExtension(
    const std::string& extension_path,
    const std::string& page_name) {
  // TODO(jianli): Once the GCM API enters stable, remove |channel|.
  ScopedCurrentChannel channel(chrome::VersionInfo::CHANNEL_UNKNOWN);

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(extension_path));
  if (extension) {
    ui_test_utils::NavigateToURL(
        browser(), extension->GetResourceURL(page_name));
  }
  return extension;
}

bool GcmApiTest::ShouldSkipTest() const {
  // TODO(jianli): Remove this once the GCM API enters stable.
  return chrome::VersionInfo::GetChannel() ==
      chrome::VersionInfo::CHANNEL_STABLE;
}

IN_PROC_BROWSER_TEST_F(GcmApiTest, RegisterValidation) {
  if (ShouldSkipTest())
    return;

  ASSERT_TRUE(RunExtensionTest("gcm/functions/register_validation"));
}

IN_PROC_BROWSER_TEST_F(GcmApiTest, Register) {
  if (ShouldSkipTest())
    return;

  StartCollecting();
  ASSERT_TRUE(RunExtensionTest("gcm/functions/register"));

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

IN_PROC_BROWSER_TEST_F(GcmApiTest, RegisterWithoutKey) {
  if (ShouldSkipTest())
    return;

  ASSERT_TRUE(RunExtensionTest("gcm/functions/register_without_key"));
}

IN_PROC_BROWSER_TEST_F(GcmApiTest, SendValidation) {
  if (ShouldSkipTest())
    return;

  ASSERT_TRUE(RunExtensionTest("gcm/functions/send"));
}

IN_PROC_BROWSER_TEST_F(GcmApiTest, SendMessageData) {
  if (ShouldSkipTest())
    return;

  StartCollecting();
  ASSERT_TRUE(RunExtensionTest("gcm/functions/send_message_data"));

  EXPECT_EQ("destination-id", service()->last_receiver_id());
  const gcm::GCMClient::OutgoingMessage& message =
      service()->last_sent_message();
  gcm::GCMClient::MessageData::const_iterator iter;

  EXPECT_TRUE((iter = message.data.find("key1")) != message.data.end());
  EXPECT_EQ("value1", iter->second);

  EXPECT_TRUE((iter = message.data.find("key2")) != message.data.end());
  EXPECT_EQ("value2", iter->second);
}

IN_PROC_BROWSER_TEST_F(GcmApiTest, OnMessagesDeleted) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(profile());

  const extensions::Extension* extension =
      LoadTestExtension(kEventsExtension, "on_messages_deleted.html");
  ASSERT_TRUE(extension);

  GcmJsEventRouter router(profile());
  router.OnMessagesDeleted(extension->id());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(GcmApiTest, OnMessage) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(profile());

  const extensions::Extension* extension =
      LoadTestExtension(kEventsExtension, "on_message.html");
  ASSERT_TRUE(extension);

  GcmJsEventRouter router(profile());

  gcm::GCMClient::IncomingMessage message;
  message.data["property1"] = "value1";
  message.data["property2"] = "value2";
  // First message is sent without a collapse key.
  router.OnMessage(extension->id(), message);

  // Second message carries the same data and a collapse key.
  message.collapse_key = "collapseKeyValue";
  router.OnMessage(extension->id(), message);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(GcmApiTest, OnSendError) {
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

IN_PROC_BROWSER_TEST_F(GcmApiTest, Incognito) {
  if (ShouldSkipTest())
    return;

  ResultCatcher catcher;
  catcher.RestrictToProfile(profile());
  ResultCatcher incognito_catcher;
  incognito_catcher.RestrictToProfile(profile()->GetOffTheRecordProfile());

  ASSERT_TRUE(RunExtensionTestIncognito("gcm/functions/incognito"));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(incognito_catcher.GetNextResult()) << incognito_catcher.message();
}

}  // namespace extensions
