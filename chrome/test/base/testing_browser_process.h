// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An implementation of BrowserProcess for unit tests that fails for most
// services. By preventing creation of services, we reduce dependencies and
// keep the profile clean. Clients of this class must handle the NULL return
// value, however.

#ifndef CHROME_TEST_BASE_TESTING_BROWSER_PROCESS_H_
#define CHROME_TEST_BASE_TESTING_BROWSER_PROCESS_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "content/browser/notification_service_impl.h"

class BackgroundModeManager;
class CRLSetFetcher;
class IOThread;
class GoogleURLTracker;
class MHTMLGenerationManager;
class NotificationUIManager;
class PrefService;
class WatchDogThread;

namespace policy {
class BrowserPolicyConnector;
}

namespace prerender {
class PrerenderTracker;
}

namespace ui {
class Clipboard;
}

class TestingBrowserProcess : public BrowserProcess {
 public:
  TestingBrowserProcess();
  virtual ~TestingBrowserProcess();

  virtual void EndSession() OVERRIDE;
  virtual ResourceDispatcherHost* resource_dispatcher_host() OVERRIDE;
  virtual MetricsService* metrics_service() OVERRIDE;
  virtual IOThread* io_thread() OVERRIDE;

  virtual base::Thread* file_thread() OVERRIDE;
  virtual base::Thread* db_thread() OVERRIDE;
  virtual base::Thread* cache_thread() OVERRIDE;
  virtual WatchDogThread* watchdog_thread() OVERRIDE;

#if defined(OS_CHROMEOS)
  virtual base::Thread* web_socket_proxy_thread() OVERRIDE;
#endif

  virtual ProfileManager* profile_manager() OVERRIDE;
  virtual PrefService* local_state() OVERRIDE;
  virtual policy::BrowserPolicyConnector* browser_policy_connector() OVERRIDE;
  virtual IconManager* icon_manager() OVERRIDE;
  virtual ThumbnailGenerator* GetThumbnailGenerator() OVERRIDE;
  virtual DevToolsManager* devtools_manager() OVERRIDE;
  virtual SidebarManager* sidebar_manager() OVERRIDE;
  virtual TabCloseableStateWatcher* tab_closeable_state_watcher() OVERRIDE;
  virtual BackgroundModeManager* background_mode_manager() OVERRIDE;
  virtual StatusTray* status_tray() OVERRIDE;
  virtual SafeBrowsingService* safe_browsing_service() OVERRIDE;
  virtual safe_browsing::ClientSideDetectionService*
      safe_browsing_detection_service() OVERRIDE;
  virtual net::URLRequestContextGetter* system_request_context() OVERRIDE;

#if defined(OS_CHROMEOS)
  virtual browser::OomPriorityManager* oom_priority_manager() OVERRIDE;
#endif  // defined(OS_CHROMEOS)

  virtual ui::Clipboard* clipboard() OVERRIDE;
  virtual ExtensionEventRouterForwarder*
      extension_event_router_forwarder() OVERRIDE;
  virtual NotificationUIManager* notification_ui_manager() OVERRIDE;
  virtual GoogleURLTracker* google_url_tracker() OVERRIDE;
  virtual IntranetRedirectDetector* intranet_redirect_detector() OVERRIDE;
  virtual AutomationProviderList* GetAutomationProviderList() OVERRIDE;
  virtual void InitDevToolsHttpProtocolHandler(
      Profile* profile,
      const std::string& ip,
      int port,
      const std::string& frontend_url) OVERRIDE;
  virtual void InitDevToolsLegacyProtocolHandler(int port) OVERRIDE;
  virtual unsigned int AddRefModule() OVERRIDE;
  virtual unsigned int ReleaseModule() OVERRIDE;
  virtual bool IsShuttingDown() OVERRIDE;
  virtual printing::PrintJobManager* print_job_manager() OVERRIDE;
  virtual printing::PrintPreviewTabController*
      print_preview_tab_controller() OVERRIDE;
  virtual printing::BackgroundPrintingManager*
      background_printing_manager() OVERRIDE;
  virtual const std::string& GetApplicationLocale() OVERRIDE;
  virtual void SetApplicationLocale(const std::string& app_locale) OVERRIDE;
  virtual DownloadStatusUpdater* download_status_updater() OVERRIDE;
  virtual DownloadRequestLimiter* download_request_limiter() OVERRIDE;
  virtual bool plugin_finder_disabled() const OVERRIDE;

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
  virtual void StartAutoupdateTimer() OVERRIDE {}
#endif

  virtual ChromeNetLog* net_log() OVERRIDE;
  virtual prerender::PrerenderTracker* prerender_tracker() OVERRIDE;
  virtual MHTMLGenerationManager* mhtml_generation_manager() OVERRIDE;
  virtual GpuBlacklistUpdater* gpu_blacklist_updater() OVERRIDE;
  virtual ComponentUpdateService* component_updater() OVERRIDE;
  virtual CRLSetFetcher* crl_set_fetcher() OVERRIDE;

  // Set the local state for tests. Consumer is responsible for cleaning it up
  // afterwards (using ScopedTestingLocalState, for example).
  void SetLocalState(PrefService* local_state);
  void SetGoogleURLTracker(GoogleURLTracker* google_url_tracker);
  void SetProfileManager(ProfileManager* profile_manager);
  void SetIOThread(IOThread* io_thread);
  void SetDevToolsManager(DevToolsManager*);
  void SetBrowserPolicyConnector(policy::BrowserPolicyConnector* connector);

 private:
  NotificationServiceImpl notification_service_;
  unsigned int module_ref_count_;
  scoped_ptr<ui::Clipboard> clipboard_;
  std::string app_locale_;

  // Weak pointer.
  PrefService* local_state_;
  scoped_ptr<policy::BrowserPolicyConnector> browser_policy_connector_;
  scoped_ptr<GoogleURLTracker> google_url_tracker_;
  scoped_ptr<ProfileManager> profile_manager_;
  scoped_ptr<NotificationUIManager> notification_ui_manager_;
  scoped_ptr<printing::BackgroundPrintingManager> background_printing_manager_;
  scoped_ptr<prerender::PrerenderTracker> prerender_tracker_;
  IOThread* io_thread_;
  scoped_ptr<DevToolsManager> devtools_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestingBrowserProcess);
};

#endif  // CHROME_TEST_BASE_TESTING_BROWSER_PROCESS_H_
