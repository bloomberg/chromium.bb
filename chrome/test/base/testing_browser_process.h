// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An implementation of BrowserProcess for unit tests that fails for most
// services. By preventing creation of services, we reduce dependencies and
// keep the profile clean. Clients of this class must handle the NULL return
// value, however.

#ifndef CHROME_TEST_BASE_TESTING_BROWSER_PROCESS_H_
#define CHROME_TEST_BASE_TESTING_BROWSER_PROCESS_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"

class BackgroundModeManager;
class CRLSetFetcher;
class IOThread;
class MHTMLGenerationManager;
class NotificationUIManager;
class PrefService;
class WatchDogThread;

namespace content {
class NotificationService;
}

namespace extensions {
class ExtensionsBrowserClient;
}

namespace gcm {
class GCMDriver;
}

namespace policy {
class BrowserPolicyConnector;
class PolicyService;
}

namespace prerender {
class PrerenderTracker;
}

class TestingBrowserProcess : public BrowserProcess {
 public:
  // Initializes |g_browser_process| with a new TestingBrowserProcess.
  static void CreateInstance();

  // Cleanly destroys |g_browser_process|, which has special deletion semantics.
  static void DeleteInstance();

  // Convenience method to get g_browser_process as a TestingBrowserProcess*.
  static TestingBrowserProcess* GetGlobal();

  virtual void ResourceDispatcherHostCreated() override;
  virtual void EndSession() override;
  virtual MetricsServicesManager* GetMetricsServicesManager() override;
  virtual metrics::MetricsService* metrics_service() override;
  virtual rappor::RapporService* rappor_service() override;
  virtual IOThread* io_thread() override;
  virtual WatchDogThread* watchdog_thread() override;
  virtual ProfileManager* profile_manager() override;
  virtual PrefService* local_state() override;
  virtual chrome_variations::VariationsService* variations_service() override;
  virtual policy::BrowserPolicyConnector* browser_policy_connector() override;
  virtual policy::PolicyService* policy_service() override;
  virtual IconManager* icon_manager() override;
  virtual GLStringManager* gl_string_manager() override;
  virtual GpuModeManager* gpu_mode_manager() override;
  virtual BackgroundModeManager* background_mode_manager() override;
  virtual void set_background_mode_manager_for_test(
      scoped_ptr<BackgroundModeManager> manager) override;
  virtual StatusTray* status_tray() override;
  virtual SafeBrowsingService* safe_browsing_service() override;
  virtual safe_browsing::ClientSideDetectionService*
      safe_browsing_detection_service() override;
  virtual net::URLRequestContextGetter* system_request_context() override;
  virtual BrowserProcessPlatformPart* platform_part() override;

  virtual extensions::EventRouterForwarder*
      extension_event_router_forwarder() override;
  virtual NotificationUIManager* notification_ui_manager() override;
  virtual message_center::MessageCenter* message_center() override;
  virtual IntranetRedirectDetector* intranet_redirect_detector() override;
  virtual void CreateDevToolsHttpProtocolHandler(
      chrome::HostDesktopType host_desktop_type,
      const std::string& ip,
      int port) override;
  virtual unsigned int AddRefModule() override;
  virtual unsigned int ReleaseModule() override;
  virtual bool IsShuttingDown() override;
  virtual printing::PrintJobManager* print_job_manager() override;
  virtual printing::PrintPreviewDialogController*
      print_preview_dialog_controller() override;
  virtual printing::BackgroundPrintingManager*
      background_printing_manager() override;
  virtual const std::string& GetApplicationLocale() override;
  virtual void SetApplicationLocale(const std::string& app_locale) override;
  virtual DownloadStatusUpdater* download_status_updater() override;
  virtual DownloadRequestLimiter* download_request_limiter() override;

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
  virtual void StartAutoupdateTimer() override {}
#endif

