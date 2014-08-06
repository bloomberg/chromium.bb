// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/ephemeral_app_browsertest.h"

#include <vector>

#include "apps/app_restore_service.h"
#include "apps/saved_files_service.h"
#include "base/files/scoped_temp_dir.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/apps/ephemeral_app_service.h"
#include "chrome/browser/extensions/api/file_system/file_system_api.h"
#include "chrome/browser/extensions/app_sync_data.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/alarms.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "sync/api/fake_sync_change_processor.h"
#include "sync/api/sync_change_processor_wrapper_for_test.h"
#include "sync/api/sync_error_factory_mock.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notifier_settings.h"

using extensions::AppSyncData;
using extensions::Event;
using extensions::EventRouter;
using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;
using extensions::ExtensionRegistryObserver;
using extensions::ExtensionSystem;
using extensions::Manifest;

namespace {

namespace alarms = extensions::api::alarms;

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

// Saves some parameters from the extension installed notification in order
// to verify them in tests.
class InstallObserver : public ExtensionRegistryObserver {
 public:
  struct InstallParameters {
    std::string id;
    bool is_update;
    bool from_ephemeral;

    InstallParameters(
        const std::string& id,
        bool is_update,
        bool from_ephemeral)
          : id(id), is_update(is_update), from_ephemeral(from_ephemeral) {}
  };

  explicit InstallObserver(Profile* profile) : registry_observer_(this) {
    registry_observer_.Add(ExtensionRegistry::Get(profile));
  }

  virtual ~InstallObserver() {}

  const InstallParameters& Last() {
    CHECK(!install_params_.empty());
    return install_params_.back();
  }

 private:
  virtual void OnExtensionWillBeInstalled(
      content::BrowserContext* browser_context,
      const Extension* extension,
      bool is_update,
      bool from_ephemeral,
      const std::string& old_name) OVERRIDE {
    install_params_.push_back(
        InstallParameters(extension->id(), is_update, from_ephemeral));
  }

  std::vector<InstallParameters> install_params_;
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observer_;
};

}  // namespace


// EphemeralAppTestBase:

const char EphemeralAppTestBase::kMessagingReceiverApp[] =
    "ephemeral_apps/messaging_receiver";
const char EphemeralAppTestBase::kMessagingReceiverAppV2[] =
    "ephemeral_apps/messaging_receiver2";
const char EphemeralAppTestBase::kDispatchEventTestApp[] =
    "ephemeral_apps/dispatch_event";
const char EphemeralAppTestBase::kNotificationsTestApp[] =
    "ephemeral_apps/notification_settings";
const char EphemeralAppTestBase::kFileSystemTestApp[] =
    "ephemeral_apps/filesystem_retain_entries";

EphemeralAppTestBase::EphemeralAppTestBase() {}

EphemeralAppTestBase::~EphemeralAppTestBase() {}

void EphemeralAppTestBase::SetUpCommandLine(base::CommandLine* command_line) {
  // Skip PlatformAppBrowserTest, which sets different values for the switches
  // below.
  ExtensionBrowserTest::SetUpCommandLine(command_line);

  // Make event pages get suspended immediately.
  command_line->AppendSwitchASCII(
      extensions::switches::kEventPageIdleTime, "10");
  command_line->AppendSwitchASCII(
      extensions::switches::kEventPageSuspendingTime, "10");

  // Enable ephemeral apps flag.
  command_line->AppendSwitch(switches::kEnableEphemeralApps);
}

void EphemeralAppTestBase::SetUpOnMainThread() {
  PlatformAppBrowserTest::SetUpOnMainThread();

  // Disable ephemeral apps immediately after they stop running in tests.
  EphemeralAppService::Get(profile())->set_disable_delay_for_test(0);
}

base::FilePath EphemeralAppTestBase::GetTestPath(const char* test_path) {
  return test_data_dir_.AppendASCII("platform_apps").AppendASCII(test_path);
}

