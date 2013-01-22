// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "chrome/browser/extensions/api/storage/settings_frontend.h"
#include "chrome/browser/extensions/api/storage/settings_namespace.h"
#include "chrome/browser/extensions/api/storage/settings_sync_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/value_builder.h"
#include "chrome/test/base/ui_test_utils.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_error_factory_mock.h"
#include "testing/gmock/include/gmock/gmock.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_map.h"
#endif

namespace extensions {

using settings_namespace::FromString;
using settings_namespace::LOCAL;
using settings_namespace::MANAGED;
using settings_namespace::Namespace;
using settings_namespace::SYNC;
using settings_namespace::ToString;
using testing::Return;

namespace {

// TODO(kalman): test both EXTENSION_SETTINGS and APP_SETTINGS.
const syncer::ModelType kModelType = syncer::EXTENSION_SETTINGS;

// The managed_storage extension has a key defined in its manifest, so that
// its extension ID is well-known and the policy system can push policies for
// the extension.
const char kManagedStorageExtensionId[] = "kjmkgkdkpedkejedfhmfcenooemhbpbo";

class NoopSyncChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE {
    return syncer::SyncError();
  }

  virtual ~NoopSyncChangeProcessor() {};
};

class SyncChangeProcessorDelegate : public syncer::SyncChangeProcessor {
 public:
  explicit SyncChangeProcessorDelegate(syncer::SyncChangeProcessor* recipient)
      : recipient_(recipient) {
    DCHECK(recipient_);
  }
  virtual ~SyncChangeProcessorDelegate() {}

  // syncer::SyncChangeProcessor implementation.
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE {
    return recipient_->ProcessSyncChanges(from_here, change_list);
  }

 private:
  // The recipient of all sync changes.
  syncer::SyncChangeProcessor* recipient_;

  DISALLOW_COPY_AND_ASSIGN(SyncChangeProcessorDelegate);
};

}  // namespace

class ExtensionSettingsApiTest : public ExtensionApiTest {
 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();

#if defined(ENABLE_CONFIGURATION_POLICY)
    EXPECT_CALL(policy_provider_, IsInitializationComplete())
        .WillRepeatedly(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
#endif
  }

  void ReplyWhenSatisfied(
      Namespace settings_namespace,
      const std::string& normal_action,
      const std::string& incognito_action) {
    MaybeLoadAndReplyWhenSatisfied(
        settings_namespace, normal_action, incognito_action, NULL, false);
  }

  const Extension* LoadAndReplyWhenSatisfied(
      Namespace settings_namespace,
      const std::string& normal_action,
      const std::string& incognito_action,
      const std::string& extension_dir) {
    return MaybeLoadAndReplyWhenSatisfied(
        settings_namespace,
        normal_action,
        incognito_action,
        &extension_dir,
        false);
  }

  void FinalReplyWhenSatisfied(
      Namespace settings_namespace,
      const std::string& normal_action,
      const std::string& incognito_action) {
    MaybeLoadAndReplyWhenSatisfied(
        settings_namespace, normal_action, incognito_action, NULL, true);
  }

  void InitSync(syncer::SyncChangeProcessor* sync_processor) {
    MessageLoop::current()->RunUntilIdle();
    InitSyncWithSyncableService(
        sync_processor,
        browser()->profile()->GetExtensionService()->settings_frontend()->
              GetBackendForSync(kModelType));
  }

  void SendChanges(const syncer::SyncChangeList& change_list) {
    MessageLoop::current()->RunUntilIdle();
    SendChangesToSyncableService(
        change_list,
        browser()->profile()->GetExtensionService()->settings_frontend()->
              GetBackendForSync(kModelType));
  }

#if defined(ENABLE_CONFIGURATION_POLICY)
  void SetPolicies(const base::DictionaryValue& policies) {
    scoped_ptr<policy::PolicyBundle> bundle(new policy::PolicyBundle());
    policy::PolicyMap& policy_map = bundle->Get(
        policy::POLICY_DOMAIN_EXTENSIONS, kManagedStorageExtensionId);
    policy_map.LoadFrom(
        &policies, policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER);
    policy_provider_.UpdatePolicy(bundle.Pass());
  }
#endif

