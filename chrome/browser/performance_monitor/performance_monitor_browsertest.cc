// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/performance_monitor/constants.h"
#include "chrome/browser/performance_monitor/database.h"
#include "chrome/browser/performance_monitor/metric.h"
#include "chrome/browser/performance_monitor/performance_monitor.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

using extensions::Extension;

namespace performance_monitor {

namespace {

const base::TimeDelta kMaxStartupTime = base::TimeDelta::FromMinutes(3);

// Helper struct to store the information of an extension; this is needed if the
// pointer to the extension ever becomes invalid (e.g., if we uninstall the
// extension).
struct ExtensionBasicInfo {
  // Empty constructor for stl-container-happiness.
  ExtensionBasicInfo() {
  }
  explicit ExtensionBasicInfo(const Extension* extension)
      : description(extension->description()),
        id(extension->id()),
        name(extension->name()),
        url(extension->url().spec()),
        version(extension->VersionString()),
        location(extension->location()) {
  }

  std::string description;
  std::string id;
  std::string name;
  std::string url;
  std::string version;
  extensions::Manifest::Location location;
};

// Compare the fields of |extension| to those in |value|; this is a check to
// make sure the extension data was recorded properly in the event.
void ValidateExtensionInfo(const ExtensionBasicInfo extension,
                           const DictionaryValue* value) {
  std::string extension_description;
  std::string extension_id;
  std::string extension_name;
  std::string extension_url;
  std::string extension_version;
  int extension_location;

  ASSERT_TRUE(value->GetString("extensionDescription",
                               &extension_description));
  ASSERT_EQ(extension.description, extension_description);
  ASSERT_TRUE(value->GetString("extensionId", &extension_id));
  ASSERT_EQ(extension.id, extension_id);
  ASSERT_TRUE(value->GetString("extensionName", &extension_name));
  ASSERT_EQ(extension.name, extension_name);
  ASSERT_TRUE(value->GetString("extensionUrl", &extension_url));
  ASSERT_EQ(extension.url, extension_url);
  ASSERT_TRUE(value->GetString("extensionVersion", &extension_version));
  ASSERT_EQ(extension.version, extension_version);
  ASSERT_TRUE(value->GetInteger("extensionLocation", &extension_location));
  ASSERT_EQ(extension.location, extension_location);
}

// Verify that a particular event has the proper type.
void CheckEventType(int expected_event_type, const linked_ptr<Event>& event) {
  int event_type = -1;
  ASSERT_TRUE(event->data()->GetInteger("eventType", &event_type));
  ASSERT_EQ(expected_event_type, event_type);
  ASSERT_EQ(expected_event_type, event->type());
}

// Verify that we received the proper number of events, checking the type of
// each one.
void CheckEventTypes(const std::vector<int> expected_event_types,
                     const Database::EventVector& events) {
  ASSERT_EQ(expected_event_types.size(), events.size());

  for (size_t i = 0; i < expected_event_types.size(); ++i)
    CheckEventType(expected_event_types[i], events[i]);
}

// Check that we received the proper number of events, that each event is of the
// proper type, and that each event recorded the proper information about the
// extension.
void CheckExtensionEvents(
    const std::vector<int>& expected_event_types,
    const Database::EventVector& events,
    const std::vector<ExtensionBasicInfo>& extension_infos) {
  CheckEventTypes(expected_event_types, events);

  for (size_t i = 0; i < expected_event_types.size(); ++i) {
    ValidateExtensionInfo(extension_infos[i], events[i]->data());
    int event_type;
    ASSERT_TRUE(events[i]->data()->GetInteger("eventType", &event_type));
    ASSERT_EQ(expected_event_types[i], event_type);
  }
}

}  // namespace

class PerformanceMonitorBrowserTest : public ExtensionBrowserTest {
 public:
  virtual void SetUpOnMainThread() OVERRIDE {
    CHECK(db_dir_.CreateUniqueTempDir());
    performance_monitor_ = PerformanceMonitor::GetInstance();
    performance_monitor_->SetDatabasePath(db_dir_.path());

    // PerformanceMonitor's initialization process involves a significant
    // amount of thread-hopping between the UI thread and the background thread.
    // If we begin the tests prior to full initialization, we cannot predict
    // the behavior or mock synchronicity as we must. Wait for initialization
    // to complete fully before proceeding with the test.
    content::WindowedNotificationObserver windowed_observer(
        chrome::NOTIFICATION_PERFORMANCE_MONITOR_INITIALIZED,
        content::NotificationService::AllSources());

    performance_monitor_->Start();

    windowed_observer.Wait();

    // We stop the timer in charge of doing timed collections so that we can
    // enforce when, and how many times, we do these collections.
    performance_monitor_->timer_.Stop();
  }

