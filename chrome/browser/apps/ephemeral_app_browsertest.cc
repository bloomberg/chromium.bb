// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/saved_files_service.h"
#include "base/files/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/apps/ephemeral_app_service.h"
#include "chrome/browser/extensions/api/file_system/file_system_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/common/extensions/api/alarms.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/switches.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notifier_settings.h"

using extensions::Event;
using extensions::EventRouter;
using extensions::Extension;
using extensions::ExtensionInfo;
using extensions::ExtensionPrefs;
using extensions::ExtensionSystem;
using extensions::Manifest;
using extensions::PlatformAppBrowserTest;

namespace {

namespace alarms = extensions::api::alarms;

const char kDispatchEventTestApp[] = "ephemeral_apps/dispatch_event";
const char kMessagingReceiverApp[] = "ephemeral_apps/messaging_receiver";
const char kMessagingReceiverAppV2[] = "ephemeral_apps/messaging_receiver2";
const char kNotificationsTestApp[] = "ephemeral_apps/notification_settings";
const char kFileSystemTestApp[] = "ephemeral_apps/filesystem_retain_entries";
const char kRetainDataApp[] = "ephemeral_apps/retain_data";

typedef std::vector<message_center::Notifier*> NotifierList;

bool IsNotifierInList(const message_center::NotifierId& notifier_id,
                      const NotifierList& notifiers) {
  for (NotifierList::const_iterator it = notifiers.begin();
       it != notifiers.end(); ++it) {
    const message_center::Notifier* notifier = *it;
    if (notifier->notifier_id == notifier_id)
      return true;
  }

  return false;
}

bool IsAppInExtensionsInfo(const ExtensionPrefs::ExtensionsInfo& ext_info,
                           const std::string& extension_id) {
  for (size_t i = 0; i < ext_info.size(); ++i) {
    ExtensionInfo* info = ext_info.at(i).get();
    if (info->extension_id == extension_id)
      return true;
  }

  return false;
}

}  // namespace

