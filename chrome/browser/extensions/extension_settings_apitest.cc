// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_settings_backend.h"
#include "chrome/browser/extensions/extension_settings_sync_util.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/api/sync_change.h"
#include "chrome/browser/sync/api/sync_change_processor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"

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
      const std::string& normal_action,
      const std::string& incognito_action) {
    MaybeLoadAndReplyWhenSatisfied(
        normal_action, incognito_action, NULL, false);
  }

  const Extension* LoadAndReplyWhenSatisfied(
      const std::string& normal_action,
      const std::string& incognito_action,
      const std::string& extension_dir) {
    return MaybeLoadAndReplyWhenSatisfied(
        normal_action, incognito_action, &extension_dir, false);
  }

  void FinalReplyWhenSatisfied(
      const std::string& normal_action,
      const std::string& incognito_action) {
    MaybeLoadAndReplyWhenSatisfied(normal_action, incognito_action, NULL, true);
  }

  void InitSync(SyncChangeProcessor* sync_processor) {
    browser()->profile()->GetExtensionService()->
        extension_settings_frontend()->RunWithBackend(base::Bind(
            &ExtensionSettingsApiTest::InitSyncWithBackend,
            this,
            sync_processor));
    MessageLoop::current()->RunAllPending();
  }

  void SendChanges(const SyncChangeList& change_list) {
    browser()->profile()->GetExtensionService()->
        extension_settings_frontend()->RunWithBackend(base::Bind(
            &ExtensionSettingsApiTest::SendChangesToBackend,
            this,
            change_list));
    MessageLoop::current()->RunAllPending();
  }

 private:
  const Extension* MaybeLoadAndReplyWhenSatisfied(
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

    listener.Reply(CreateMessage(normal_action, is_final_action));
    listener_incognito.Reply(CreateMessage(incognito_action, is_final_action));
    return extension;
  }

  std::string CreateMessage(const std::string& action, bool is_final_action) {
    scoped_ptr<DictionaryValue> message(new DictionaryValue());
    message->SetString("action", action);
    message->SetBoolean("isFinalAction", is_final_action);
    std::string message_json;
    base::JSONWriter::Write(message.get(), false, &message_json);
    return message_json;
  }

  void InitSyncWithBackend(
      SyncChangeProcessor* sync_processor, ExtensionSettingsBackend* backend) {
    EXPECT_FALSE(backend->MergeDataAndStartSyncing(
        syncable::EXTENSION_SETTINGS,
        SyncDataList(),
        sync_processor).IsSet());
  }

  void SendChangesToBackend(
      const SyncChangeList& change_list, ExtensionSettingsBackend* backend) {
    EXPECT_FALSE(backend->ProcessSyncChanges(FROM_HERE, change_list).IsSet());
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

  LoadAndReplyWhenSatisfied("assertEmpty", "assertEmpty", "split_incognito");
  ReplyWhenSatisfied("noop", "setFoo");
  ReplyWhenSatisfied("assertFoo", "assertFoo");
  ReplyWhenSatisfied("clear", "noop");
  ReplyWhenSatisfied("assertEmpty", "assertEmpty");
  ReplyWhenSatisfied("setFoo", "noop");
  ReplyWhenSatisfied("assertFoo", "assertFoo");
  ReplyWhenSatisfied("noop", "removeFoo");
  FinalReplyWhenSatisfied("assertEmpty", "assertEmpty");

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

  LoadAndReplyWhenSatisfied(
      "assertNoNotifications", "assertNoNotifications", "split_incognito");
  ReplyWhenSatisfied("noop", "setFoo");
  ReplyWhenSatisfied("assertAddFooNotification", "assertNoNotifications");
  ReplyWhenSatisfied("clearNotifications", "noop");
  ReplyWhenSatisfied("removeFoo", "noop");
  FinalReplyWhenSatisfied(
      "assertNoNotifications", "assertDeleteFooNotification");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

#if defined(OS_WIN)
// Very flaky on Windows, so just disable.  crbug.com/101110
#define MAYBE_OnChangedNotificationsFromSync \
    DISABLED_OnChangedNotificationsFromSync
#else
// Possibly flaky on other platforms, though haven't observed it yet.
#define MAYBE_OnChangedNotificationsFromSync \
    FLAKY_OnChangedNotificationsFromSync
#endif

IN_PROC_BROWSER_TEST_F(ExtensionSettingsApiTest,
    MAYBE_OnChangedNotificationsFromSync) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher, catcher_incognito;
  catcher.RestrictToProfile(browser()->profile());
  catcher_incognito.RestrictToProfile(
      browser()->profile()->GetOffTheRecordProfile());

  const Extension* extension =
      LoadAndReplyWhenSatisfied(
          "assertNoNotifications", "assertNoNotifications", "split_incognito");
  const std::string& extension_id = extension->id();

  NoopSyncChangeProcessor sync_processor;
  InitSync(&sync_processor);

  // Set "foo" to "bar" via sync.
  SyncChangeList sync_changes;
  StringValue bar("bar");
  sync_changes.push_back(extension_settings_sync_util::CreateAdd(
      extension_id, "foo", bar));
  SendChanges(sync_changes);

  ReplyWhenSatisfied("assertAddFooNotification", "assertAddFooNotification");
  ReplyWhenSatisfied("clearNotifications", "clearNotifications");

  // Remove "foo" via sync.
  sync_changes.clear();
  sync_changes.push_back(extension_settings_sync_util::CreateDelete(
      extension_id, "foo"));
  SendChanges(sync_changes);

  FinalReplyWhenSatisfied(
      "assertDeleteFooNotification", "assertDeleteFooNotification");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}