 private:
  const Extension* MaybeLoadAndReplyWhenSatisfied(
      Namespace settings_namespace,
      const std::string& normal_action,
      const std::string& incognito_action,
      // May be NULL to imply not loading the extension.
      const std::string* extension_dir,
      bool is_final_action) {
    ExtensionTestMessageListener listener("waiting", true);
    ExtensionTestMessageListener listener_incognito("waiting_incognito", true);

    // Only load the extension after the listeners have been set up, to avoid
    // initialisation race conditions.
    const Extension* extension = NULL;
    if (extension_dir) {
      extension = LoadExtensionIncognito(
          test_data_dir_.AppendASCII("settings").AppendASCII(*extension_dir));
      EXPECT_TRUE(extension);
    }

    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_TRUE(listener_incognito.WaitUntilSatisfied());

    listener.Reply(
        CreateMessage(settings_namespace, normal_action, is_final_action));
    listener_incognito.Reply(
        CreateMessage(settings_namespace, incognito_action, is_final_action));
    return extension;
  }

  std::string CreateMessage(
      Namespace settings_namespace,
      const std::string& action,
      bool is_final_action) {
    scoped_ptr<DictionaryValue> message(new DictionaryValue());
    message->SetString("namespace", ToString(settings_namespace));
    message->SetString("action", action);
    message->SetBoolean("isFinalAction", is_final_action);
    std::string message_json;
    base::JSONWriter::Write(message.get(), &message_json);
    return message_json;
  }

  void InitSyncWithSyncableService(
      syncer::SyncChangeProcessor* sync_processor,
      syncer::SyncableService* settings_service) {
    EXPECT_FALSE(settings_service->MergeDataAndStartSyncing(
        kModelType,
        syncer::SyncDataList(),
        scoped_ptr<syncer::SyncChangeProcessor>(
            new SyncChangeProcessorDelegate(sync_processor)),
        scoped_ptr<syncer::SyncErrorFactory>(
            new syncer::SyncErrorFactoryMock())).error().IsSet());
  }

  void SendChangesToSyncableService(
      const syncer::SyncChangeList& change_list,
      syncer::SyncableService* settings_service) {
    EXPECT_FALSE(
        settings_service->ProcessSyncChanges(FROM_HERE, change_list).IsSet());
  }

 protected:
#if defined(ENABLE_CONFIGURATION_POLICY)
  policy::MockConfigurationPolicyProvider policy_provider_;
#endif
};

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, SimpleTest) {
  ASSERT_TRUE(RunExtensionTest("settings/simple_test")) << message_;
}