class EphemeralAppBrowserTest : public PlatformAppBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Skip PlatformAppBrowserTest, which sets different values for the switches
    // below.
    ExtensionBrowserTest::SetUpCommandLine(command_line);

    // Make event pages get suspended immediately.
    command_line->AppendSwitchASCII(
        extensions::switches::kEventPageIdleTime, "10");
    command_line->AppendSwitchASCII(
        extensions::switches::kEventPageSuspendingTime, "10");
  }

  base::FilePath GetTestPath(const char* test_path) {
    return test_data_dir_.AppendASCII("platform_apps").AppendASCII(test_path);
  }

  const Extension* InstallEphemeralApp(const char* test_path,
                                       Manifest::Location manifest_location) {
    const Extension* extension =
        InstallExtensionWithSourceAndFlags(
            GetTestPath(test_path),
            1,
            manifest_location,
            Extension::IS_EPHEMERAL);
    return extension;
  }

  const Extension* InstallEphemeralApp(const char* test_path) {
    return InstallEphemeralApp(test_path, Manifest::INTERNAL);
  }

  const Extension* InstallAndLaunchEphemeralApp(const char* test_path) {
    ExtensionTestMessageListener launched_listener("launched", false);
    const Extension* extension = InstallEphemeralApp(test_path);
    EXPECT_TRUE(extension);
    if (!extension)
      return NULL;

    LaunchPlatformApp(extension);
    bool wait_result = launched_listener.WaitUntilSatisfied();
    EXPECT_TRUE(wait_result);
    if (!wait_result)
      return NULL;

    return extension;
  }

  bool LaunchAppAndRunTest(const Extension* app, const char* test_name) {
    ExtensionTestMessageListener launched_listener("launched", true);
    LaunchPlatformApp(app);
    if (!launched_listener.WaitUntilSatisfied()) {
      message_ = "Failed to receive launched message from test";
      return false;
    }

    launched_listener.Reply(test_name);

    ResultCatcher catcher;
    if (!catcher.GetNextResult()) {
      message_ = catcher.message();
      return false;
    }

    CloseApp(app->id());
    return true;
  }

  void CloseApp(const std::string& app_id) {
    content::WindowedNotificationObserver event_page_destroyed_signal(
        chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
        content::Source<Profile>(browser()->profile()));

    EXPECT_EQ(1U, GetAppWindowCountForApp(app_id));
    apps::AppWindow* app_window = GetFirstAppWindowForApp(app_id);
    ASSERT_TRUE(app_window);
    CloseAppWindow(app_window);

    event_page_destroyed_signal.Wait();
  }

  void EvictApp(const std::string& app_id) {
    // Uninstall the app, which is what happens when ephemeral apps get evicted
    // from the cache.
    content::WindowedNotificationObserver uninstalled_signal(
        chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
        content::Source<Profile>(browser()->profile()));

    ExtensionService* service =
        ExtensionSystem::Get(browser()->profile())->extension_service();
    ASSERT_TRUE(service);
    service->UninstallExtension(app_id, false, NULL);

    uninstalled_signal.Wait();
  }

  void VerifyAppNotLoaded(const std::string& app_id) {
    EXPECT_FALSE(ExtensionSystem::Get(browser()->profile())->
        process_manager()->GetBackgroundHostForExtension(app_id));
  }

  void DispatchAlarmEvent(EventRouter* event_router,
                          const std::string& app_id) {
    alarms::Alarm dummy_alarm;
    dummy_alarm.name = "test_alarm";

    scoped_ptr<base::ListValue> args(new base::ListValue());
    args->Append(dummy_alarm.ToValue().release());
    scoped_ptr<Event> event(new Event(alarms::OnAlarm::kEventName,
                                      args.Pass()));

    event_router->DispatchEventToExtension(app_id, event.Pass());
  }

  void GarbageCollectData() {
    EphemeralAppService* service =
        EphemeralAppService::Get(browser()->profile());
    ASSERT_TRUE(service);
    service->GarbageCollectData();
  }
};

// Verify that ephemeral apps can be launched and receive system events when
// they are running. Once they are inactive they should not receive system
// events.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest, EventDispatchWhenLaunched) {
  const Extension* extension =
      InstallAndLaunchEphemeralApp(kDispatchEventTestApp);
  ASSERT_TRUE(extension);

  // Send a fake alarm event to the app and verify that a response is
  // received.
  EventRouter* event_router =
      ExtensionSystem::Get(browser()->profile())->event_router();
  ASSERT_TRUE(event_router);

  ExtensionTestMessageListener alarm_received_listener("alarm_received", false);
  DispatchAlarmEvent(event_router, extension->id());
  ASSERT_TRUE(alarm_received_listener.WaitUntilSatisfied());

  CloseApp(extension->id());

  // The app needs to be launched once in order to have the onAlarm() event
  // registered.
  ASSERT_TRUE(event_router->ExtensionHasEventListener(
      extension->id(), alarms::OnAlarm::kEventName));

  // Dispatch the alarm event again and verify that the event page did not get
  // loaded for the app.
  DispatchAlarmEvent(event_router, extension->id());
  VerifyAppNotLoaded(extension->id());
}