  virtual ChromeNetLog* net_log() override;
  virtual prerender::PrerenderTracker* prerender_tracker() override;
  virtual component_updater::ComponentUpdateService*
      component_updater() override;
  virtual CRLSetFetcher* crl_set_fetcher() override;
  virtual component_updater::PnaclComponentInstaller*
      pnacl_component_installer() override;
  virtual MediaFileSystemRegistry* media_file_system_registry() override;
  virtual bool created_local_state() const override;

#if defined(ENABLE_WEBRTC)
  virtual WebRtcLogUploader* webrtc_log_uploader() override;
#endif

  virtual network_time::NetworkTimeTracker* network_time_tracker() override;

  virtual gcm::GCMDriver* gcm_driver() override;

  // Set the local state for tests. Consumer is responsible for cleaning it up
  // afterwards (using ScopedTestingLocalState, for example).
  void SetLocalState(PrefService* local_state);
  void SetProfileManager(ProfileManager* profile_manager);
  void SetIOThread(IOThread* io_thread);
  void SetBrowserPolicyConnector(policy::BrowserPolicyConnector* connector);
  void SetSafeBrowsingService(SafeBrowsingService* sb_service);
  void SetSystemRequestContext(net::URLRequestContextGetter* context_getter);

 private:
  // See CreateInstance() and DestoryInstance() above.
  TestingBrowserProcess();
  virtual ~TestingBrowserProcess();

  scoped_ptr<content::NotificationService> notification_service_;
  unsigned int module_ref_count_;
  std::string app_locale_;

  // TODO(ios): Add back members as more code is compiled.
#if !defined(OS_IOS)
#if defined(ENABLE_CONFIGURATION_POLICY)
  scoped_ptr<policy::BrowserPolicyConnector> browser_policy_connector_;
#else
  scoped_ptr<policy::PolicyService> policy_service_;
#endif
  scoped_ptr<ProfileManager> profile_manager_;
  scoped_ptr<NotificationUIManager> notification_ui_manager_;

#if defined(ENABLE_PRINTING)
  scoped_ptr<printing::PrintJobManager> print_job_manager_;
#endif

#if defined(ENABLE_FULL_PRINTING)
  scoped_ptr<printing::BackgroundPrintingManager> background_printing_manager_;
  scoped_refptr<printing::PrintPreviewDialogController>
      print_preview_dialog_controller_;
#endif

  scoped_ptr<prerender::PrerenderTracker> prerender_tracker_;
  scoped_refptr<SafeBrowsingService> sb_service_;
#endif  // !defined(OS_IOS)

  scoped_ptr<network_time::NetworkTimeTracker> network_time_tracker_;

  // The following objects are not owned by TestingBrowserProcess:
  PrefService* local_state_;
  IOThread* io_thread_;
  net::URLRequestContextGetter* system_request_context_;

  scoped_ptr<BrowserProcessPlatformPart> platform_part_;

#if defined(ENABLE_EXTENSIONS)
  scoped_ptr<MediaFileSystemRegistry> media_file_system_registry_;

  scoped_ptr<extensions::ExtensionsBrowserClient> extensions_browser_client_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TestingBrowserProcess);
};

// RAII (resource acquisition is initialization) for TestingBrowserProcess.
// Allows you to initialize TestingBrowserProcess/NotificationService before
// other member variables.
//
// This can be helpful if you are running a unit test inside the browser_tests
// suite because browser_tests do not make a TestingBrowserProcess for you.
//
// class MyUnitTestRunningAsBrowserTest : public testing::Test {
//  ...stuff...
//  private:
//   TestingBrowserProcessInitializer initializer_;
//   LocalState local_state_;  // Needs a BrowserProcess to initialize.
//   NotificationRegistrar registar_;  // Needs NotificationService.
// };
class TestingBrowserProcessInitializer {
 public:
  TestingBrowserProcessInitializer();
  ~TestingBrowserProcessInitializer();

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingBrowserProcessInitializer);
};

#endif  // CHROME_TEST_BASE_TESTING_BROWSER_PROCESS_H_