const Extension* EphemeralAppTestBase::InstallEphemeralApp(
    const char* test_path, Manifest::Location manifest_location) {
  const Extension* extension = InstallEphemeralAppWithSourceAndFlags(
      GetTestPath(test_path), 1, manifest_location, Extension::NO_FLAGS);
  EXPECT_TRUE(extension);
  if (extension)
    EXPECT_TRUE(extensions::util::IsEphemeralApp(extension->id(), profile()));
  return extension;
}

const Extension* EphemeralAppTestBase::InstallEphemeralApp(
    const char* test_path) {
  return InstallEphemeralApp(test_path, Manifest::INTERNAL);
}

const Extension* EphemeralAppTestBase::InstallAndLaunchEphemeralApp(
    const char* test_path) {
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

const Extension* EphemeralAppTestBase::UpdateEphemeralApp(
    const std::string& app_id,
    const base::FilePath& test_dir,
    const base::FilePath& pem_path) {
  // Pack a new version of the app.
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath crx_path = temp_dir.path().AppendASCII("temp.crx");
  if (!base::DeleteFile(crx_path, false)) {
    ADD_FAILURE() << "Failed to delete existing crx: " << crx_path.value();
    return NULL;
  }

  base::FilePath app_v2_path = PackExtensionWithOptions(
      test_dir, crx_path, pem_path, base::FilePath());
  EXPECT_FALSE(app_v2_path.empty());

  // Update the ephemeral app and wait for the update to finish.
  extensions::CrxInstaller* crx_installer = NULL;
  content::WindowedNotificationObserver windowed_observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::Source<extensions::CrxInstaller>(crx_installer));
  ExtensionService* service =
      ExtensionSystem::Get(profile())->extension_service();
  EXPECT_TRUE(service->UpdateExtension(app_id, app_v2_path, true,
                                       &crx_installer));
  windowed_observer.Wait();

  return ExtensionRegistry::Get(profile())
      ->GetExtensionById(app_id, ExtensionRegistry::EVERYTHING);
}

void EphemeralAppTestBase::PromoteEphemeralApp(
    const extensions::Extension* app) {
  ExtensionService* extension_service =
      ExtensionSystem::Get(profile())->extension_service();
  ASSERT_TRUE(extension_service);
  extension_service->PromoteEphemeralApp(app, false);
}

void EphemeralAppTestBase::DisableEphemeralApp(
    const Extension* app,
    Extension::DisableReason disable_reason) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  // Disabling due to a permissions increase also involves setting the
  // DidExtensionEscalatePermissions flag.
  if (disable_reason == Extension::DISABLE_PERMISSIONS_INCREASE)
    prefs->SetDidExtensionEscalatePermissions(app, true);

  ExtensionSystem::Get(profile())->extension_service()->DisableExtension(
      app->id(), disable_reason);

  ASSERT_TRUE(ExtensionRegistry::Get(profile())->disabled_extensions().Contains(
      app->id()));
}

void EphemeralAppTestBase::CloseApp(const std::string& app_id) {
  EXPECT_EQ(1U, GetAppWindowCountForApp(app_id));
  apps::AppWindow* app_window = GetFirstAppWindowForApp(app_id);
  ASSERT_TRUE(app_window);
  CloseAppWindow(app_window);
}

void EphemeralAppTestBase::CloseAppWaitForUnload(const std::string& app_id) {
  // Ephemeral apps are unloaded from extension system after they stop running.
  content::WindowedNotificationObserver unloaded_signal(
      extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
      content::Source<Profile>(profile()));
  CloseApp(app_id);
  unloaded_signal.Wait();
}

void EphemeralAppTestBase::EvictApp(const std::string& app_id) {
  // Uninstall the app, which is what happens when ephemeral apps get evicted
  // from the cache.
  content::WindowedNotificationObserver uninstalled_signal(
      extensions::NOTIFICATION_EXTENSION_UNINSTALLED_DEPRECATED,
      content::Source<Profile>(profile()));

  ExtensionService* service =
      ExtensionSystem::Get(profile())->extension_service();
  ASSERT_TRUE(service);
  service->UninstallExtension(
      app_id,
      extensions::UNINSTALL_REASON_ORPHANED_EPHEMERAL_EXTENSION,
      base::Bind(&base::DoNothing),
      NULL);

  uninstalled_signal.Wait();
}