  // A handle for gathering statistics from the database, which must be done on
  // the background thread. Since we are testing, we can mock synchronicity with
  // FlushForTesting().
  void GatherStatistics() {
    content::BrowserThread::PostBlockingPoolSequencedTask(
        Database::kDatabaseSequenceToken,
        FROM_HERE,
        base::Bind(&PerformanceMonitor::GatherStatisticsOnBackgroundThread,
                   base::Unretained(performance_monitor())));

    content::BrowserThread::GetBlockingPool()->FlushForTesting();
  }

  void GetEventsOnBackgroundThread(Database::EventVector* events) {
    // base::Time is potentially flaky in that there is no guarantee that it
    // won't actually decrease between successive calls. If we call GetEvents
    // and the Database uses base::Time::Now() and gets a lesser time, then it
    // will return 0 events. Thus, we use a time that is guaranteed to be in the
    // future (for at least the next couple hundred thousand years).
    *events = performance_monitor_->database()->GetEvents(
        base::Time(), base::Time::FromInternalValue(kint64max));
  }

  // A handle for getting the events from the database, which must be done on
  // the background thread. Since we are testing, we can mock synchronicity
  // with FlushForTesting().
  Database::EventVector GetEvents() {
    // Ensure that any event insertions happen prior to getting events in order
    // to avoid race conditions.
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    content::RunAllPendingInMessageLoop();

    Database::EventVector events;
    content::BrowserThread::PostBlockingPoolSequencedTask(
        Database::kDatabaseSequenceToken,
        FROM_HERE,
        base::Bind(&PerformanceMonitorBrowserTest::GetEventsOnBackgroundThread,
                   base::Unretained(this),
                   &events));

    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    return events;
  }

  void GetStatsOnBackgroundThread(Database::MetricVector* metrics,
                                  MetricType type) {
    *metrics = *performance_monitor_->database()->GetStatsForActivityAndMetric(
        type, base::Time(), base::Time::FromInternalValue(kint64max));
  }

  // A handle for getting statistics from the database (see previous comments on
  // GetEvents() and GetEventsOnBackgroundThread).
  Database::MetricVector GetStats(MetricType type) {
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    content::RunAllPendingInMessageLoop();

    Database::MetricVector metrics;
    content::BrowserThread::PostBlockingPoolSequencedTask(
        Database::kDatabaseSequenceToken,
        FROM_HERE,
        base::Bind(&PerformanceMonitorBrowserTest::GetStatsOnBackgroundThread,
                   base::Unretained(this),
                   &metrics,
                   type));

    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    return metrics;
  }

  // A handle for inserting a state value into the database, which must be done
  // on the background thread. This is useful for mocking up a scenario in which
  // the database has prior data stored. We mock synchronicity with
  // FlushForTesting().
  void AddStateValue(const std::string& key, const std::string& value) {
    content::BrowserThread::PostBlockingPoolSequencedTask(
        Database::kDatabaseSequenceToken,
        FROM_HERE,
        base::Bind(base::IgnoreResult(&Database::AddStateValue),
                   base::Unretained(performance_monitor()->database()),
                   key,
                   value));

    content::BrowserThread::GetBlockingPool()->FlushForTesting();
  }

  // A handle for PerformanceMonitor::CheckForVersionUpdateOnBackgroundThread();
  // we mock synchronicity with FlushForTesting().
  void CheckForVersionUpdate() {
    content::BrowserThread::PostBlockingPoolSequencedTask(
        Database::kDatabaseSequenceToken,
        FROM_HERE,
        base::Bind(&PerformanceMonitor::CheckForVersionUpdateOnBackgroundThread,
                   base::Unretained(performance_monitor())));

    content::BrowserThread::GetBlockingPool()->FlushForTesting();
  }

  PerformanceMonitor* performance_monitor() const {
    return performance_monitor_;
  }

