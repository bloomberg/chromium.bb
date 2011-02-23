// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An implementation of BrowserProcess for unit tests that fails for most
// services. By preventing creation of services, we reduce dependencies and
// keep the profile clean. Clients of this class must handle the NULL return
// value, however.

#ifndef CHROME_TEST_TESTING_BROWSER_PROCESS_H_
#define CHROME_TEST_TESTING_BROWSER_PROCESS_H_
#pragma once

#include "build/build_config.h"

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/notification_service.h"

class IOThread;
class GoogleURLTracker;
class PrefService;
class WatchDogThread;

namespace base {
class WaitableEvent;
}

namespace policy {
class BrowserPolicyConnector;
}

namespace ui {
class Clipboard;
}

class TestingBrowserProcess : public BrowserProcess {
 public:
  TestingBrowserProcess();
  virtual ~TestingBrowserProcess();

  virtual void EndSession();

  virtual ResourceDispatcherHost* resource_dispatcher_host();

  virtual MetricsService* metrics_service();

  virtual IOThread* io_thread();

#if defined(OS_LINUX)
  virtual base::Thread* background_x11_thread();
#endif

  virtual base::Thread* file_thread();

  virtual base::Thread* db_thread();

  virtual base::Thread* cache_thread();

  virtual WatchDogThread* watchdog_thread();

  virtual ProfileManager* profile_manager();

  virtual PrefService* local_state();

  virtual policy::BrowserPolicyConnector* browser_policy_connector();

  virtual IconManager* icon_manager();

  virtual ThumbnailGenerator* GetThumbnailGenerator();

  virtual DevToolsManager* devtools_manager();

  virtual SidebarManager* sidebar_manager();

  virtual TabCloseableStateWatcher* tab_closeable_state_watcher();

  virtual safe_browsing::ClientSideDetectionService*
      safe_browsing_detection_service();

  virtual ui::Clipboard* clipboard();

  virtual NotificationUIManager* notification_ui_manager();

  virtual GoogleURLTracker* google_url_tracker();

  virtual IntranetRedirectDetector* intranet_redirect_detector();

  virtual AutomationProviderList* InitAutomationProviderList();

  virtual void InitDevToolsHttpProtocolHandler(
      const std::string& ip,
      int port,
      const std::string& frontend_url);

  virtual void InitDevToolsLegacyProtocolHandler(int port);

  virtual unsigned int AddRefModule();
  virtual unsigned int ReleaseModule();

  virtual bool IsShuttingDown();

  virtual printing::PrintJobManager* print_job_manager();

  virtual printing::PrintPreviewTabController* print_preview_tab_controller();

  virtual const std::string& GetApplicationLocale();

  virtual void SetApplicationLocale(const std::string& app_locale);

  virtual DownloadStatusUpdater* download_status_updater();

  virtual base::WaitableEvent* shutdown_event();

  virtual void CheckForInspectorFiles() {}

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
  virtual void StartAutoupdateTimer() {}
#endif

  virtual bool have_inspector_files() const;

  virtual ChromeNetLog* net_log();

#if defined(IPC_MESSAGE_LOG_ENABLED)
  virtual void SetIPCLoggingEnabled(bool enable) {}
#endif

  void SetPrefService(PrefService* pref_service);
  void SetGoogleURLTracker(GoogleURLTracker* google_url_tracker);
  void SetProfileManager(ProfileManager* profile_manager);

 private:
  NotificationService notification_service_;
  scoped_ptr<base::WaitableEvent> shutdown_event_;
  unsigned int module_ref_count_;
  scoped_ptr<ui::Clipboard> clipboard_;
  std::string app_locale_;

  PrefService* pref_service_;
  scoped_ptr<policy::BrowserPolicyConnector> browser_policy_connector_;
  scoped_ptr<GoogleURLTracker> google_url_tracker_;
  scoped_ptr<ProfileManager> profile_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestingBrowserProcess);
};

// Scoper to put a TestingBrowserProcess in |g_browser_process|.
class ScopedTestingBrowserProcess {
 public:
  ScopedTestingBrowserProcess();
  ~ScopedTestingBrowserProcess();

  TestingBrowserProcess* get() {
    return browser_process_.get();
  }

 private:
  // TODO(phajdan.jr): Temporary, for http://crbug.com/61062.
  // After the transition is over, we should just stack-allocate it.
  scoped_ptr<TestingBrowserProcess> browser_process_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTestingBrowserProcess);
};

#endif  // CHROME_TEST_TESTING_BROWSER_PROCESS_H_