// EphemeralAppBrowserTest:

class EphemeralAppBrowserTest : public EphemeralAppTestBase {
 protected:
  bool LaunchAppAndRunTest(const Extension* app, const char* test_name) {
    // Ephemeral apps are unloaded after they are closed. Ensure they are
    // enabled before launch.
    ExtensionService* service =
        ExtensionSystem::Get(profile())->extension_service();
    service->EnableExtension(app->id());

    ExtensionTestMessageListener launched_listener("launched", true);
    LaunchPlatformApp(app);
    if (!launched_listener.WaitUntilSatisfied()) {
      message_ = "Failed to receive launched message from test";
      return false;
    }

    ResultCatcher catcher;
    launched_listener.Reply(test_name);

    bool result = catcher.GetNextResult();
    message_ = catcher.message();

    CloseAppWaitForUnload(app->id());
    return result;
  }

  // Verify that the event page of the app has not been loaded.
  void VerifyAppNotLoaded(const std::string& app_id) {
    EXPECT_FALSE(ExtensionSystem::Get(profile())->
        process_manager()->GetBackgroundHostForExtension(app_id));
  }

  // Verify properties of ephemeral apps.
  void VerifyEphemeralApp(const std::string& app_id) {
    EXPECT_TRUE(extensions::util::IsEphemeralApp(app_id, profile()));

    // Ephemeral apps should not be synced.
    scoped_ptr<AppSyncData> sync_change = GetLastSyncChangeForApp(app_id);
    EXPECT_FALSE(sync_change.get());

    // Ephemeral apps should not be assigned ordinals.
    extensions::AppSorting* app_sorting =
        ExtensionPrefs::Get(profile())->app_sorting();
    EXPECT_FALSE(app_sorting->GetAppLaunchOrdinal(app_id).IsValid());
    EXPECT_FALSE(app_sorting->GetPageOrdinal(app_id).IsValid());
  }

  // Verify that after ephemeral apps stop running, they reside in extension
  // system in a disabled and unloaded state.
  void VerifyInactiveEphemeralApp(const std::string& app_id) {
    EXPECT_TRUE(
        ExtensionRegistry::Get(profile())->disabled_extensions().Contains(
            app_id));

    ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
    EXPECT_TRUE(prefs->IsExtensionDisabled(app_id));
    EXPECT_NE(0,
              prefs->GetDisableReasons(app_id) &
                  Extension::DISABLE_INACTIVE_EPHEMERAL_APP);
  }

  // Verify the state of an app that has been promoted from an ephemeral to a
  // fully installed app.
  void VerifyPromotedApp(const std::string& app_id,
                         ExtensionRegistry::IncludeFlag expected_set) {
    const Extension* app = ExtensionRegistry::Get(profile())
                               ->GetExtensionById(app_id, expected_set);
    ASSERT_TRUE(app) << "App not found in expected set: " << expected_set;

    // The app should not be ephemeral.
    ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
    ASSERT_TRUE(prefs);
    EXPECT_FALSE(prefs->IsEphemeralApp(app_id));
    EXPECT_EQ(0,
              prefs->GetDisableReasons(app_id) &
                  Extension::DISABLE_INACTIVE_EPHEMERAL_APP);

    // Check sort ordinals.
    extensions::AppSorting* app_sorting = prefs->app_sorting();
    EXPECT_TRUE(app_sorting->GetAppLaunchOrdinal(app_id).IsValid());
    EXPECT_TRUE(app_sorting->GetPageOrdinal(app_id).IsValid());
  }

  // Dispatch a fake alarm event to the app.
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

  // Simulates the scenario where an app is installed, via the normal
  // installation route, on top of an ephemeral app. This can occur due to race
  // conditions.
  const Extension* ReplaceEphemeralApp(const std::string& app_id,
                                       const char* test_path,
                                       int expected_enabled_change) {
    return UpdateExtensionWaitForIdle(
        app_id, GetTestPath(test_path), expected_enabled_change);
  }

