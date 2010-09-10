// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// When each service is created, we set a flag indicating this. At this point,
// the service initialization could fail or succeed. This allows us to remember
// if we tried to create a service, and not try creating it over and over if
// the creation failed.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_IMPL_H_
#define CHROME_BROWSER_BROWSER_PROCESS_IMPL_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/non_thread_safe.h"
#include "base/timer.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "ipc/ipc_message.h"

class CommandLine;
class DebuggerWrapper;
class FilePath;
class NotificationService;
class TabCloseableStateWatcher;

// Real implementation of BrowserProcess that creates and returns the services.
class BrowserProcessImpl : public BrowserProcess, public NonThreadSafe {
 public:
  explicit BrowserProcessImpl(const CommandLine& command_line);
  virtual ~BrowserProcessImpl();

  virtual void EndSession();

  virtual ResourceDispatcherHost* resource_dispatcher_host();
  virtual MetricsService* metrics_service();
  virtual IOThread* io_thread();
  virtual base::Thread* file_thread();
  virtual base::Thread* db_thread();
  virtual base::Thread* process_launcher_thread();
  virtual base::Thread* cache_thread();
#if defined(USE_X11)
  virtual base::Thread* background_x11_thread();
#endif
  virtual ProfileManager* profile_manager();
  virtual PrefService* local_state();
  virtual DevToolsManager* devtools_manager();
  virtual SidebarManager* sidebar_manager();
  virtual Clipboard* clipboard();
  virtual NotificationUIManager* notification_ui_manager();
  virtual IconManager* icon_manager();
  virtual ThumbnailGenerator* GetThumbnailGenerator();
  virtual AutomationProviderList* InitAutomationProviderList();
  virtual void InitDebuggerWrapper(int port, bool useHttp);
  virtual unsigned int AddRefModule();
  virtual unsigned int ReleaseModule();
  virtual bool IsShuttingDown();
  virtual printing::PrintJobManager* print_job_manager();
  virtual GoogleURLTracker* google_url_tracker();
  virtual IntranetRedirectDetector* intranet_redirect_detector();
  virtual const std::string& GetApplicationLocale();
  virtual void SetApplicationLocale(const std::string& locale);
  virtual base::WaitableEvent* shutdown_event();
  virtual TabCloseableStateWatcher* tab_closeable_state_watcher();
  virtual void CheckForInspectorFiles();

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
  void StartAutoupdateTimer();
#endif

  virtual bool have_inspector_files() const;

#if defined(IPC_MESSAGE_LOG_ENABLED)
  virtual void SetIPCLoggingEnabled(bool enable);
#endif

 private:
  void ClearLocalState(const FilePath& profile_path);
  bool ShouldClearLocalState(FilePath* profile_path);

  void CreateResourceDispatcherHost();
  void CreatePrefService();
  void CreateMetricsService();

  void CreateIOThread();
  static void CleanupOnIOThread();

  void CreateFileThread();
  void CreateDBThread();
  void CreateProcessLauncherThread();
  void CreateCacheThread();
  void CreateTemplateURLModel();
  void CreateProfileManager();
  void CreateWebDataService();
  void CreateLocalState();
  void CreateViewedPageTracker();
  void CreateIconManager();
  void CreateDebuggerWrapper(int port, bool useHttp);
  void CreateDevToolsManager();
  void CreateSidebarManager();
  void CreateGoogleURLTracker();
  void CreateIntranetRedirectDetector();
  void CreateNotificationUIManager();
  void CreateStatusTrayManager();
  void CreateTabCloseableStateWatcher();

#if defined(IPC_MESSAGE_LOG_ENABLED)
  void SetIPCLoggingEnabledForChildProcesses(bool enabled);
#endif

  bool created_resource_dispatcher_host_;
  scoped_ptr<ResourceDispatcherHost> resource_dispatcher_host_;

  bool created_metrics_service_;
  scoped_ptr<MetricsService> metrics_service_;

  bool created_io_thread_;
  scoped_ptr<IOThread> io_thread_;
#if defined(USE_X11)
  // This shares a created flag with the IO thread.
  scoped_ptr<base::Thread> background_x11_thread_;
#endif

  bool created_file_thread_;
  scoped_ptr<base::Thread> file_thread_;

  bool created_db_thread_;
  scoped_ptr<base::Thread> db_thread_;

  bool created_process_launcher_thread_;
  scoped_ptr<base::Thread> process_launcher_thread_;

  bool created_cache_thread_;
  scoped_ptr<base::Thread> cache_thread_;

  bool created_profile_manager_;
  scoped_ptr<ProfileManager> profile_manager_;

  bool created_local_state_;
  scoped_ptr<PrefService> local_state_;

  bool created_icon_manager_;
  scoped_ptr<IconManager> icon_manager_;

  bool created_debugger_wrapper_;
  scoped_refptr<DebuggerWrapper> debugger_wrapper_;

  bool created_devtools_manager_;
  scoped_refptr<DevToolsManager> devtools_manager_;

  bool created_sidebar_manager_;
  scoped_refptr<SidebarManager> sidebar_manager_;

  scoped_ptr<Clipboard> clipboard_;

  // Manager for desktop notification UI.
  bool created_notification_ui_manager_;
  scoped_ptr<NotificationUIManager> notification_ui_manager_;

  scoped_ptr<AutomationProviderList> automation_provider_list_;

  scoped_ptr<GoogleURLTracker> google_url_tracker_;
  scoped_ptr<IntranetRedirectDetector> intranet_redirect_detector_;

  scoped_ptr<NotificationService> main_notification_service_;

  scoped_ptr<TabCloseableStateWatcher> tab_closeable_state_watcher_;

  unsigned int module_ref_count_;
  bool did_start_;

  // Ensures that all the print jobs are finished before closing the browser.
  scoped_ptr<printing::PrintJobManager> print_job_manager_;

  std::string locale_;

  bool checked_for_new_frames_;
  bool using_new_frames_;

  // This service just sits around and makes thumanails for tabs. It does
  // nothing in the cosntructor so we don't have to worry about lazy init.
  ThumbnailGenerator thumbnail_generator_;

  // An event that notifies when we are shutting-down.
  scoped_ptr<base::WaitableEvent> shutdown_event_;

  // Runs on the file thread and stats the inspector's directory, filling in
  // have_inspector_files_ with the result.
  void DoInspectorFilesCheck();
  // Our best estimate about the existence of the inspector directory.
  bool have_inspector_files_;

  // Ensures that the observer of plugin disable/enable state
  // notifications are properly added and removed.
  PrefChangeRegistrar plugin_state_change_registrar_;

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
  base::RepeatingTimer<BrowserProcessImpl> autoupdate_timer_;

  // Gets called by autoupdate timer to see if browser needs restart and can be
  // restarted, and if that's the case, restarts the browser.
  void OnAutoupdateTimer();
  bool CanAutorestartForUpdate() const;
  void RestartPersistentInstance();
#endif  // defined(OS_WIN) || defined(OS_LINUX)

  DISALLOW_COPY_AND_ASSIGN(BrowserProcessImpl);
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_IMPL_H_