// Structure of this test taken from IncognitoSplitMode.
// Note that only split-mode incognito is tested, because spanning mode
// incognito looks the same as normal mode when the only API activity comes
// from background pages.
IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, SplitModeIncognito) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher, catcher_incognito;
  catcher.RestrictToProfile(browser()->profile());
  catcher_incognito.RestrictToProfile(
      browser()->profile()->GetOffTheRecordProfile());

  LoadAndReplyWhenSatisfied(SYNC,
      "assertEmpty", "assertEmpty", "split_incognito");
  ReplyWhenSatisfied(SYNC, "noop", "setFoo");
  ReplyWhenSatisfied(SYNC, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(SYNC, "clear", "noop");
  ReplyWhenSatisfied(SYNC, "assertEmpty", "assertEmpty");
  ReplyWhenSatisfied(SYNC, "setFoo", "noop");
  ReplyWhenSatisfied(SYNC, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(SYNC, "noop", "removeFoo");
  FinalReplyWhenSatisfied(SYNC, "assertEmpty", "assertEmpty");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest,
    OnChangedNotificationsBetweenBackgroundPages) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher, catcher_incognito;
  catcher.RestrictToProfile(browser()->profile());
  catcher_incognito.RestrictToProfile(
      browser()->profile()->GetOffTheRecordProfile());

  LoadAndReplyWhenSatisfied(SYNC,
      "assertNoNotifications", "assertNoNotifications", "split_incognito");
  ReplyWhenSatisfied(SYNC, "noop", "setFoo");
  ReplyWhenSatisfied(SYNC,
      "assertAddFooNotification", "assertAddFooNotification");
  ReplyWhenSatisfied(SYNC, "clearNotifications", "clearNotifications");
  ReplyWhenSatisfied(SYNC, "removeFoo", "noop");
  FinalReplyWhenSatisfied(SYNC,
      "assertDeleteFooNotification", "assertDeleteFooNotification");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest,
    SyncAndLocalAreasAreSeparate) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher, catcher_incognito;
  catcher.RestrictToProfile(browser()->profile());
  catcher_incognito.RestrictToProfile(
      browser()->profile()->GetOffTheRecordProfile());

  LoadAndReplyWhenSatisfied(SYNC,
      "assertNoNotifications", "assertNoNotifications", "split_incognito");

  ReplyWhenSatisfied(SYNC, "noop", "setFoo");
  ReplyWhenSatisfied(SYNC, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(SYNC,
      "assertAddFooNotification", "assertAddFooNotification");
  ReplyWhenSatisfied(LOCAL, "assertEmpty", "assertEmpty");
  ReplyWhenSatisfied(LOCAL, "assertNoNotifications", "assertNoNotifications");

  ReplyWhenSatisfied(SYNC, "clearNotifications", "clearNotifications");

  ReplyWhenSatisfied(LOCAL, "setFoo", "noop");
  ReplyWhenSatisfied(LOCAL, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(LOCAL,
      "assertAddFooNotification", "assertAddFooNotification");
  ReplyWhenSatisfied(SYNC, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(SYNC, "assertNoNotifications", "assertNoNotifications");

  ReplyWhenSatisfied(LOCAL, "clearNotifications", "clearNotifications");

  ReplyWhenSatisfied(LOCAL, "noop", "removeFoo");
  ReplyWhenSatisfied(LOCAL, "assertEmpty", "assertEmpty");
  ReplyWhenSatisfied(LOCAL,
      "assertDeleteFooNotification", "assertDeleteFooNotification");
  ReplyWhenSatisfied(SYNC, "assertFoo", "assertFoo");
  ReplyWhenSatisfied(SYNC, "assertNoNotifications", "assertNoNotifications");

  ReplyWhenSatisfied(LOCAL, "clearNotifications", "clearNotifications");

  ReplyWhenSatisfied(SYNC, "removeFoo", "noop");
  ReplyWhenSatisfied(SYNC, "assertEmpty", "assertEmpty");
  ReplyWhenSatisfied(SYNC,
      "assertDeleteFooNotification", "assertDeleteFooNotification");
  ReplyWhenSatisfied(LOCAL, "assertNoNotifications", "assertNoNotifications");
  FinalReplyWhenSatisfied(LOCAL, "assertEmpty", "assertEmpty");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

// Disabled, see crbug.com/101110
IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest,
    DISABLED_OnChangedNotificationsFromSync) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher, catcher_incognito;
  catcher.RestrictToProfile(browser()->profile());
  catcher_incognito.RestrictToProfile(
      browser()->profile()->GetOffTheRecordProfile());

  const Extension* extension =
      LoadAndReplyWhenSatisfied(SYNC,
          "assertNoNotifications", "assertNoNotifications", "split_incognito");
  const std::string& extension_id = extension->id();

  NoopSyncChangeProcessor sync_processor;
  InitSync(&sync_processor);

  // Set "foo" to "bar" via sync.
  syncer::SyncChangeList sync_changes;
  StringValue bar("bar");
  sync_changes.push_back(settings_sync_util::CreateAdd(
      extension_id, "foo", bar, kModelType));
  SendChanges(sync_changes);

  ReplyWhenSatisfied(SYNC,
      "assertAddFooNotification", "assertAddFooNotification");
  ReplyWhenSatisfied(SYNC, "clearNotifications", "clearNotifications");

  // Remove "foo" via sync.
  sync_changes.clear();
  sync_changes.push_back(settings_sync_util::CreateDelete(
      extension_id, "foo", kModelType));
  SendChanges(sync_changes);

  FinalReplyWhenSatisfied(SYNC,
      "assertDeleteFooNotification", "assertDeleteFooNotification");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

// Disabled, see crbug.com/101110
//
// TODO: boring test, already done in the unit tests.  What we really should be
// be testing is that the areas don't overlap.
IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest,
    DISABLED_OnChangedNotificationsFromSyncNotSentToLocal) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher, catcher_incognito;
  catcher.RestrictToProfile(browser()->profile());
  catcher_incognito.RestrictToProfile(
      browser()->profile()->GetOffTheRecordProfile());

  const Extension* extension =
      LoadAndReplyWhenSatisfied(LOCAL,
          "assertNoNotifications", "assertNoNotifications", "split_incognito");
  const std::string& extension_id = extension->id();

  NoopSyncChangeProcessor sync_processor;
  InitSync(&sync_processor);

  // Set "foo" to "bar" via sync.
  syncer::SyncChangeList sync_changes;
  StringValue bar("bar");
  sync_changes.push_back(settings_sync_util::CreateAdd(
      extension_id, "foo", bar, kModelType));
  SendChanges(sync_changes);

  ReplyWhenSatisfied(LOCAL, "assertNoNotifications", "assertNoNotifications");

  // Remove "foo" via sync.
  sync_changes.clear();
  sync_changes.push_back(settings_sync_util::CreateDelete(
      extension_id, "foo", kModelType));
  SendChanges(sync_changes);

  FinalReplyWhenSatisfied(LOCAL,
      "assertNoNotifications", "assertNoNotifications");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, IsStorageEnabled) {
  SettingsFrontend* frontend =
      browser()->profile()->GetExtensionService()->settings_frontend();
  EXPECT_TRUE(frontend->IsStorageEnabled(LOCAL));
  EXPECT_TRUE(frontend->IsStorageEnabled(SYNC));

#if defined(ENABLE_CONFIGURATION_POLICY)
  EXPECT_TRUE(frontend->IsStorageEnabled(MANAGED));
#else
  EXPECT_FALSE(frontend->IsStorageEnabled(MANAGED));
#endif
}