 protected:
  base::ScopedTempDir db_dir_;
  PerformanceMonitor* performance_monitor_;
};

class PerformanceMonitorUncleanExitBrowserTest
    : public PerformanceMonitorBrowserTest {
 public:
  virtual bool SetUpUserDataDirectory() OVERRIDE {
    base::FilePath user_data_directory;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_directory);

    // On CrOS, if we are "logged in" with the --login-profile switch,
    // the default profile will be different. We check if we are logged in, and,
    // if we are, we use that profile name instead. (Note: trybots will
    // typically be logged in with 'user'.)
#if defined(OS_CHROMEOS)
    const CommandLine command_line = *CommandLine::ForCurrentProcess();
    if (command_line.HasSwitch(switches::kLoginProfile)) {
      first_profile_name_ =
          command_line.GetSwitchValueASCII(switches::kLoginProfile);
    } else {
      first_profile_name_ = chrome::kInitialProfile;
    }
#else
    first_profile_name_ = chrome::kInitialProfile;
#endif

    base::FilePath first_profile =
        user_data_directory.AppendASCII(first_profile_name_);
    CHECK(file_util::CreateDirectory(first_profile));

    base::FilePath stock_prefs_file;
    PathService::Get(chrome::DIR_TEST_DATA, &stock_prefs_file);
    stock_prefs_file = stock_prefs_file.AppendASCII("performance_monitor")
                                       .AppendASCII("unclean_exit_prefs");
    CHECK(file_util::PathExists(stock_prefs_file));

    base::FilePath first_profile_prefs_file =
        first_profile.Append(chrome::kPreferencesFilename);
    CHECK(file_util::CopyFile(stock_prefs_file, first_profile_prefs_file));
    CHECK(file_util::PathExists(first_profile_prefs_file));

    second_profile_name_ =
        std::string(chrome::kMultiProfileDirPrefix)
        .append(base::IntToString(1));

    base::FilePath second_profile =
        user_data_directory.AppendASCII(second_profile_name_);
    CHECK(file_util::CreateDirectory(second_profile));

    base::FilePath second_profile_prefs_file =
        second_profile.Append(chrome::kPreferencesFilename);
    CHECK(file_util::CopyFile(stock_prefs_file, second_profile_prefs_file));
    CHECK(file_util::PathExists(second_profile_prefs_file));

    return true;
  }

 protected:
  std::string first_profile_name_;
  std::string second_profile_name_;
};

class PerformanceMonitorSessionRestoreBrowserTest
    : public PerformanceMonitorBrowserTest {
 public:
  virtual void SetUpOnMainThread() OVERRIDE {
    SessionStartupPref pref(SessionStartupPref::LAST);
    SessionStartupPref::SetStartupPref(browser()->profile(), pref);
#if defined(OS_CHROMEOS) || defined (OS_MACOSX)
    // Undo the effect of kBrowserAliveWithNoWindows in defaults.cc so that we
    // can get these test to work without quitting.
    SessionServiceTestHelper helper(
        SessionServiceFactory::GetForProfile(browser()->profile()));
    helper.SetForceBrowserNotAliveWithNoWindows(true);
    helper.ReleaseService();
#endif

    PerformanceMonitorBrowserTest::SetUpOnMainThread();
  }

  Browser* QuitBrowserAndRestore(Browser* browser, int expected_tab_count) {
    Profile* profile = browser->profile();

    // Close the browser.
    g_browser_process->AddRefModule();
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::NotificationService::AllSources());
    browser->window()->Close();
#if defined(OS_MACOSX)
    // BrowserWindowController depends on the auto release pool being recycled
    // in the message loop to delete itself, which frees the Browser object
    // which fires this event.
    AutoreleasePool()->Recycle();
#endif
    observer.Wait();

    // Create a new window, which should trigger session restore.
    ui_test_utils::BrowserAddedObserver window_observer;
    content::TestNavigationObserver navigation_observer(
        content::NotificationService::AllSources(), NULL, expected_tab_count);
    chrome::NewEmptyWindow(profile, chrome::HOST_DESKTOP_TYPE_NATIVE);
    Browser* new_browser = window_observer.WaitForSingleNewBrowser();
    navigation_observer.Wait();
    g_browser_process->ReleaseModule();

    return new_browser;
  }
};

