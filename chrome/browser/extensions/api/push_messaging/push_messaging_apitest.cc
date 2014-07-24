// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/push_messaging/push_messaging_api.h"

#include "apps/launcher.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/push_messaging/push_messaging_invalidation_handler.h"
#include "chrome/browser/extensions/api/push_messaging/push_messaging_invalidation_mapper.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/invalidation/fake_invalidation_service.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/invalidation/fake_invalidator.h"
#include "components/invalidation/invalidation.h"
#include "components/invalidation/invalidation_service.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/keyed_service/core/keyed_service.h"
#include "google/cacheinvalidation/types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::_;

namespace content {
class BrowserContext;
}

namespace extensions {

namespace {

invalidation::ObjectId ExtensionAndSubchannelToObjectId(
    const std::string& extension_id, int subchannel_id) {
  return invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_PUSH_MESSAGING,
      base::StringPrintf("U/%s/%d", extension_id.c_str(), subchannel_id));
}

KeyedService* BuildFakeProfileInvalidationProvider(
    content::BrowserContext* context) {
  return new invalidation::ProfileInvalidationProvider(
      scoped_ptr<invalidation::InvalidationService>(
          new invalidation::FakeInvalidationService));
}

class MockInvalidationMapper : public PushMessagingInvalidationMapper {
 public:
  MockInvalidationMapper();
  ~MockInvalidationMapper();

  MOCK_METHOD1(SuppressInitialInvalidationsForExtension,
               void(const std::string&));
  MOCK_METHOD1(RegisterExtension, void(const std::string&));
  MOCK_METHOD1(UnregisterExtension, void(const std::string&));
};

MockInvalidationMapper::MockInvalidationMapper() {}
MockInvalidationMapper::~MockInvalidationMapper() {}

}  // namespace

class PushMessagingApiTest : public ExtensionApiTest {
 public:
  PushMessagingApiTest()
      : fake_invalidation_service_(NULL) {
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

  virtual void SetUp() OVERRIDE {
    invalidation::ProfileInvalidationProviderFactory::GetInstance()->
        RegisterTestingFactory(BuildFakeProfileInvalidationProvider);
    ExtensionApiTest::SetUp();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionApiTest::SetUpOnMainThread();
    fake_invalidation_service_ =
        static_cast<invalidation::FakeInvalidationService*>(
            static_cast<invalidation::ProfileInvalidationProvider*>(
                invalidation::ProfileInvalidationProviderFactory::
                    GetInstance()->GetForProfile(profile()))->
                        GetInvalidationService());
  }

  void EmitInvalidation(
      const invalidation::ObjectId& object_id,
      int64 version,
      const std::string& payload) {
    fake_invalidation_service_->EmitInvalidationForTest(
        syncer::Invalidation::Init(object_id, version, payload));
  }

  PushMessagingAPI* GetAPI() {
    return PushMessagingAPI::Get(profile());
  }

  PushMessagingEventRouter* GetEventRouter() {
    return PushMessagingAPI::Get(profile())->GetEventRouterForTest();
  }

  invalidation::FakeInvalidationService* fake_invalidation_service_;
};

IN_PROC_BROWSER_TEST_F(PushMessagingApiTest, EventDispatch) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(profile());

  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("push_messaging"));
  ASSERT_TRUE(extension);
  ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("event_dispatch.html"));

  GetEventRouter()->TriggerMessageForTest(extension->id(), 1, "payload");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Test that a push introduced into the sync code makes it to the extension
// that we install.
IN_PROC_BROWSER_TEST_F(PushMessagingApiTest, ReceivesPush) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(profile());

  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("push_messaging"));
  ASSERT_TRUE(extension);
  ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("event_dispatch.html"));

  // PushMessagingInvalidationHandler suppresses the initial invalidation on
  // each subchannel at install, so trigger the suppressions first.
  for (int i = 0; i < 3; ++i) {
    EmitInvalidation(
        ExtensionAndSubchannelToObjectId(extension->id(), i), i, std::string());
  }

  EmitInvalidation(
      ExtensionAndSubchannelToObjectId(extension->id(), 1), 5, "payload");
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

  std::string extension_id1;
  std::string extension_id2;
  EXPECT_CALL(*unsafe_mapper, SuppressInitialInvalidationsForExtension(_))
      .WillOnce(SaveArg<0>(&extension_id1));
  EXPECT_CALL(*unsafe_mapper, RegisterExtension(_))
      .WillOnce(SaveArg<0>(&extension_id2));
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("push_messaging"));
  ASSERT_TRUE(extension);
  EXPECT_EQ(extension->id(), extension_id1);
  EXPECT_EQ(extension->id(), extension_id2);
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
  catcher.RestrictToProfile(profile());

  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("push_messaging"));
  ASSERT_TRUE(extension);
  ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("get_channel_id.html"));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
