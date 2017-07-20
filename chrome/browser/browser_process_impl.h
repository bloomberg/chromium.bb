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

#include <memory>
#include <string>

#include "base/debug/stack_trace.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/keep_alive_state_observer.h"
#include "chrome/common/features.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/features/features.h"
#include "media/media_features.h"
#include "ppapi/features/features.h"
#include "printing/features/features.h"

class ChromeChildProcessWatcher;
class ChromeDeviceClient;
class ChromeResourceDispatcherHostDelegate;
class DevToolsAutoOpener;
class RemoteDebuggingServer;
class PrefRegistrySimple;

#if BUILDFLAG(ENABLE_PLUGINS)
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

// Real implementation of BrowserProcess that creates and returns the services.
class BrowserProcessImpl : public BrowserProcess,
                           public KeepAliveStateObserver {
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
  rappor::RapporServiceImpl* rappor_service() override;
  ukm::UkmRecorder* ukm_recorder() override;
  IOThread* io_thread() override;
  WatchDogThread* watchdog_thread() override;
  ProfileManager* profile_manager() override;
  PrefService* local_state() override;
  net::URLRequestContextGetter* system_request_context() override;
  variations::VariationsService* variations_service() override;
  BrowserProcessPlatformPart* platform_part() override;
  extensions::EventRouterForwarder* extension_event_router_forwarder() override;
  NotificationUIManager* notification_ui_manager() override;
  NotificationPlatformBridge* notification_platform_bridge() override;
  message_center::MessageCenter* message_center() override;
  policy::BrowserPolicyConnector* browser_policy_connector() override;
  policy::PolicyService* policy_service() override;
  IconManager* icon_manager() override;
  GpuModeManager* gpu_mode_manager() override;
  GpuProfileCache* gpu_profile_cache() override;
  void CreateDevToolsHttpProtocolHandler(const std::string& ip,
                                         uint16_t port) override;
  void CreateDevToolsAutoOpener() override;
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
      std::unique_ptr<BackgroundModeManager> manager) override;
  StatusTray* status_tray() override;
  safe_browsing::SafeBrowsingService* safe_browsing_service() override;
  safe_browsing::ClientSideDetectionService* safe_browsing_detection_service()
      override;
  subresource_filter::ContentRulesetService*
  subresource_filter_ruleset_service() override;

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
#if BUILDFLAG(ENABLE_WEBRTC)
  WebRtcLogUploader* webrtc_log_uploader() override;
#endif
  network_time::NetworkTimeTracker* network_time_tracker() override;
  gcm::GCMDriver* gcm_driver() override;
  resource_coordinator::TabManager* GetTabManager() override;
  shell_integration::DefaultWebClientState CachedDefaultWebClientState()
      override;
  physical_web::PhysicalWebDataSource* GetPhysicalWebDataSource() override;
  prefs::InProcessPrefServiceFactory* pref_service_factory() const override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // KeepAliveStateObserver implementation
  void OnKeepAliveStateChanged(bool is_keeping_alive) override;
  void OnKeepAliveRestartStateChanged(bool can_restart) override;

  void CreateWatchdogThread();
  void CreateProfileManager();
  void CreateLocalState();
  void CreateViewedPageTracker();
  void CreateIconManager();
  void CreateIntranetRedirectDetector();
  void CreateNotificationPlatformBridge();
  void CreateNotificationUIManager();
  void CreateStatusTrayManager();
  void CreatePrintPreviewDialogController();
  void CreateBackgroundPrintingManager();
  void CreateSafeBrowsingService();
  void CreateSafeBrowsingDetectionService();
  void CreateSubresourceFilterRulesetService();
  void CreateStatusTray();
  void CreateBackgroundModeManager();
  void CreateGCMDriver();
  void CreatePhysicalWebDataSource();

  void ApplyAllowCrossOriginAuthPromptPolicy();
  void ApplyDefaultBrowserPolicy();
#if !defined(OS_ANDROID)
  void ApplyMetricsReportingPolicy();
#endif

  void CacheDefaultWebClientState();

  // Methods called to control our lifetime. The browser process can be "pinned"
  // to make sure it keeps running.
  void Pin();
  void Unpin();

  std::unique_ptr<metrics_services_manager::MetricsServicesManager>
      metrics_services_manager_;

  std::unique_ptr<IOThread> io_thread_;

  bool created_watchdog_thread_;
  std::unique_ptr<WatchDogThread> watchdog_thread_;

  bool created_browser_policy_connector_;
  // Must be destroyed after |local_state_|.
  std::unique_ptr<policy::BrowserPolicyConnector> browser_policy_connector_;

  bool created_profile_manager_;
  std::unique_ptr<ProfileManager> profile_manager_;

  bool created_local_state_;
  std::unique_ptr<PrefService> local_state_;

  bool created_icon_manager_;
  std::unique_ptr<IconManager> icon_manager_;

  std::unique_ptr<GpuProfileCache> gpu_profile_cache_;

  std::unique_ptr<GpuModeManager> gpu_mode_manager_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::unique_ptr<extensions::ExtensionsBrowserClient>
      extensions_browser_client_;

  scoped_refptr<extensions::EventRouterForwarder>
      extension_event_router_forwarder_;

  std::unique_ptr<MediaFileSystemRegistry> media_file_system_registry_;