// Test that PerformanceMonitor will correctly record an extension installation
// event.
IN_PROC_BROWSER_TEST_F(PerformanceMonitorBrowserTest, InstallExtensionEvent) {
  base::FilePath extension_path;
  PathService::Get(chrome::DIR_TEST_DATA, &extension_path);
  extension_path = extension_path.AppendASCII("performance_monitor")
                                 .AppendASCII("extensions")
                                 .AppendASCII("simple_extension_v1");
  const Extension* extension = LoadExtension(extension_path);

  std::vector<ExtensionBasicInfo> extension_infos;
  extension_infos.push_back(ExtensionBasicInfo(extension));

  std::vector<int> expected_event_types;
  expected_event_types.push_back(EVENT_EXTENSION_INSTALL);

  Database::EventVector events = GetEvents();
  CheckExtensionEvents(expected_event_types, events, extension_infos);
}

// Test that PerformanceMonitor will correctly record events as an extension is
// disabled and enabled.
// Test is falky, see http://crbug.com/157980
IN_PROC_BROWSER_TEST_F(PerformanceMonitorBrowserTest,
                       DISABLED_DisableAndEnableExtensionEvent) {
  const int kNumEvents = 3;

  base::FilePath extension_path;
  PathService::Get(chrome::DIR_TEST_DATA, &extension_path);
  extension_path = extension_path.AppendASCII("performance_monitor")
                                 .AppendASCII("extensions")
                                 .AppendASCII("simple_extension_v1");
  const Extension* extension = LoadExtension(extension_path);

  DisableExtension(extension->id());
  EnableExtension(extension->id());

  std::vector<ExtensionBasicInfo> extension_infos;
  // There will be three events in all, each pertaining to the same extension:
  //   Extension Install
  //   Extension Disable
  //   Extension Enable
  for (int i = 0; i < kNumEvents; ++i)
    extension_infos.push_back(ExtensionBasicInfo(extension));

  std::vector<int> expected_event_types;
  expected_event_types.push_back(EVENT_EXTENSION_INSTALL);
  expected_event_types.push_back(EVENT_EXTENSION_DISABLE);
  expected_event_types.push_back(EVENT_EXTENSION_ENABLE);

  Database::EventVector events = GetEvents();
  CheckExtensionEvents(expected_event_types, events, extension_infos);
}

// Test that PerformanceMonitor correctly records an extension update event.
IN_PROC_BROWSER_TEST_F(PerformanceMonitorBrowserTest, UpdateExtensionEvent) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath test_data_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  test_data_dir = test_data_dir.AppendASCII("performance_monitor")
                               .AppendASCII("extensions");

  // We need two versions of the same extension.
  base::FilePath pem_path = test_data_dir.AppendASCII("simple_extension.pem");
  base::FilePath path_v1_ = PackExtensionWithOptions(
      test_data_dir.AppendASCII("simple_extension_v1"),
      temp_dir.path().AppendASCII("simple_extension1.crx"),
      pem_path,
      base::FilePath());
  base::FilePath path_v2_ = PackExtensionWithOptions(
      test_data_dir.AppendASCII("simple_extension_v2"),
      temp_dir.path().AppendASCII("simple_extension2.crx"),
      pem_path,
      base::FilePath());

  const extensions::Extension* extension = InstallExtension(path_v1_, 1);

  std::vector<ExtensionBasicInfo> extension_infos;
  extension_infos.push_back(ExtensionBasicInfo(extension));

  ExtensionService* extension_service =
      browser()->profile()->GetExtensionService();

  extensions::CrxInstaller* crx_installer = NULL;

  // Create an observer to wait for the update to finish.
  content::WindowedNotificationObserver windowed_observer(
      chrome::NOTIFICATION_CRX_INSTALLER_DONE,
      content::Source<extensions::CrxInstaller>(crx_installer));
  ASSERT_TRUE(extension_service->
      UpdateExtension(extension->id(), path_v2_, GURL(), &crx_installer));
  windowed_observer.Wait();

  extension = extension_service->GetExtensionById(
      extension_infos[0].id, false); // don't include disabled extensions.

  // The total series of events for this process will be:
  //   Extension Install - install version 1
  //   Extension Install - install version 2
  //   Extension Update  - signal the udate to version 2
  // We push back the corresponding ExtensionBasicInfos.
  extension_infos.push_back(ExtensionBasicInfo(extension));
  extension_infos.push_back(extension_infos[1]);

  std::vector<int> expected_event_types;
  expected_event_types.push_back(EVENT_EXTENSION_INSTALL);
  expected_event_types.push_back(EVENT_EXTENSION_INSTALL);
  expected_event_types.push_back(EVENT_EXTENSION_UPDATE);

  Database::EventVector events = GetEvents();

  CheckExtensionEvents(expected_event_types, events, extension_infos);
}