// Verify that ephemeral apps will receive messages while they are running.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest, ReceiveMessagesWhenLaunched) {
  const Extension* receiver =
      InstallAndLaunchEphemeralApp(kMessagingReceiverApp);
  ASSERT_TRUE(receiver);

  // Verify that messages are received while the app is running.
  ExtensionApiTest::ResultCatcher result_catcher;
  LoadAndLaunchPlatformApp("ephemeral_apps/messaging_sender_success");
  EXPECT_TRUE(result_catcher.GetNextResult());

  CloseApp(receiver->id());

  // Verify that messages are not received while the app is inactive.
  LoadAndLaunchPlatformApp("ephemeral_apps/messaging_sender_fail");
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Verify that an updated ephemeral app will still have its ephemeral flag
// enabled.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest, UpdateEphemeralApp) {
  const Extension* app_v1 = InstallEphemeralApp(kMessagingReceiverApp);
  ASSERT_TRUE(app_v1);
  ASSERT_TRUE(app_v1->is_ephemeral());
  std::string app_id = app_v1->id();
  base::Version app_original_version = *app_v1->version();
  app_v1 = NULL; // The extension object will be destroyed during update.

  // Pack version 2 of the app.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath crx_path = temp_dir.path().AppendASCII("temp.crx");
  if (!base::DeleteFile(crx_path, false)) {
    ADD_FAILURE() << "Failed to delete crx: " << crx_path.value();
    return;
  }

  base::FilePath app_v2_path = PackExtensionWithOptions(
      GetTestPath(kMessagingReceiverAppV2),
      crx_path,
      GetTestPath(kMessagingReceiverApp).ReplaceExtension(
          FILE_PATH_LITERAL(".pem")),
      base::FilePath());
  ASSERT_FALSE(app_v2_path.empty());

  // Update the ephemeral app and wait for the update to finish.
  extensions::CrxInstaller* crx_installer = NULL;
  content::WindowedNotificationObserver windowed_observer(
      chrome::NOTIFICATION_CRX_INSTALLER_DONE,
      content::Source<extensions::CrxInstaller>(crx_installer));
  ExtensionService* service =
      ExtensionSystem::Get(browser()->profile())->extension_service();
  EXPECT_TRUE(service->UpdateExtension(app_id, app_v2_path, true, GURL(),
                                       &crx_installer));
  windowed_observer.Wait();

  const Extension* app_v2 = service->GetExtensionById(app_id, false);
  ASSERT_TRUE(app_v2);
  EXPECT_TRUE(app_v2->version()->CompareTo(app_original_version) > 0);
  EXPECT_TRUE(app_v2->is_ephemeral());
}

// Verify that if notifications have been disabled for an ephemeral app, it will
// remain disabled even after being evicted from the cache.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest, StickyNotificationSettings) {
  const Extension* app = InstallEphemeralApp(kNotificationsTestApp);
  ASSERT_TRUE(app);

  // Disable notifications for this app.
  DesktopNotificationService* notification_service =
      DesktopNotificationServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(notification_service);

  message_center::NotifierId notifier_id(
      message_center::NotifierId::APPLICATION, app->id());
  EXPECT_TRUE(notification_service->IsNotifierEnabled(notifier_id));
  notification_service->SetNotifierEnabled(notifier_id, false);
  EXPECT_FALSE(notification_service->IsNotifierEnabled(notifier_id));

  // Remove the app.
  EvictApp(app->id());

  // Reinstall the ephemeral app and verify that notifications remain disabled.
  app = InstallEphemeralApp(kNotificationsTestApp);
  ASSERT_TRUE(app);
  message_center::NotifierId reinstalled_notifier_id(
      message_center::NotifierId::APPLICATION, app->id());
  EXPECT_FALSE(notification_service->IsNotifierEnabled(
      reinstalled_notifier_id));
}

// Verify that only running ephemeral apps will appear in the Notification
// Settings UI. Inactive, cached ephemeral apps should not appear.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest,
                       IncludeRunningEphemeralAppsInNotifiers) {
  message_center::NotifierSettingsProvider* settings_provider =
      message_center::MessageCenter::Get()->GetNotifierSettingsProvider();
  // TODO(tmdiep): Remove once notifications settings are supported across
  // all platforms. This test will fail for Linux GTK.
  if (!settings_provider)
    return;

  const Extension* app = InstallAndLaunchEphemeralApp(kNotificationsTestApp);
  ASSERT_TRUE(app);
  message_center::NotifierId notifier_id(
      message_center::NotifierId::APPLICATION, app->id());

  // Since the ephemeral app is running, it should be included in the list
  // of notifiers to show in the UI.
  NotifierList notifiers;
  STLElementDeleter<NotifierList> notifier_deleter(&notifiers);

  settings_provider->GetNotifierList(&notifiers);
  EXPECT_TRUE(IsNotifierInList(notifier_id, notifiers));
  STLDeleteElements(&notifiers);

  // Close the ephemeral app.
  CloseApp(app->id());

  // Inactive ephemeral apps should not be included in the list of notifiers to
  // show in the UI.
  settings_provider->GetNotifierList(&notifiers);
  EXPECT_FALSE(IsNotifierInList(notifier_id, notifiers));
}