  void PromoteEphemeralAppAndVerify(
      const Extension* app,
      ExtensionRegistry::IncludeFlag expected_set) {
    ASSERT_TRUE(app);

    // Ephemeral apps should not be synced.
    scoped_ptr<AppSyncData> sync_change = GetLastSyncChangeForApp(app->id());
    EXPECT_FALSE(sync_change.get());

    // Promote the app to a regular installed app.
    InstallObserver installed_observer(profile());
    PromoteEphemeralApp(app);
    VerifyPromotedApp(app->id(), expected_set);

    // Check the notification parameters.
    const InstallObserver::InstallParameters& params =
        installed_observer.Last();
    EXPECT_EQ(app->id(), params.id);
    EXPECT_TRUE(params.is_update);
    EXPECT_TRUE(params.from_ephemeral);

    // The installation should now be synced.
    sync_change = GetLastSyncChangeForApp(app->id());
    VerifySyncChange(sync_change.get(),
                     expected_set == ExtensionRegistry::ENABLED);
  }

  void PromoteEphemeralAppFromSyncAndVerify(
      const Extension* app,
      bool enable_from_sync,
      ExtensionRegistry::IncludeFlag expected_set) {
    ASSERT_TRUE(app);

    // Simulate an install from sync.
    const syncer::StringOrdinal kAppLaunchOrdinal("x");
    const syncer::StringOrdinal kPageOrdinal("y");
    AppSyncData app_sync_data(*app,
                              enable_from_sync,
                              false /* incognito enabled */,
                              false /* remote install */,
                              kAppLaunchOrdinal,
                              kPageOrdinal,
                              extensions::LAUNCH_TYPE_REGULAR);

    std::string app_id = app->id();
    app = NULL;

    ExtensionSyncService* sync_service = ExtensionSyncService::Get(profile());
    sync_service->ProcessAppSyncData(app_sync_data);

    // Verify the installation.
    VerifyPromotedApp(app_id, expected_set);

    // The sort ordinals from sync should not be overridden.
    ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
    extensions::AppSorting* app_sorting = prefs->app_sorting();
    EXPECT_TRUE(
        app_sorting->GetAppLaunchOrdinal(app_id).Equals(kAppLaunchOrdinal));
    EXPECT_TRUE(app_sorting->GetPageOrdinal(app_id).Equals(kPageOrdinal));
  }

  void InitSyncService() {
    ExtensionSyncService* sync_service = ExtensionSyncService::Get(profile());
    sync_service->MergeDataAndStartSyncing(
        syncer::APPS,
        syncer::SyncDataList(),
        scoped_ptr<syncer::SyncChangeProcessor>(
            new syncer::SyncChangeProcessorWrapperForTest(
                &mock_sync_processor_)),
        scoped_ptr<syncer::SyncErrorFactory>(
            new syncer::SyncErrorFactoryMock()));
  }

  scoped_ptr<AppSyncData> GetLastSyncChangeForApp(const std::string& id) {
    scoped_ptr<AppSyncData> sync_data;
    for (syncer::SyncChangeList::iterator it =
             mock_sync_processor_.changes().begin();
         it != mock_sync_processor_.changes().end(); ++it) {
      scoped_ptr<AppSyncData> data(new AppSyncData(*it));
      if (data->id() == id)
        sync_data.reset(data.release());
    }

    return sync_data.Pass();
  }

  void VerifySyncChange(const AppSyncData* sync_change, bool expect_enabled) {
    ASSERT_TRUE(sync_change);
    EXPECT_TRUE(sync_change->page_ordinal().IsValid());
    EXPECT_TRUE(sync_change->app_launch_ordinal().IsValid());
    EXPECT_FALSE(sync_change->uninstalled());
    EXPECT_EQ(expect_enabled, sync_change->extension_sync_data().enabled());
  }

