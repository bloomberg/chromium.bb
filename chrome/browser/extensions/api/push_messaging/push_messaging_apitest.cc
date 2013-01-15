// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/push_messaging/push_messaging_api.h"
#include "chrome/browser/extensions/api/push_messaging/push_messaging_invalidation_handler.h"
#include "chrome/browser/extensions/api/push_messaging/push_messaging_invalidation_mapper.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/platform_app_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "google/cacheinvalidation/types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace extensions {

class MockInvalidationMapper : public PushMessagingInvalidationMapper {
 public:
  MockInvalidationMapper();
  ~MockInvalidationMapper();

  MOCK_METHOD1(RegisterExtension, void(const std::string&));
  MOCK_METHOD1(UnregisterExtension, void(const std::string&));
};

MockInvalidationMapper::MockInvalidationMapper() {}
MockInvalidationMapper::~MockInvalidationMapper() {}

class PushMessagingApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

  PushMessagingAPI* GetAPI() {
    return PushMessagingAPI::Get(browser()->profile());
  }

  PushMessagingEventRouter* GetEventRouter() {
    return PushMessagingAPI::Get(browser()->profile())->GetEventRouterForTest();
  }
};

IN_PROC_BROWSER_TEST_F(PushMessagingApiTest, EventDispatch) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());
  ExtensionTestMessageListener ready("ready", true);

  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("push_messaging"));
  ASSERT_TRUE(extension);
  ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("event_dispatch.html"));
  EXPECT_TRUE(ready.WaitUntilSatisfied());

  GetEventRouter()->TriggerMessageForTest(extension->id(), 1, "payload");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Test that a push introduced into the sync code makes it to the extension
// that we install.
IN_PROC_BROWSER_TEST_F(PushMessagingApiTest, ReceivesPush) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());
  ExtensionTestMessageListener ready("ready", true);

  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("push_messaging"));
  ASSERT_TRUE(extension);
  ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("event_dispatch.html"));
  EXPECT_TRUE(ready.WaitUntilSatisfied());

  ProfileSyncService* pss = ProfileSyncServiceFactory::GetForProfile(
      browser()->profile());
  ASSERT_TRUE(pss);

  // Construct a sync id for the object "U/<extension-id>/1".
  std::string id = "U/";
  id += extension->id();
  id += "/1";

  invalidation::ObjectId object_id(
      ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING, id);

  pss->EmitInvalidationForTest(object_id, "payload");
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Checks that an extension with the pushMessaging permission gets automatically
// registered for invalidations when it is loaded.
IN_PROC_BROWSER_TEST_F(PushMessagingApiTest, AutoRegistration) {
  scoped_ptr<StrictMock<MockInvalidationMapper> > mapper(
      new StrictMock<MockInvalidationMapper>);
  StrictMock<MockInvalidationMapper>* unsafe_mapper = mapper.get();
  // PushMessagingEventRouter owns the mapper now.
  GetAPI()->SetMapperForTest(
      mapper.PassAs<PushMessagingInvalidationMapper>());

  std::string extension_id;
  EXPECT_CALL(*unsafe_mapper, RegisterExtension(_))
      .WillOnce(SaveArg<0>(&extension_id));
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("push_messaging"));
  ASSERT_TRUE(extension);
  EXPECT_EQ(extension->id(), extension_id);
  EXPECT_CALL(*unsafe_mapper, UnregisterExtension(extension->id()));
  UnloadExtension(extension->id());
}

// Tests that we re-register for invalidations on restart for extensions that
// are already installed.
IN_PROC_BROWSER_TEST_F(PushMessagingApiTest, PRE_Restart) {
  PushMessagingInvalidationHandler* handler =
      static_cast<PushMessagingInvalidationHandler*>(
          GetAPI()->GetMapperForTest());
  EXPECT_TRUE(handler->GetRegisteredExtensionsForTest().empty());
  ASSERT_TRUE(InstallExtension(test_data_dir_.AppendASCII("push_messaging"),
                               1 /* new install */));
}

IN_PROC_BROWSER_TEST_F(PushMessagingApiTest, Restart) {
  PushMessagingInvalidationHandler* handler =
      static_cast<PushMessagingInvalidationHandler*>(
          GetAPI()->GetMapperForTest());
  EXPECT_EQ(1U, handler->GetRegisteredExtensionsForTest().size());
}

// Test that GetChannelId fails if no user is signed in.
IN_PROC_BROWSER_TEST_F(PushMessagingApiTest, GetChannelId) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("push_messaging"));
  ASSERT_TRUE(extension);
  ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("get_channel_id.html"));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