#if defined(ENABLE_CONFIGURATION_POLICY)

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, ManagedStorage) {
  // Set policies for the test extension.
  scoped_ptr<base::DictionaryValue> policy = extensions::DictionaryBuilder()
      .Set("string-policy", "value")
      .Set("int-policy", -123)
      .Set("double-policy", 456e7)
      .SetBoolean("boolean-policy", true)
      .Set("list-policy", extensions::ListBuilder()
          .Append("one")
          .Append("two")
          .Append("three"))
      .Set("dict-policy", extensions::DictionaryBuilder()
          .Set("list", extensions::ListBuilder()
              .Append(extensions::DictionaryBuilder()
                  .Set("one", 1)
                  .Set("two", 2))
              .Append(extensions::DictionaryBuilder()
                  .Set("three", 3))))
      .Build();
  SetPolicies(*policy);
  // Now run the extension.
  ASSERT_TRUE(RunExtensionTest("settings/managed_storage")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, PRE_ManagedStorageEvents) {
  ResultCatcher catcher;

  // This test starts without any test extensions installed.
  EXPECT_FALSE(GetSingleLoadedExtension());
  message_.clear();

  // Set policies for the test extension.
  scoped_ptr<base::DictionaryValue> policy = extensions::DictionaryBuilder()
      .Set("constant-policy", "aaa")
      .Set("changes-policy", "bbb")
      .Set("deleted-policy", "ccc")
      .Build();
  SetPolicies(*policy);

  ExtensionTestMessageListener ready_listener("ready", false);
  // Load the extension to install the event listener.
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("settings/managed_storage_events"));
  ASSERT_TRUE(extension);
  // Wait until the extension sends the "ready" message.
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // Now change the policies and wait until the extension is done.
  policy = extensions::DictionaryBuilder()
      .Set("constant-policy", "aaa")
      .Set("changes-policy", "ddd")
      .Set("new-policy", "eee")
      .Build();
  SetPolicies(*policy);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

#if defined(OS_CHROMEOS)
// Flakily times out. http://crbug.com/171477
#define MAYBE_ManagedStorageEvents DISABLED_ManagedStorageEvents
#else
#define MAYBE_ManagedStorageEvents ManagedStorageEvents
#endif

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, ManagedStorageEvents) {
  // This test runs after PRE_ManagedStorageEvents without having deleted the
  // profile, so the extension is still around. While the browser restarted the
  // policy went back to the empty default, and so the extension should receive
  // the corresponding change events.

  ResultCatcher catcher;

  // Verify that the test extension is still installed.
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);
  EXPECT_EQ(kManagedStorageExtensionId, extension->id());

  // Running the test again skips the onInstalled callback, and just triggers
  // the onChanged notification.
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

#endif  // defined(ENABLE_CONFIGURATION_POLICY)

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, ManagedStorageDisabled) {
  // Disable the 'managed' namespace. This is redundant when
  // ENABLE_CONFIGURATION_POLICY is not defined.
  SettingsFrontend* frontend =
      browser()->profile()->GetExtensionService()->settings_frontend();
  frontend->DisableStorageForTesting(MANAGED);
  EXPECT_FALSE(frontend->IsStorageEnabled(MANAGED));
  // Now run the extension.
  ASSERT_TRUE(RunExtensionTest("settings/managed_storage_disabled"))
      << message_;
}

}  // namespace extensions