  void TestInstallEvent(bool close_app) {
    ExtensionTestMessageListener first_msg_listener(false);
    const Extension* app = InstallAndLaunchEphemeralApp(kDispatchEventTestApp);
    ASSERT_TRUE(app);

    // When an ephemeral app is first added, it should not receive the
    // onInstalled event, hence the first message received from the test should
    // be "launched" and not "installed".
    ASSERT_TRUE(first_msg_listener.WaitUntilSatisfied());
    EXPECT_EQ(std::string("launched"), first_msg_listener.message());

    if (close_app)
      CloseAppWaitForUnload(app->id());

    // When installed permanently, the app should receive the onInstalled event.
    ExtensionTestMessageListener install_listener("installed", false);
    PromoteEphemeralApp(app);
    ASSERT_TRUE(install_listener.WaitUntilSatisfied());
  }

 private:
  syncer::FakeSyncChangeProcessor mock_sync_processor_;
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
  EventRouter* event_router = EventRouter::Get(profile());
  ASSERT_TRUE(event_router);

  ExtensionTestMessageListener alarm_received_listener("alarm_received", false);
  DispatchAlarmEvent(event_router, extension->id());
  ASSERT_TRUE(alarm_received_listener.WaitUntilSatisfied());

  CloseAppWaitForUnload(extension->id());

  // Dispatch the alarm event again and verify that the event page did not get
  // loaded for the app.
  DispatchAlarmEvent(event_router, extension->id());
  VerifyAppNotLoaded(extension->id());
}

// Verify that ephemeral apps will receive messages while they are running.
// Flaky test: crbug.com/394426
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest,
                       DISABLED_ReceiveMessagesWhenLaunched) {
  const Extension* receiver =
      InstallAndLaunchEphemeralApp(kMessagingReceiverApp);
  ASSERT_TRUE(receiver);

  // Verify that messages are received while the app is running.
  ExtensionApiTest::ResultCatcher result_catcher;
  LoadAndLaunchPlatformApp("ephemeral_apps/messaging_sender_success",
                           "Launched");
  EXPECT_TRUE(result_catcher.GetNextResult());

  CloseAppWaitForUnload(receiver->id());

  // Verify that messages are not received while the app is inactive.
  LoadAndLaunchPlatformApp("ephemeral_apps/messaging_sender_fail", "Launched");
  EXPECT_TRUE(result_catcher.GetNextResult());
}

// Verifies that the chrome.runtime.onInstalled() event is received by a running
// ephemeral app only when it is promoted.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest,
                       InstallEventReceivedWhileRunning) {
  TestInstallEvent(false /* close app */);
}

// Verifies that when an idle ephemeral app is promoted, it will be loaded to
// receive the chrome.runtime.onInstalled() event.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest, InstallEventReceivedWhileIdle) {
  TestInstallEvent(true /* close app */);
}

// Verifies that the chrome.runtime.onRestarted() event is received by an
// ephemeral app.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest, RestartEventReceived) {
  const Extension* app = InstallAndLaunchEphemeralApp(kDispatchEventTestApp);
  ASSERT_TRUE(app);
  CloseAppWaitForUnload(app->id());

  // Fake ephemeral app running before restart.
  ExtensionSystem::Get(profile())->extension_service()->EnableExtension(
      app->id());
  ASSERT_TRUE(ExtensionRegistry::Get(profile())->enabled_extensions().Contains(
      app->id()));
  ExtensionPrefs::Get(profile())->SetExtensionRunning(app->id(), true);

  ExtensionTestMessageListener restart_listener("restarted", false);
  apps::AppRestoreService::Get(profile())->HandleStartup(true);
  EXPECT_TRUE(restart_listener.WaitUntilSatisfied());
}