IN_PROC_BROWSER_TEST_F(PerformanceMonitorBrowserTest, UninstallExtensionEvent) {
  const int kNumEvents = 2;
  base::FilePath extension_path;
  PathService::Get(chrome::DIR_TEST_DATA, &extension_path);
  extension_path = extension_path.AppendASCII("performance_monitor")
                                 .AppendASCII("extensions")
                                 .AppendASCII("simple_extension_v1");
  const Extension* extension = LoadExtension(extension_path);

  std::vector<ExtensionBasicInfo> extension_infos;
  // There will be two events, both pertaining to the same extension:
  //   Extension Install
  //   Extension Uninstall
  for (int i = 0; i < kNumEvents; ++i)
    extension_infos.push_back(ExtensionBasicInfo(extension));

  UninstallExtension(extension->id());

  std::vector<int> expected_event_types;
  expected_event_types.push_back(EVENT_EXTENSION_INSTALL);
  expected_event_types.push_back(EVENT_EXTENSION_UNINSTALL);

  Database::EventVector events = GetEvents();

  CheckExtensionEvents(expected_event_types, events, extension_infos);
}

IN_PROC_BROWSER_TEST_F(PerformanceMonitorBrowserTest, NewVersionEvent) {
  const char kOldVersion[] = "0.0";

  // The version in the database right now will be the current version of chrome
  // (gathered at initialization of PerformanceMonitor). Replace this with an
  // older version so an event is generated.
  AddStateValue(kStateChromeVersion, kOldVersion);

  CheckForVersionUpdate();

  chrome::VersionInfo version;
  ASSERT_TRUE(version.is_valid());
  std::string version_string = version.Version();

  Database::EventVector events = GetEvents();
  ASSERT_EQ(1u, events.size());
  ASSERT_EQ(EVENT_CHROME_UPDATE, events[0]->type());

  const base::DictionaryValue* value;
  ASSERT_TRUE(events[0]->data()->GetAsDictionary(&value));

  std::string previous_version;
  std::string current_version;

  ASSERT_TRUE(value->GetString("previousVersion", &previous_version));
  ASSERT_EQ(kOldVersion, previous_version);
  ASSERT_TRUE(value->GetString("currentVersion", &current_version));
  ASSERT_EQ(version_string, current_version);
}

// crbug.com/160502
IN_PROC_BROWSER_TEST_F(PerformanceMonitorBrowserTest,
                       DISABLED_GatherStatistics) {
  GatherStatistics();

  // No stats should be recorded for this CPUUsage because this was the first
  // call to GatherStatistics.
  Database::MetricVector stats = GetStats(METRIC_CPU_USAGE);
  ASSERT_EQ(0u, stats.size());

  stats = GetStats(METRIC_PRIVATE_MEMORY_USAGE);
  ASSERT_EQ(1u, stats.size());
  EXPECT_GT(stats[0].value, 0);

  stats = GetStats(METRIC_SHARED_MEMORY_USAGE);
  ASSERT_EQ(1u, stats.size());
  EXPECT_GT(stats[0].value, 0);

  // Open new tabs to incur CPU usage.
  for (int i = 0; i < 10; ++i) {
    chrome::NavigateParams params(
        browser(), ui_test_utils::GetTestUrl(
                       base::FilePath(base::FilePath::kCurrentDirectory),
                       base::FilePath(FILE_PATH_LITERAL("title1.html"))),
        content::PAGE_TRANSITION_LINK);
    params.disposition = NEW_BACKGROUND_TAB;
    ui_test_utils::NavigateToURL(&params);
  }
  GatherStatistics();

  // One CPUUsage stat should exist now.
  stats = GetStats(METRIC_CPU_USAGE);
  ASSERT_EQ(1u, stats.size());
  EXPECT_GT(stats[0].value, 0);

  stats = GetStats(METRIC_PRIVATE_MEMORY_USAGE);
  ASSERT_EQ(2u, stats.size());
  EXPECT_GT(stats[1].value, 0);

  stats = GetStats(METRIC_SHARED_MEMORY_USAGE);
  ASSERT_EQ(2u, stats.size());
  EXPECT_GT(stats[1].value, 0);
}