// Verify that ephemeral apps will have no ability to retain file entries after
// close. Normal retainEntry behavior for installed apps is tested in
// FileSystemApiTest.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest,
                       DisableRetainFileSystemEntries) {
  // Create a dummy file that we can just return to the test.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.path(), &temp_file));

  using extensions::FileSystemChooseEntryFunction;
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &temp_file);
  // The temporary file needs to be registered for the tests to pass on
  // ChromeOS.
  FileSystemChooseEntryFunction::RegisterTempExternalFileSystemForTest(
      "temp", temp_dir.path());

  // The first test opens the file and writes the file handle to local storage.
  const Extension* app = InstallEphemeralApp(kFileSystemTestApp,
                                             Manifest::UNPACKED);
  ASSERT_TRUE(LaunchAppAndRunTest(app, "OpenAndRetainFile")) << message_;

  // Verify that after the app has been closed, all retained entries are
  // flushed.
  std::vector<apps::SavedFileEntry> file_entries =
      apps::SavedFilesService::Get(browser()->profile())
          ->GetAllFileEntries(app->id());
  EXPECT_TRUE(file_entries.empty());

  // The second test verifies that the file cannot be reopened.
  ASSERT_TRUE(LaunchAppAndRunTest(app, "RestoreRetainedFile")) << message_;
}

// Verify that once evicted from the cache, the data of ephemeral apps will not
// be deleted.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest, RetainData) {
  // Phase 1 - Install the ephemeral app and write data to various storage.
  const Extension* app = InstallEphemeralApp(kRetainDataApp);
  ASSERT_TRUE(app);
  ASSERT_TRUE(LaunchAppAndRunTest(app, "WriteData")) << message_;

  // Sanity check to ensure that the ReadData tests should pass before the app
  // is removed.
  ASSERT_TRUE(LaunchAppAndRunTest(app, "ReadData")) << message_;

  // Remove the app.
  const std::string app_id = app->id();
  EvictApp(app->id());
  app = NULL;

  // The app should be in the list of evicted apps.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser()->profile());
  ASSERT_TRUE(prefs);
  scoped_ptr<ExtensionPrefs::ExtensionsInfo> extensions_info(
      prefs->GetEvictedEphemeralAppsInfo());
  EXPECT_TRUE(IsAppInExtensionsInfo(*extensions_info, app_id));

  // The app should not be in the list of installed extensions.
  extensions_info = prefs->GetInstalledExtensionsInfo();
  EXPECT_FALSE(IsAppInExtensionsInfo(*extensions_info, app_id));

  // Ensure the evicted app is considered to have isolated storage. This will
  // prevent its data from getting garbage collected by
  // ExtensionService::GarbageCollectIsolatedStorage().
  GURL site_url = extensions::util::GetSiteForExtensionId(
      app_id, browser()->profile());
  EXPECT_TRUE(extensions::util::SiteHasIsolatedStorage(
                  site_url, browser()->profile()));

  // Phase 2 - Reinstall the ephemeral app and verify that data still exists
  // in the storage.
  app = InstallEphemeralApp(kRetainDataApp);
  ASSERT_TRUE(app);
  EXPECT_TRUE(LaunchAppAndRunTest(app, "ReadData")) << message_;

  // The app should now be in the list of installed extensions, but not in the
  // list of evicted apps.
  extensions_info = prefs->GetInstalledExtensionsInfo();
  EXPECT_TRUE(IsAppInExtensionsInfo(*extensions_info, app_id));
  extensions_info = prefs->GetEvictedEphemeralAppsInfo();
  EXPECT_FALSE(IsAppInExtensionsInfo(*extensions_info, app_id));
}