// Verify that an updated ephemeral app will still have its ephemeral flag
// enabled.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest, UpdateEphemeralApp) {
  InitSyncService();

  const Extension* app_v1 = InstallAndLaunchEphemeralApp(kMessagingReceiverApp);
  ASSERT_TRUE(app_v1);
  VerifyEphemeralApp(app_v1->id());
  CloseAppWaitForUnload(app_v1->id());
  VerifyInactiveEphemeralApp(app_v1->id());

  std::string app_id = app_v1->id();
  base::Version app_original_version = *app_v1->version();

  // Update to version 2 of the app.
  app_v1 = NULL;  // The extension object will be destroyed during update.
  InstallObserver installed_observer(profile());
  const Extension* app_v2 =
      UpdateEphemeralApp(app_id,
                         GetTestPath(kMessagingReceiverAppV2),
                         GetTestPath(kMessagingReceiverApp)
                             .ReplaceExtension(FILE_PATH_LITERAL(".pem")));

  // Check the notification parameters.
  const InstallObserver::InstallParameters& params = installed_observer.Last();
  EXPECT_EQ(app_id, params.id);
  EXPECT_TRUE(params.is_update);
  EXPECT_FALSE(params.from_ephemeral);

  // The ephemeral flag should still be set.
  ASSERT_TRUE(app_v2);
  EXPECT_GT(app_v2->version()->CompareTo(app_original_version), 0);
  VerifyEphemeralApp(app_id);

  // The app should still be disabled in extension system.
  VerifyInactiveEphemeralApp(app_id);
}

// Verify that if notifications have been disabled for an ephemeral app, it will
// remain disabled even after being evicted from the cache.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest, StickyNotificationSettings) {
  const Extension* app = InstallAndLaunchEphemeralApp(kNotificationsTestApp);
  ASSERT_TRUE(app);

  // Disable notifications for this app.
  DesktopNotificationService* notification_service =
      DesktopNotificationServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(notification_service);

  message_center::NotifierId notifier_id(
      message_center::NotifierId::APPLICATION, app->id());
  EXPECT_TRUE(notification_service->IsNotifierEnabled(notifier_id));
  notification_service->SetNotifierEnabled(notifier_id, false);
  EXPECT_FALSE(notification_service->IsNotifierEnabled(notifier_id));

  // Remove the app.
  CloseAppWaitForUnload(app->id());
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
  DCHECK(settings_provider);

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
  CloseAppWaitForUnload(app->id());

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
      apps::SavedFilesService::Get(profile())
          ->GetAllFileEntries(app->id());
  EXPECT_TRUE(file_entries.empty());

  // The second test verifies that the file cannot be reopened.
  ASSERT_TRUE(LaunchAppAndRunTest(app, "RestoreRetainedFile")) << message_;
}

// Checks the process of launching an ephemeral app and then promoting the app
// while it is running.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest, PromoteAppWhileRunning) {
  InitSyncService();

  const Extension* app = InstallAndLaunchEphemeralApp(kNotificationsTestApp);
  ASSERT_TRUE(app);

  PromoteEphemeralAppAndVerify(app, ExtensionRegistry::ENABLED);

  // Ensure that the app is not unloaded and disabled after it is closed.
  CloseApp(app->id());
  VerifyPromotedApp(app->id(), ExtensionRegistry::ENABLED);
}

// Checks the process of launching an ephemeral app and then promoting the app
// while it is idle.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest, PromoteAppWhileIdle) {
  InitSyncService();

  const Extension* app = InstallAndLaunchEphemeralApp(kNotificationsTestApp);
  ASSERT_TRUE(app);
  CloseAppWaitForUnload(app->id());
  VerifyInactiveEphemeralApp(app->id());

  PromoteEphemeralAppAndVerify(app, ExtensionRegistry::ENABLED);
}

// Verifies that promoting an ephemeral app that was disabled due to a
// permissions increase will enable it.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest, PromoteAppAndGrantPermissions) {
  InitSyncService();

  const Extension* app = InstallAndLaunchEphemeralApp(kNotificationsTestApp);
  ASSERT_TRUE(app);
  CloseAppWaitForUnload(app->id());
  DisableEphemeralApp(app, Extension::DISABLE_PERMISSIONS_INCREASE);

  PromoteEphemeralAppAndVerify(app, ExtensionRegistry::ENABLED);
  EXPECT_FALSE(ExtensionPrefs::Get(profile())
                   ->DidExtensionEscalatePermissions(app->id()));
}