// Disabled on other platforms because of flakiness: http://crbug.com/159172.
#if !defined(OS_WIN)
// Disabled on Windows due to a bug where Windows will return a normal exit
// code in the testing environment, even if the process died (this is not the
// case when hand-testing). This code can be traced to MSDN functions in
// base::GetTerminationStatus(), so there's not much we can do.
IN_PROC_BROWSER_TEST_F(PerformanceMonitorBrowserTest,
                       DISABLED_RendererKilledEvent) {
  content::CrashTab(browser()->tab_strip_model()->GetActiveWebContents());

  Database::EventVector events = GetEvents();

  ASSERT_EQ(1u, events.size());
  CheckEventType(EVENT_RENDERER_KILLED, events[0]);

  // Check the url - since we never went anywhere, this should be about:blank.
  std::string url;
  ASSERT_TRUE(events[0]->data()->GetString("url", &url));
  ASSERT_EQ("about:blank", url);
}
#endif  // !defined(OS_WIN)

IN_PROC_BROWSER_TEST_F(PerformanceMonitorBrowserTest, RendererCrashEvent) {
  content::WindowedNotificationObserver windowed_observer(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::NotificationService::AllSources());

  ui_test_utils::NavigateToURL(browser(), GURL(content::kChromeUICrashURL));

  windowed_observer.Wait();

  Database::EventVector events = GetEvents();
  ASSERT_EQ(1u, events.size());

  CheckEventType(EVENT_RENDERER_CRASH, events[0]);

  std::string url;
  ASSERT_TRUE(events[0]->data()->GetString("url", &url));
  ASSERT_EQ("chrome://crash/", url);
}

IN_PROC_BROWSER_TEST_F(PerformanceMonitorUncleanExitBrowserTest,
                       OneProfileUncleanExit) {
  // Initialize the database value (if there's no value in the database, it
  // can't determine the last active time of the profile, and doesn't insert
  // the event).
  const std::string time = "12985807272597591";
  AddStateValue(kStateProfilePrefix + first_profile_name_, time);

  performance_monitor()->CheckForUncleanExits();
  content::RunAllPendingInMessageLoop();

  Database::EventVector events = GetEvents();

  const size_t kNumEvents = 1;
  ASSERT_EQ(kNumEvents, events.size());

  CheckEventType(EVENT_UNCLEAN_EXIT, events[0]);

  std::string event_profile;
  ASSERT_TRUE(events[0]->data()->GetString("profileName", &event_profile));
  ASSERT_EQ(first_profile_name_, event_profile);
}

IN_PROC_BROWSER_TEST_F(PerformanceMonitorUncleanExitBrowserTest,
                       TwoProfileUncleanExit) {
  base::FilePath second_profile_path;
  PathService::Get(chrome::DIR_USER_DATA, &second_profile_path);
  second_profile_path = second_profile_path.AppendASCII(second_profile_name_);

  const std::string time1 = "12985807272597591";
  const std::string time2 = "12985807272599918";

  // Initialize the database.
  AddStateValue(kStateProfilePrefix + first_profile_name_, time1);
  AddStateValue(kStateProfilePrefix + second_profile_name_, time2);

  performance_monitor()->CheckForUncleanExits();
  content::RunAllPendingInMessageLoop();

  // Load the second profile, which has also exited uncleanly. Note that since
  // the second profile is new, component extensions will be installed as part
  // of the browser startup for that profile, generating extra events.
  g_browser_process->profile_manager()->GetProfile(second_profile_path);
  content::RunAllPendingInMessageLoop();

  Database::EventVector events = GetEvents();

  const size_t kNumUncleanExitEvents = 2;
  size_t num_unclean_exit_events = 0;
  for (size_t i = 0; i < events.size(); ++i) {
    int event_type = -1;
    if (events[i]->data()->GetInteger("eventType", &event_type) &&
        event_type == EVENT_EXTENSION_INSTALL) {
      continue;
    }
    CheckEventType(EVENT_UNCLEAN_EXIT, events[i]);
    ++num_unclean_exit_events;
  }
  ASSERT_EQ(kNumUncleanExitEvents, num_unclean_exit_events);

  std::string event_profile;
  ASSERT_TRUE(events[0]->data()->GetString("profileName", &event_profile));
  ASSERT_EQ(first_profile_name_, event_profile);

  ASSERT_TRUE(events[1]->data()->GetString("profileName", &event_profile));
  ASSERT_EQ(second_profile_name_, event_profile);
}

