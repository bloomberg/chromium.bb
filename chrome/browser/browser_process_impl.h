// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// When each service is created, we set a flag indicating this. At this point,
// the service initialization could fail or succeed. This allows us to remember
// if we tried to create a service, and not try creating it over and over if
// the creation failed.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_IMPL_H_
#define CHROME_BROWSER_BROWSER_PROCESS_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/debug/stack_trace.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "base/threading/non_thread_safe.h"
#include "base/timer.h"
#include "chrome/browser/browser_process.h"

class ChromeNetLog;
class ChromeResourceDispatcherHostDelegate;
class CommandLine;
class RemoteDebuggingServer;
class PrefRegistrySimple;
class PromoResourceService;

#if defined(ENABLE_PLUGIN_INSTALLATION)
class PluginsResourceService;
#endif

namespace base {
class SequencedTaskRunner;
}

namespace policy {
class BrowserPolicyConnector;
class PolicyService;
};

#if defined(OS_WIN) && defined(USE_AURA)
class MetroViewerProcessHost;
#endif

// Real implementation of BrowserProcess that creates and returns the services.
class BrowserProcessImpl : public BrowserProcess,
                           public base::NonThreadSafe {
 public:
  // |local_state_task_runner| must be a shutdown-blocking task runner.
  BrowserProcessImpl(base::SequencedTaskRunner* local_state_task_runner,
                     const CommandLine& command_line);
  virtual ~BrowserProcessImpl();

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
  void StartTearDown();
  void PostDestroyThreads();

  // BrowserProcess implementation.
  virtual void ResourceDispatcherHostCreated() OVERRIDE;
  virtual void EndSession() OVERRIDE;
  virtual MetricsService* metrics_service() OVERRIDE;
  virtual IOThread* io_thread() OVERRIDE;
  virtual WatchDogThread* watchdog_thread() OVERRIDE;
  virtual ProfileManager* profile_manager() OVERRIDE;
  virtual PrefService* local_state() OVERRIDE;
  virtual net::URLRequestContextGetter* system_request_context() OVERRIDE;
  virtual chrome_variations::VariationsService* variations_service() OVERRIDE;
#if defined(OS_CHROMEOS)
  virtual chromeos::OomPriorityManager* oom_priority_manager() OVERRIDE;
#endif  // defined(OS_CHROMEOS)
  virtual extensions::EventRouterForwarder*
        extension_event_router_forwarder() OVERRIDE;
  virtual NotificationUIManager* notification_ui_manager() OVERRIDE;
#if defined(ENABLE_MESSAGE_CENTER)
  virtual message_center::MessageCenter* message_center() OVERRIDE;
#endif
  virtual policy::BrowserPolicyConnector* browser_policy_connector() OVERRIDE;
  virtual policy::PolicyService* policy_service() OVERRIDE;
  virtual IconManager* icon_manager() OVERRIDE;
  virtual GLStringManager* gl_string_manager() OVERRIDE;
  virtual RenderWidgetSnapshotTaker* GetRenderWidgetSnapshotTaker() OVERRIDE;
  virtual AutomationProviderList* GetAutomationProviderList() OVERRIDE;
  virtual void CreateDevToolsHttpProtocolHandler(
      Profile* profile,
      chrome::HostDesktopType host_desktop_type,
      const std::string& ip,
      int port,
      const std::string& frontend_url) OVERRIDE;
  virtual unsigned int AddRefModule() OVERRIDE;
  virtual unsigned int ReleaseModule() OVERRIDE;
  virtual bool IsShuttingDown() OVERRIDE;
  virtual printing::PrintJobManager* print_job_manager() OVERRIDE;
  virtual printing::PrintPreviewDialogController*
      print_preview_dialog_controller() OVERRIDE;
  virtual printing::BackgroundPrintingManager*
      background_printing_manager() OVERRIDE;
  virtual IntranetRedirectDetector* intranet_redirect_detector() OVERRIDE;
  virtual const std::string& GetApplicationLocale() OVERRIDE;
  virtual void SetApplicationLocale(const std::string& locale) OVERRIDE;
  virtual DownloadStatusUpdater* download_status_updater() OVERRIDE;
  virtual DownloadRequestLimiter* download_request_limiter() OVERRIDE;
  virtual BackgroundModeManager* background_mode_manager() OVERRIDE;
  virtual StatusTray* status_tray() OVERRIDE;
  virtual SafeBrowsingService* safe_browsing_service() OVERRIDE;
  virtual safe_browsing::ClientSideDetectionService*
      safe_browsing_detection_service() OVERRIDE;

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
  virtual void StartAutoupdateTimer() OVERRIDE;
#endif

  virtual ChromeNetLog* net_log() OVERRIDE;
  virtual prerender::PrerenderTracker* prerender_tracker() OVERRIDE;
  virtual ComponentUpdateService* component_updater() OVERRIDE;
  virtual CRLSetFetcher* crl_set_fetcher() OVERRIDE;
  virtual BookmarkPromptController* bookmark_prompt_controller() OVERRIDE;
  virtual chrome::MediaFileSystemRegistry*
      media_file_system_registry() OVERRIDE;
  virtual void PlatformSpecificCommandLineProcessing(
      const CommandLine& command_line) OVERRIDE;

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  void CreateMetricsService();
  void CreateWatchdogThread();
#if defined(OS_CHROMEOS)
  void InitializeWebSocketProxyThread();
#endif
  void CreateProfileManager();
  void CreateLocalState();
  void CreateViewedPageTracker();
  void CreateIconManager();
  void CreateIntranetRedirectDetector();
  void CreateNotificationUIManager();
#if defined(ENABLE_MESSAGE_CENTER) && !defined(USE_ASH)
  void CreateMessageCenter();
#endif
  void CreateStatusTrayManager();
  void CreatePrintPreviewDialogController();
  void CreateBackgroundPrintingManager();
  void CreateSafeBrowsingService();
  void CreateSafeBrowsingDetectionService();
  void CreateStatusTray();
  void CreateBackgroundModeManager();

  void ApplyDisabledSchemesPolicy();
  void ApplyAllowCrossOriginAuthPromptPolicy();
  void ApplyDefaultBrowserPolicy();

  bool created_metrics_service_;
  scoped_ptr<MetricsService> metrics_service_;

  scoped_ptr<IOThread> io_thread_;

  bool created_watchdog_thread_;
  scoped_ptr<WatchDogThread> watchdog_thread_;

  bool created_browser_policy_connector_;
#if defined(ENABLE_CONFIGURATION_POLICY)
  // Must be destroyed after |local_state_|.
  scoped_ptr<policy::BrowserPolicyConnector> browser_policy_connector_;
#endif

  // Must be destroyed after |local_state_|.
  // This is a stub when policy is not enabled. Otherwise, the PolicyService
  // is owned by the |browser_policy_connector_| and this is not used.
  scoped_ptr<policy::PolicyService> policy_service_;

  bool created_profile_manager_;
  scoped_ptr<ProfileManager> profile_manager_;

  bool created_local_state_;
  scoped_ptr<PrefService> local_state_;

  bool created_icon_manager_;
  scoped_ptr<IconManager> icon_manager_;

  scoped_ptr<GLStringManager> gl_string_manager_;

  scoped_refptr<extensions::EventRouterForwarder>
      extension_event_router_forwarder_;

#if !defined(OS_ANDROID)
  scoped_ptr<RemoteDebuggingServer> remote_debugging_server_;

  // Bookmark prompt controller displays the prompt for frequently visited URL.
  scoped_ptr<BookmarkPromptController> bookmark_prompt_controller_;
#endif

  scoped_ptr<chrome::MediaFileSystemRegistry> media_file_system_registry_;

  scoped_refptr<printing::PrintPreviewDialogController>
      print_preview_dialog_controller_;

  scoped_ptr<printing::BackgroundPrintingManager> background_printing_manager_;

  scoped_ptr<chrome_variations::VariationsService> variations_service_;

  // Manager for desktop notification UI.
  bool created_notification_ui_manager_;
  scoped_ptr<NotificationUIManager> notification_ui_manager_;

#if defined(ENABLE_MESSAGE_CENTER) && !defined(USE_ASH)
  // MessageCenter keeps currently displayed UI notifications.
  scoped_ptr<message_center::MessageCenter> message_center_;
  bool created_message_center_;
#endif

#if defined(ENABLE_AUTOMATION)
  scoped_ptr<AutomationProviderList> automation_provider_list_;
#endif

  scoped_ptr<IntranetRedirectDetector> intranet_redirect_detector_;

  scoped_ptr<StatusTray> status_tray_;

  scoped_ptr<BackgroundModeManager> background_mode_manager_;

  bool created_safe_browsing_service_;
  scoped_refptr<SafeBrowsingService> safe_browsing_service_;

  unsigned int module_ref_count_;
  bool did_start_;

  // Ensures that all the print jobs are finished before closing the browser.
  scoped_ptr<printing::PrintJobManager> print_job_manager_;

  std::string locale_;

  bool checked_for_new_frames_;
  bool using_new_frames_;

  // This service just sits around and makes snapshots for renderers. It does
  // nothing in the constructor so we don't have to worry about lazy init.
  scoped_ptr<RenderWidgetSnapshotTaker> render_widget_snapshot_taker_;

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
  scoped_ptr<ChromeNetLog> net_log_;

  // Ordered before resource_dispatcher_host_delegate_ due to destruction
  // ordering.
  scoped_ptr<prerender::PrerenderTracker> prerender_tracker_;

  scoped_ptr<ChromeResourceDispatcherHostDelegate>
      resource_dispatcher_host_delegate_;

  scoped_refptr<PromoResourceService> promo_resource_service_;

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
  base::RepeatingTimer<BrowserProcessImpl> autoupdate_timer_;

  // Gets called by autoupdate timer to see if browser needs restart and can be
  // restarted, and if that's the case, restarts the browser.
  void OnAutoupdateTimer();
  bool CanAutorestartForUpdate() const;
  void RestartBackgroundInstance();
#endif  // defined(OS_WIN) || defined(OS_LINUX) && !defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
  scoped_ptr<chromeos::OomPriorityManager> oom_priority_manager_;
#else
  scoped_ptr<ComponentUpdateService> component_updater_;

  scoped_refptr<CRLSetFetcher> crl_set_fetcher_;
#endif

#if defined(ENABLE_PLUGIN_INSTALLATION)
  scoped_refptr<PluginsResourceService> plugins_resource_service_;
#endif

#if defined(OS_WIN) && defined(USE_AURA)
  void PerformInitForWindowsAura(const CommandLine& command_line);

  // Hosts the channel for the Windows 8 metro viewer process which runs in
  // the ASH environment.
  scoped_ptr<MetroViewerProcessHost> metro_viewer_process_host_;
#endif

  // TODO(eroman): Remove this when done debugging 113031. This tracks
  // the callstack which released the final module reference count.
  base::debug::StackTrace release_last_reference_callstack_;

  DISALLOW_COPY_AND_ASSIGN(BrowserProcessImpl);
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_IMPL_H_