#endif

#if !defined(OS_ANDROID)
  std::unique_ptr<RemoteDebuggingServer> remote_debugging_server_;
  std::unique_ptr<DevToolsAutoOpener> devtools_auto_opener_;
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  scoped_refptr<printing::PrintPreviewDialogController>
      print_preview_dialog_controller_;

  std::unique_ptr<printing::BackgroundPrintingManager>
      background_printing_manager_;
#endif

  // Manager for desktop notification UI.
  bool created_notification_ui_manager_;
  std::unique_ptr<NotificationUIManager> notification_ui_manager_;

  std::unique_ptr<IntranetRedirectDetector> intranet_redirect_detector_;

  std::unique_ptr<StatusTray> status_tray_;

  bool created_notification_bridge_;
  std::unique_ptr<NotificationPlatformBridge> notification_bridge_;

#if BUILDFLAG(ENABLE_BACKGROUND)
  std::unique_ptr<BackgroundModeManager> background_mode_manager_;
#endif

  bool created_safe_browsing_service_;
  scoped_refptr<safe_browsing::SafeBrowsingService> safe_browsing_service_;

  bool created_subresource_filter_ruleset_service_;
  std::unique_ptr<subresource_filter::ContentRulesetService>
      subresource_filter_ruleset_service_;

  bool shutting_down_;

  bool tearing_down_;

  // Ensures that all the print jobs are finished before closing the browser.
  std::unique_ptr<printing::PrintJobManager> print_job_manager_;

  std::string locale_;

  // Download status updates (like a changing application icon on dock/taskbar)
  // are global per-application. DownloadStatusUpdater does no work in the ctor
  // so we don't have to worry about lazy initialization.
  std::unique_ptr<DownloadStatusUpdater> download_status_updater_;

  scoped_refptr<DownloadRequestLimiter> download_request_limiter_;

  // Sequenced task runner for local state related I/O tasks.
  const scoped_refptr<base::SequencedTaskRunner> local_state_task_runner_;

  // Ensures that the observers of plugin/print disable/enable state
  // notifications are properly added and removed.
  PrefChangeRegistrar pref_change_registrar_;

  // Lives here so can safely log events on shutdown.
  std::unique_ptr<net_log::ChromeNetLog> net_log_;

  std::unique_ptr<ChromeResourceDispatcherHostDelegate>
      resource_dispatcher_host_delegate_;

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
  std::unique_ptr<component_updater::ComponentUpdateService> component_updater_;
  scoped_refptr<CRLSetFetcher> crl_set_fetcher_;

#if !defined(DISABLE_NACL)
  scoped_refptr<component_updater::PnaclComponentInstaller>
      pnacl_component_installer_;
#endif

  std::unique_ptr<component_updater::SupervisedUserWhitelistInstaller>
      supervised_user_whitelist_installer_;

#if BUILDFLAG(ENABLE_PLUGINS)
  std::unique_ptr<PluginsResourceService> plugins_resource_service_;
#endif

  std::unique_ptr<BrowserProcessPlatformPart> platform_part_;

  // TODO(eroman): Remove this when done debugging 113031. This tracks
  // the callstack which released the final module reference count.
  base::debug::StackTrace release_last_reference_callstack_;

#if BUILDFLAG(ENABLE_WEBRTC)
  // Lazily initialized.
  std::unique_ptr<WebRtcLogUploader> webrtc_log_uploader_;
#endif

  std::unique_ptr<network_time::NetworkTimeTracker> network_time_tracker_;

  std::unique_ptr<gcm::GCMDriver> gcm_driver_;

  std::unique_ptr<ChromeChildProcessWatcher> child_process_watcher_;

  std::unique_ptr<ChromeDeviceClient> device_client_;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
  // Any change to this #ifdef must be reflected as well in
  // chrome/browser/resource_coordinator/tab_manager_browsertest.cc
  std::unique_ptr<resource_coordinator::TabManager> tab_manager_;
#endif

  shell_integration::DefaultWebClientState cached_default_web_client_state_;

  std::unique_ptr<physical_web::PhysicalWebDataSource>
      physical_web_data_source_;

  std::unique_ptr<prefs::InProcessPrefServiceFactory> pref_service_factory_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(BrowserProcessImpl);
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_IMPL_H_