IN_PROC_BROWSER_TEST_F(PerformanceMonitorBrowserTest, StartupTime) {
  Database::MetricVector metrics = GetStats(METRIC_TEST_STARTUP_TIME);

  ASSERT_EQ(1u, metrics.size());
  ASSERT_LT(metrics[0].value, kMaxStartupTime.ToInternalValue());
}

IN_PROC_BROWSER_TEST_F(PerformanceMonitorSessionRestoreBrowserTest,
                       StartupWithSessionRestore) {
  ui_test_utils::NavigateToURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath(base::FilePath::kCurrentDirectory),
                     base::FilePath(FILE_PATH_LITERAL("title1.html"))));

  QuitBrowserAndRestore(browser(), 1);

  Database::MetricVector metrics = GetStats(METRIC_TEST_STARTUP_TIME);
  ASSERT_EQ(1u, metrics.size());
  ASSERT_LT(metrics[0].value, kMaxStartupTime.ToInternalValue());

  metrics = GetStats(METRIC_SESSION_RESTORE_TIME);
  ASSERT_EQ(1u, metrics.size());
  ASSERT_LT(metrics[0].value, kMaxStartupTime.ToInternalValue());
}

IN_PROC_BROWSER_TEST_F(PerformanceMonitorBrowserTest, PageLoadTime) {
  const base::TimeDelta kMaxLoadTime = base::TimeDelta::FromSeconds(30);

  ui_test_utils::NavigateToURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath(base::FilePath::kCurrentDirectory),
                     base::FilePath(FILE_PATH_LITERAL("title1.html"))));

  ui_test_utils::NavigateToURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath(base::FilePath::kCurrentDirectory),
                     base::FilePath(FILE_PATH_LITERAL("title1.html"))));

  Database::MetricVector metrics = GetStats(METRIC_PAGE_LOAD_TIME);

  ASSERT_EQ(2u, metrics.size());
  ASSERT_LT(metrics[0].value, kMaxLoadTime.ToInternalValue());
  ASSERT_LT(metrics[1].value, kMaxLoadTime.ToInternalValue());
}

IN_PROC_BROWSER_TEST_F(PerformanceMonitorBrowserTest, NetworkBytesRead) {
  base::FilePath test_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &test_dir);

  int64 page1_size = 0;
  ASSERT_TRUE(file_util::GetFileSize(test_dir.AppendASCII("title1.html"),
                                     &page1_size));

  int64 page2_size = 0;
  ASSERT_TRUE(file_util::GetFileSize(test_dir.AppendASCII("title2.html"),
                                     &page2_size));

  ASSERT_TRUE(test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(),
      test_server()->GetURL(std::string("files/").append("title1.html")));

  performance_monitor()->DoTimedCollections();

  // Since network bytes are read and set on the IO thread, we must flush this
  // additional thread to be sure that all messages are run.
  RunAllPendingInMessageLoop(content::BrowserThread::IO);

  Database::MetricVector metrics = GetStats(METRIC_NETWORK_BYTES_READ);
  ASSERT_EQ(1u, metrics.size());
  // Since these pages are read over the "network" (actually the test_server),
  // some extraneous information is carried along, and the best check we can do
  // is for greater than or equal to.
  EXPECT_GE(metrics[0].value, page1_size);

  ui_test_utils::NavigateToURL(
      browser(),
      test_server()->GetURL(std::string("files/").append("title2.html")));

  performance_monitor()->DoTimedCollections();

  metrics = GetStats(METRIC_NETWORK_BYTES_READ);
  ASSERT_EQ(2u, metrics.size());
  EXPECT_GE(metrics[1].value, page1_size + page2_size);
}

}  // namespace performance_monitor