// Verifies that promoting an ephemeral app that has unsupported requirements
// will not enable it.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest,
                       PromoteUnsupportedEphemeralApp) {
  InitSyncService();

  const Extension* app = InstallAndLaunchEphemeralApp(kNotificationsTestApp);
  ASSERT_TRUE(app);
  CloseAppWaitForUnload(app->id());
  DisableEphemeralApp(app, Extension::DISABLE_UNSUPPORTED_REQUIREMENT);

  // When promoted to a regular installed app, it should remain disabled.
  PromoteEphemeralAppAndVerify(app, ExtensionRegistry::DISABLED);
}

// Verifies that promoting an ephemeral app that is blacklisted will not enable
// it.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest,
                       PromoteBlacklistedEphemeralApp) {
  InitSyncService();

  const Extension* app = InstallAndLaunchEphemeralApp(kNotificationsTestApp);
  ASSERT_TRUE(app);
  CloseAppWaitForUnload(app->id());

  ExtensionService* service =
      ExtensionSystem::Get(profile())->extension_service();
  service->BlacklistExtensionForTest(app->id());
  ASSERT_TRUE(
      ExtensionRegistry::Get(profile())->blacklisted_extensions().Contains(
          app->id()));

  // When promoted to a regular installed app, it should remain blacklisted.
  PromoteEphemeralAppAndVerify(app, ExtensionRegistry::BLACKLISTED);

  // The app should be synced, but disabled.
  scoped_ptr<AppSyncData> sync_change = GetLastSyncChangeForApp(app->id());
  VerifySyncChange(sync_change.get(), false);
}

// Checks the process of promoting an ephemeral app from sync while the app is
// running.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest,
                       PromoteAppFromSyncWhileRunning) {
  InitSyncService();

  const Extension* app = InstallAndLaunchEphemeralApp(kNotificationsTestApp);
  ASSERT_TRUE(app);

  PromoteEphemeralAppFromSyncAndVerify(app, true, ExtensionRegistry::ENABLED);

  // Ensure that the app is not unloaded and disabled after it is closed.
  CloseApp(app->id());
  VerifyPromotedApp(app->id(), ExtensionRegistry::ENABLED);
}

// Checks the process of promoting an ephemeral app from sync while the app is
// idle.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest, PromoteAppFromSyncWhileIdle) {
  InitSyncService();

  const Extension* app = InstallAndLaunchEphemeralApp(kNotificationsTestApp);
  ASSERT_TRUE(app);
  CloseAppWaitForUnload(app->id());
  VerifyInactiveEphemeralApp(app->id());

  PromoteEphemeralAppFromSyncAndVerify(app, true, ExtensionRegistry::ENABLED);
}

// Checks the process of promoting an ephemeral app from sync, where the app
// from sync is disabled, and the ephemeral app is running.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest,
                       PromoteDisabledAppFromSyncWhileRunning) {
  InitSyncService();

  const Extension* app = InstallAndLaunchEphemeralApp(kNotificationsTestApp);
  ASSERT_TRUE(app);

  PromoteEphemeralAppFromSyncAndVerify(app, false, ExtensionRegistry::DISABLED);
}

// Checks the process of promoting an ephemeral app from sync, where the app
// from sync is disabled, and the ephemeral app is idle.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest,
                       PromoteDisabledAppFromSyncWhileIdle) {
  InitSyncService();

  const Extension* app = InstallAndLaunchEphemeralApp(kNotificationsTestApp);
  ASSERT_TRUE(app);
  CloseAppWaitForUnload(app->id());
  VerifyInactiveEphemeralApp(app->id());

  PromoteEphemeralAppFromSyncAndVerify(app, false, ExtensionRegistry::DISABLED);
}