// Verify that the data of regular installed apps are deleted on uninstall.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest, RemoveInstalledData) {
  // Install the ephemeral app and write data to various storage.
  const Extension* app = InstallPlatformApp(kRetainDataApp);
  ASSERT_TRUE(app);
  ASSERT_TRUE(LaunchAppAndRunTest(app, "WriteData")) << message_;

  // Remove the app.
  const std::string app_id = app->id();
  EvictApp(app->id());
  app = NULL;

  // The app should not be in the preferences.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser()->profile());
  ASSERT_TRUE(prefs);
  scoped_ptr<ExtensionPrefs::ExtensionsInfo> extensions_info(
      prefs->GetEvictedEphemeralAppsInfo());
  EXPECT_FALSE(IsAppInExtensionsInfo(*extensions_info, app_id));
  extensions_info = prefs->GetInstalledExtensionsInfo();
  EXPECT_FALSE(IsAppInExtensionsInfo(*extensions_info, app_id));

  // Reinstall the app and verify that all data has been reset.
  app = InstallPlatformApp(kRetainDataApp);
  ASSERT_TRUE(LaunchAppAndRunTest(app, "DataReset")) << message_;
}

// Verify that once evicted from the cache, ephemeral apps will remain in
// extension prefs, but marked as evicted. After garbage collection of data,
// both their data and preferences should be removed.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest, GarbageCollectData) {
  // Create two apps. Both will be evicted from the cache, but the data of
  // one will be garbage collected.
  const Extension* evict_app = InstallEphemeralApp(kRetainDataApp);
  ASSERT_TRUE(evict_app);
  ASSERT_TRUE(LaunchAppAndRunTest(evict_app, "WriteData")) << message_;
  std::string evict_app_id = evict_app->id();
  EvictApp(evict_app_id);
  evict_app = NULL;

  const Extension* retain_app = InstallEphemeralApp(kDispatchEventTestApp);
  ASSERT_TRUE(retain_app);
  std::string retain_app_id = retain_app->id();
  EvictApp(retain_app_id);
  retain_app = NULL;

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser()->profile());
  ASSERT_TRUE(prefs);

  // Both apps should be in the list of evicted apps.
  scoped_ptr<ExtensionPrefs::ExtensionsInfo> extensions_info(
      prefs->GetEvictedEphemeralAppsInfo());
  EXPECT_TRUE(IsAppInExtensionsInfo(*extensions_info, retain_app_id));
  EXPECT_TRUE(IsAppInExtensionsInfo(*extensions_info, evict_app_id));

  // Set a fake last launch time so that the ephemeral app's data will be
  // garbage collected.
  base::Time launch_time =
      base::Time::Now() - base::TimeDelta::FromDays(
                              EphemeralAppService::kDataInactiveThreshold + 1);
  prefs->SetLastLaunchTime(evict_app_id, launch_time);
  prefs->SetLastLaunchTime(retain_app_id, base::Time::Now());

  // Garbage collect data.
  GarbageCollectData();

  // The garbage collected app should no longer be in the preferences.
  extensions_info = prefs->GetEvictedEphemeralAppsInfo();
  EXPECT_TRUE(IsAppInExtensionsInfo(*extensions_info, retain_app_id));
  ASSERT_FALSE(IsAppInExtensionsInfo(*extensions_info, evict_app_id));

  // Reinstall the app and verify that all data has been reset.
  evict_app = InstallEphemeralApp(kRetainDataApp);
  ASSERT_TRUE(LaunchAppAndRunTest(evict_app, "DataReset")) << message_;
}
