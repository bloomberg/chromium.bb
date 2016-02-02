// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// When each service is created, we set a flag indicating this. At this point,
// the service initialization could fail or succeed. This allows us to remember
// if we tried to create a service, and not try creating it over and over if
// the creation failed.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_IMPL_H_
#define CHROME_BROWSER_BROWSER_PROCESS_IMPL_H_

#include <stdint.h>

#include <string>

#include "base/debug/stack_trace.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "components/prefs/pref_change_registrar.h"

class ChromeChildProcessWatcher;
class ChromeDeviceClient;
class ChromeResourceDispatcherHostDelegate;
class RemoteDebuggingServer;
class PrefRegistrySimple;

#if defined(ENABLE_PLUGIN_INSTALLATION)
class PluginsResourceService;
#endif

namespace base {
class CommandLine;
class SequencedTaskRunner;
}

namespace extensions {
class ExtensionsBrowserClient;
}

namespace gcm {
class GCMDriver;
}

namespace net_log {
class ChromeNetLog;
}

namespace policy {
class BrowserPolicyConnector;
class PolicyService;
};

namespace web_resource {
class PromoResourceService;
}

// Real implementation of BrowserProcess that creates and returns the services.
class BrowserProcessImpl : public BrowserProcess,
                           public base::NonThreadSafe {
 public:
  // |local_state_task_runner| must be a shutdown-blocking task runner.
  BrowserProcessImpl(base::SequencedTaskRunner* local_state_task_runner,
                     const base::CommandLine& command_line);
  ~BrowserProcessImpl() override;

  // Called before the browser threads are created.
  void PreCreateThreads();

  // Called after the threads have been created but before the message loops
  // starts running. Allows the browser process to do any initialization that
  // requires all threads running.
  void PreMainMessageLoopRun();

  // Most cleanup is done by these functions, driven from
  // ChromeBrowserMain based on notifications from the content
  // framework, rather than in the destructor, so that we can
  // interleave cleanup with threads being stopped.
#if !defined(OS_ANDROID)
  void StartTearDown();
  void PostDestroyThreads();
#endif

  // BrowserProcess implementation.
  void ResourceDispatcherHostCreated() override;
  void EndSession() override;
  metrics_services_manager::MetricsServicesManager* GetMetricsServicesManager()
      override;
  metrics::MetricsService* metrics_service() override;
  rappor::RapporService* rappor_service() override;
  IOThread* io_thread() override;
  WatchDogThread* watchdog_thread() override;
  ProfileManager* profile_manager() override;
  PrefService* local_state() override;
  net::URLRequestContextGetter* system_request_context() override;
  variations::VariationsService* variations_service() override;
  web_resource::PromoResourceService* promo_resource_service() override;
  BrowserProcessPlatformPart* platform_part() override;
  extensions::EventRouterForwarder* extension_event_router_forwarder() override;
  NotificationUIManager* notification_ui_manager() override;
  message_center::MessageCenter* message_center() override;
  policy::BrowserPolicyConnector* browser_policy_connector() override;
  policy::PolicyService* policy_service() override;
  IconManager* icon_manager() override;
  GLStringManager* gl_string_manager() override;
  GpuModeManager* gpu_mode_manager() override;
  void CreateDevToolsHttpProtocolHandler(
      chrome::HostDesktopType host_desktop_type,
      const std::string& ip,
      uint16_t port) override;
  unsigned int AddRefModule() override;
  unsigned int ReleaseModule() override;
  bool IsShuttingDown() override;
  printing::PrintJobManager* print_job_manager() override;
  printing::PrintPreviewDialogController* print_preview_dialog_controller()
      override;
  printing::BackgroundPrintingManager* background_printing_manager() override;
  IntranetRedirectDetector* intranet_redirect_detector() override;
  const std::string& GetApplicationLocale() override;
  void SetApplicationLocale(const std::string& locale) override;
  DownloadStatusUpdater* download_status_updater() override;
  DownloadRequestLimiter* download_request_limiter() override;
  BackgroundModeManager* background_mode_manager() override;
  void set_background_mode_manager_for_test(
      scoped_ptr<BackgroundModeManager> manager) override;
  StatusTray* status_tray() override;
  safe_browsing::SafeBrowsingService* safe_browsing_service() override;
  safe_browsing::ClientSideDetectionService* safe_browsing_detection_service()
      override;

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
  void StartAutoupdateTimer() override;
#endif

  net_log::ChromeNetLog* net_log() override;
  component_updater::ComponentUpdateService* component_updater() override;
  CRLSetFetcher* crl_set_fetcher() override;
  component_updater::PnaclComponentInstaller* pnacl_component_installer()
      override;
  component_updater::SupervisedUserWhitelistInstaller*
  supervised_user_whitelist_installer() override;
  MediaFileSystemRegistry* media_file_system_registry() override;
  bool created_local_state() const override;
#if defined(ENABLE_WEBRTC)
  WebRtcLogUploader* webrtc_log_uploader() override;
#endif
  network_time::NetworkTimeTracker* network_time_tracker() override;
  gcm::GCMDriver* gcm_driver() override;
  memory::TabManager* GetTabManager() override;
  ShellIntegration::DefaultWebClientState CachedDefaultWebClientState()
      override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  void CreateWatchdogThread();
  void CreateProfileManager();
  void CreateLocalState();
  void CreateViewedPageTracker();
  void CreateIconManager();
  void CreateIntranetRedirectDetector();
  void CreateNotificationUIManager();
  void CreateStatusTrayManager();
  void CreatePrintPreviewDialogController();
  void CreateBackgroundPrintingManager();
  void CreateSafeBrowsingService();
  void CreateSafeBrowsingDetectionService();
  void CreateStatusTray();
  void CreateBackgroundModeManager();
  void CreateGCMDriver();

  void ApplyAllowCrossOriginAuthPromptPolicy();
  void ApplyDefaultBrowserPolicy();
  void ApplyMetricsReportingPolicy();

  void CacheDefaultWebClientState();

  scoped_ptr<metrics_services_manager::MetricsServicesManager>
      metrics_services_manager_;

  scoped_ptr<IOThread> io_thread_;

  bool created_watchdog_thread_;
  scoped_ptr<WatchDogThread> watchdog_thread_;

  bool created_browser_policy_connector_;
#if defined(ENABLE_CONFIGURATION_POLICY)
  // Must be destroyed after |local_state_|.
  scoped_ptr<policy::BrowserPolicyConnector> browser_policy_connector_;
#else
  // Must be destroyed after |local_state_|.
  // This is a stub when policy is not enabled. Otherwise, the PolicyService
  // is owned by the |browser_policy_connector_| and this is not used.
  scoped_ptr<policy::PolicyService> policy_service_;
#endif

  bool created_profile_manager_;
  scoped_ptr<ProfileManager> profile_manager_;

  bool created_local_state_;
  scoped_ptr<PrefService> local_state_;

  bool created_icon_manager_;
  scoped_ptr<IconManager> icon_manager_;

  scoped_ptr<GLStringManager> gl_string_manager_;

  scoped_ptr<GpuModeManager> gpu_mode_manager_;

#if defined(ENABLE_EXTENSIONS)
  scoped_ptr<extensions::ExtensionsBrowserClient> extensions_browser_client_;

  scoped_refptr<extensions::EventRouterForwarder>
      extension_event_router_forwarder_;

  scoped_ptr<MediaFileSystemRegistry> media_file_system_registry_;
#endif

#if !defined(OS_ANDROID)
  scoped_ptr<RemoteDebuggingServer> remote_debugging_server_;
#endif

#if defined(ENABLE_PRINT_PREVIEW)
  scoped_refptr<printing::PrintPreviewDialogController>
      print_preview_dialog_controller_;

  scoped_ptr<printing::BackgroundPrintingManager> background_printing_manager_;
#endif

  // Manager for desktop notification UI.
  bool created_notification_ui_manager_;
  scoped_ptr<NotificationUIManager> notification_ui_manager_;

  scoped_ptr<IntranetRedirectDetector> intranet_redirect_detector_;

  scoped_ptr<StatusTray> status_tray_;

#if defined(ENABLE_BACKGROUND)
  scoped_ptr<BackgroundModeManager> background_mode_manager_;
#endif

  bool created_safe_browsing_service_;
  scoped_refptr<safe_browsing::SafeBrowsingService> safe_browsing_service_;

  unsigned int module_ref_count_;
  bool did_start_;

  bool tearing_down_;

  // Ensures that all the print jobs are finished before closing the browser.
  scoped_ptr<printing::PrintJobManager> print_job_manager_;

  std::string locale_;

  // Download status updates (like a changing application icon on dock/taskbar)
  // are global per-application. DownloadStatusUpdater does no work in the ctor
  // so we don't have to worry about lazy initialization.
  scoped_ptr<DownloadStatusUpdater> download_status_updater_;

  scoped_refptr<DownloadRequestLimiter> download_request_limiter_;

  // Sequenced task runner for local state related I/O tasks.
  const scoped_refptr<base::SequencedTaskRunner> local_state_task_runner_;

  // Ensures that the observers of plugin/print disable/enable state
  // notifications are properly added and removed.
  PrefChangeRegistrar pref_change_registrar_;

  // Lives here so can safely log events on shutdown.
  scoped_ptr<net_log::ChromeNetLog> net_log_;

  scoped_ptr<ChromeResourceDispatcherHostDelegate>
      resource_dispatcher_host_delegate_;

  scoped_ptr<web_resource::PromoResourceService> promo_resource_service_;

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
  base::RepeatingTimer autoupdate_timer_;

  // Gets called by autoupdate timer to see if browser needs restart and can be
  // restarted, and if that's the case, restarts the browser.
  void OnAutoupdateTimer();
  bool CanAutorestartForUpdate() const;
  void RestartBackgroundInstance();
#endif  // defined(OS_WIN) || defined(OS_LINUX) && !defined(OS_CHROMEOS)

  // component updater is normally not used under ChromeOS due
  // to concerns over integrity of data shared between profiles,
  // but some users of component updater only install per-user.
  scoped_ptr<component_updater::ComponentUpdateService> component_updater_;
  scoped_refptr<CRLSetFetcher> crl_set_fetcher_;

#if !defined(DISABLE_NACL)
  scoped_refptr<component_updater::PnaclComponentInstaller>
      pnacl_component_installer_;
#endif

  scoped_ptr<component_updater::SupervisedUserWhitelistInstaller>
      supervised_user_whitelist_installer_;

#if defined(ENABLE_PLUGIN_INSTALLATION)
  scoped_ptr<PluginsResourceService> plugins_resource_service_;
#endif

  scoped_ptr<BrowserProcessPlatformPart> platform_part_;

  // TODO(eroman): Remove this when done debugging 113031. This tracks
  // the callstack which released the final module reference count.
  base::debug::StackTrace release_last_reference_callstack_;

#if defined(ENABLE_WEBRTC)
  // Lazily initialized.
  scoped_ptr<WebRtcLogUploader> webrtc_log_uploader_;
#endif

  scoped_ptr<network_time::NetworkTimeTracker> network_time_tracker_;

  scoped_ptr<gcm::GCMDriver> gcm_driver_;

  scoped_ptr<ChromeChildProcessWatcher> child_process_watcher_;

  scoped_ptr<ChromeDeviceClient> device_client_;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  // Any change to this #ifdef must be reflected as well in
  // chrome/browser/memory/tab_manager_browsertest.cc
  scoped_ptr<memory::TabManager> tab_manager_;
#endif

  ShellIntegration::DefaultWebClientState cached_default_web_client_state_;

  DISALLOW_COPY_AND_ASSIGN(BrowserProcessImpl);
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_IMPL_H_