// In most cases, ExtensionService::PromoteEphemeralApp() will be called to
// permanently install an ephemeral app. However, there may be cases where an
// install occurs through the usual route of installing from the Web Store (due
// to race conditions). Ensure that the app is still installed correctly.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest,
                       ReplaceEphemeralAppWithInstalledApp) {
  InitSyncService();

  const Extension* app = InstallAndLaunchEphemeralApp(kNotificationsTestApp);
  ASSERT_TRUE(app);
  CloseAppWaitForUnload(app->id());
  std::string app_id = app->id();
  app = NULL;

  InstallObserver installed_observer(profile());
  ReplaceEphemeralApp(app_id, kNotificationsTestApp, 1);
  VerifyPromotedApp(app_id, ExtensionRegistry::ENABLED);

  // Check the notification parameters.
  const InstallObserver::InstallParameters& params = installed_observer.Last();
  EXPECT_EQ(app_id, params.id);
  EXPECT_TRUE(params.is_update);
  EXPECT_TRUE(params.from_ephemeral);
}

// This is similar to ReplaceEphemeralAppWithInstalledApp, but installs will
// be delayed until the app is idle.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest,
                       ReplaceEphemeralAppWithDelayedInstalledApp) {
  InitSyncService();

  const Extension* app = InstallAndLaunchEphemeralApp(kNotificationsTestApp);
  ASSERT_TRUE(app);
  std::string app_id = app->id();
  app = NULL;

  // Initiate install.
  ReplaceEphemeralApp(app_id, kNotificationsTestApp, 0);

  // The delayed installation will occur when the ephemeral app is closed.
  content::WindowedNotificationObserver installed_signal(
      extensions::NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED,
      content::Source<Profile>(profile()));
  InstallObserver installed_observer(profile());
  CloseAppWaitForUnload(app_id);
  installed_signal.Wait();
  VerifyPromotedApp(app_id, ExtensionRegistry::ENABLED);

  // Check the notification parameters.
  const InstallObserver::InstallParameters& params = installed_observer.Last();
  EXPECT_EQ(app_id, params.id);
  EXPECT_TRUE(params.is_update);
  EXPECT_TRUE(params.from_ephemeral);
}

// Verifies that an installed app cannot turn into an ephemeral app as result of
// race conditions, i.e. an ephemeral app can be promoted to an installed app,
// but not vice versa.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest,
                       ReplaceInstalledAppWithEphemeralApp) {
  const Extension* app = InstallPlatformApp(kNotificationsTestApp);
  ASSERT_TRUE(app);
  std::string app_id = app->id();
  app = NULL;

  EXPECT_FALSE(extensions::util::IsEphemeralApp(app_id, profile()));
  app =
      InstallEphemeralAppWithSourceAndFlags(GetTestPath(kNotificationsTestApp),
                                            0,
                                            Manifest::INTERNAL,
                                            Extension::NO_FLAGS);
  EXPECT_FALSE(extensions::util::IsEphemeralApp(app_id, profile()));
}

// Ephemerality was previously encoded by the Extension::IS_EPHEMERAL creation
// flag. This was changed to an "ephemeral_app" property. Check that the prefs
// are handled correctly.
IN_PROC_BROWSER_TEST_F(EphemeralAppBrowserTest,
                       ExtensionPrefBackcompatibility) {
  // Ensure that apps with the old prefs are recognized as ephemeral.
  const Extension* app =
      InstallExtensionWithSourceAndFlags(GetTestPath(kNotificationsTestApp),
                                         1,
                                         Manifest::INTERNAL,
                                         Extension::IS_EPHEMERAL);
  ASSERT_TRUE(app);
  EXPECT_TRUE(extensions::util::IsEphemeralApp(app->id(), profile()));

  // Ensure that when the app is promoted to an installed app, the bit in the
  // creation flags is cleared.
  PromoteEphemeralApp(app);
  EXPECT_FALSE(extensions::util::IsEphemeralApp(app->id(), profile()));

  int creation_flags =
      ExtensionPrefs::Get(profile())->GetCreationFlags(app->id());
  EXPECT_EQ(0, creation_flags & Extension::IS_EPHEMERAL);
}
