// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/settings/settings_frontend.h"
#include "chrome/browser/extensions/settings/settings_namespace.h"
#include "chrome/browser/extensions/settings/settings_sync_util.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/api/sync_change.h"
#include "chrome/browser/sync/api/sync_change_processor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"

namespace extensions {

using namespace settings_namespace;

namespace {

class NoopSyncChangeProcessor : public SyncChangeProcessor {
 public:
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE {
    return SyncError();
  }

  virtual ~NoopSyncChangeProcessor() {};
};

}  // namespace

class ExtensionSettingsApiTest : public ExtensionApiTest {
 protected:
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

  void InitSync(SyncChangeProcessor* sync_processor) {
    browser()->profile()->GetExtensionService()->
        settings_frontend()->RunWithSyncableService(
            // TODO(kalman): test both EXTENSION_SETTINGS and APP_SETTINGS.
            syncable::EXTENSION_SETTINGS,
            base::Bind(
                &ExtensionSettingsApiTest::InitSyncWithSyncableService,
                this,
                sync_processor));
    MessageLoop::current()->RunAllPending();
  }

  void SendChanges(const SyncChangeList& change_list) {
    browser()->profile()->GetExtensionService()->
        settings_frontend()->RunWithSyncableService(
            // TODO(kalman): test both EXTENSION_SETTINGS and APP_SETTINGS.
            syncable::EXTENSION_SETTINGS,
            base::Bind(
                &ExtensionSettingsApiTest::SendChangesToSyncableService,
                this,
                change_list));
    MessageLoop::current()->RunAllPending();
  }

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
    base::JSONWriter::Write(message.get(), false, &message_json);
    return message_json;
  }

  void InitSyncWithSyncableService(
      SyncChangeProcessor* sync_processor, SyncableService* settings_service) {
    EXPECT_FALSE(settings_service->MergeDataAndStartSyncing(
        syncable::EXTENSION_SETTINGS,
        SyncDataList(),
        sync_processor).IsSet());
  }

  void SendChangesToSyncableService(
      const SyncChangeList& change_list, SyncableService* settings_service) {
    EXPECT_FALSE(
        settings_service->ProcessSyncChanges(FROM_HERE, change_list).IsSet());
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, SimpleTest) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
  ASSERT_TRUE(RunExtensionTest("settings/simple_test")) << message_;
}

// Structure of this test taken from IncognitoSplitMode.
// Note that only split-mode incognito is tested, because spanning mode
// incognito looks the same as normal mode when the only API activity comes
// from background pages.
IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest, SplitModeIncognito) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

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
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

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
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

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
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

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
  SyncChangeList sync_changes;
  StringValue bar("bar");
  sync_changes.push_back(settings_sync_util::CreateAdd(
      extension_id, "foo", bar));
  SendChanges(sync_changes);

  ReplyWhenSatisfied(SYNC,
      "assertAddFooNotification", "assertAddFooNotification");
  ReplyWhenSatisfied(SYNC, "clearNotifications", "clearNotifications");

  // Remove "foo" via sync.
  sync_changes.clear();
  sync_changes.push_back(settings_sync_util::CreateDelete(
      extension_id, "foo"));
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
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

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
  SyncChangeList sync_changes;
  StringValue bar("bar");
  sync_changes.push_back(settings_sync_util::CreateAdd(
      extension_id, "foo", bar));
  SendChanges(sync_changes);

  ReplyWhenSatisfied(LOCAL, "assertNoNotifications", "assertNoNotifications");

  // Remove "foo" via sync.
  sync_changes.clear();
  sync_changes.push_back(settings_sync_util::CreateDelete(
      extension_id, "foo"));
  SendChanges(sync_changes);

  FinalReplyWhenSatisfied(LOCAL,
      "assertNoNotifications", "assertNoNotifications");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

}  // namespace extensions
