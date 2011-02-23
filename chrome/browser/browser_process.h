// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This interface is for managing the global services of the application. Each
// service is lazily created when requested the first time. The service getters
// will return NULL if the service is not available, so callers must check for
// this condition.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_H_
#define CHROME_BROWSER_BROWSER_PROCESS_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "ipc/ipc_message.h"

class AutomationProviderList;

namespace safe_browsing {
class ClientSideDetectionService;
}

class ChromeNetLog;
class DevToolsManager;
class DownloadRequestLimiter;
class DownloadStatusUpdater;
class GoogleURLTracker;
class IconManager;
class IntranetRedirectDetector;
class IOThread;
class MetricsService;
class NotificationUIManager;
class PrefService;
class ProfileManager;
class ResourceDispatcherHost;
class SidebarManager;
class TabCloseableStateWatcher;
class ThumbnailGenerator;
class WatchDogThread;

namespace base {
class Thread;
class WaitableEvent;
}

namespace printing {
class PrintJobManager;
class PrintPreviewTabController;
}

namespace policy {
class ConfigurationPolicyProviderKeeper;
}

namespace ui {
class Clipboard;
}

// NOT THREAD SAFE, call only from the main thread.
// These functions shouldn't return NULL unless otherwise noted.
class BrowserProcess {
 public:
  BrowserProcess();
  virtual ~BrowserProcess();

  // Invoked when the user is logging out/shutting down. When logging off we may
  // not have enough time to do a normal shutdown. This method is invoked prior
  // to normal shutdown and saves any state that must be saved before we are
  // continue shutdown.
  virtual void EndSession() = 0;

  // Services: any of these getters may return NULL
  virtual ResourceDispatcherHost* resource_dispatcher_host() = 0;

  virtual MetricsService* metrics_service() = 0;
  virtual ProfileManager* profile_manager() = 0;
  virtual PrefService* local_state() = 0;
  virtual DevToolsManager* devtools_manager() = 0;
  virtual SidebarManager* sidebar_manager() = 0;
  virtual ui::Clipboard* clipboard() = 0;

  // Returns the manager for desktop notifications.
  virtual NotificationUIManager* notification_ui_manager() = 0;

  // Returns the thread that we perform I/O coordination on (network requests,
  // communication with renderers, etc.
  // NOTE: You should ONLY use this to pass to IPC or other objects which must
  // need a MessageLoop*.  If you just want to post a task, use
  // BrowserThread::PostTask (or other variants) as they take care of checking
  // that a thread is still alive, race conditions, lifetime differences etc.
  // If you still must use this check the return value for NULL.
  virtual IOThread* io_thread() = 0;

  // Returns the thread that we perform random file operations on. For code
  // that wants to do I/O operations (not network requests or even file: URL
  // requests), this is the thread to use to avoid blocking the UI thread.
  // It might be nicer to have a thread pool for this kind of thing.
  virtual base::Thread* file_thread() = 0;

  // Returns the thread that is used for database operations such as the web
  // database. History has its own thread since it has much higher traffic.
  virtual base::Thread* db_thread() = 0;

  // Returns the thread that is used for background cache operations.
  virtual base::Thread* cache_thread() = 0;

#if defined(USE_X11)
  // Returns the thread that is used to process UI requests in cases where
  // we can't route the request to the UI thread. Note that this thread
  // should only be used by the IO thread and this method is only safe to call
  // from the UI thread so, if you've ended up here, something has gone wrong.
  // This method is only included for uniformity.
  virtual base::Thread* background_x11_thread() = 0;
#endif

  // Returns the thread that is used for health check of all browser threads.
  virtual WatchDogThread* watchdog_thread() = 0;

  virtual policy::ConfigurationPolicyProviderKeeper*
      configuration_policy_provider_keeper() = 0;

  virtual IconManager* icon_manager() = 0;

  virtual ThumbnailGenerator* GetThumbnailGenerator() = 0;

  virtual AutomationProviderList* InitAutomationProviderList() = 0;

  virtual void InitDevToolsHttpProtocolHandler(
      const std::string& ip,
      int port,
      const std::string& frontend_url) = 0;

  virtual void InitDevToolsLegacyProtocolHandler(int port) = 0;

  virtual unsigned int AddRefModule() = 0;
  virtual unsigned int ReleaseModule() = 0;

  virtual bool IsShuttingDown() = 0;

  virtual printing::PrintJobManager* print_job_manager() = 0;
  virtual printing::PrintPreviewTabController*
      print_preview_tab_controller() = 0;

  virtual GoogleURLTracker* google_url_tracker() = 0;
  virtual IntranetRedirectDetector* intranet_redirect_detector() = 0;

  // Returns the locale used by the application.
  virtual const std::string& GetApplicationLocale() = 0;
  virtual void SetApplicationLocale(const std::string& locale) = 0;

  DownloadRequestLimiter* download_request_limiter();
  virtual DownloadStatusUpdater* download_status_updater() = 0;

  // Returns an event that is signaled when the browser shutdown.
  virtual base::WaitableEvent* shutdown_event() = 0;

  // Returns a reference to the user-data-dir based profiles vector.
  std::vector<std::wstring>& user_data_dir_profiles() {
    return user_data_dir_profiles_;
  }

  // Returns the object that watches for changes in the closeable state of tab.
  virtual TabCloseableStateWatcher* tab_closeable_state_watcher() = 0;

  // Returns an object which handles communication with the SafeBrowsing
  // client-side detection servers.
  virtual safe_browsing::ClientSideDetectionService*
      safe_browsing_detection_service() = 0;

  // Trigger an asynchronous check to see if we have the inspector's files on
  // disk.
  virtual void CheckForInspectorFiles() = 0;

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
  // This will start a timer that, if Chrome is in persistent mode, will check
  // whether an update is available, and if that's the case, restart the
  // browser. Note that restart code will strip some of the command line keys
  // and all loose values from the cl this instance of Chrome was launched with,
  // and add the command line key that will force Chrome to start in the
  // background mode. For the full list of "blacklisted" keys, refer to
  // |kSwitchesToRemoveOnAutorestart| array in browser_process_impl.cc.
  virtual void StartAutoupdateTimer() = 0;
#endif

  // Return true iff we found the inspector files on disk. It's possible to
  // call this function before we have a definite answer from the disk. In that
  // case, we default to returning true.
  virtual bool have_inspector_files() const = 0;

  virtual ChromeNetLog* net_log() = 0;

#if defined(IPC_MESSAGE_LOG_ENABLED)
  // Enable or disable IPC logging for the browser, all processes
  // derived from ChildProcess (plugin etc), and all
  // renderers.
  virtual void SetIPCLoggingEnabled(bool enable) = 0;
#endif

  const std::string& plugin_data_remover_mime_type() const {
    return plugin_data_remover_mime_type_;
  }

  void set_plugin_data_remover_mime_type(const std::string& mime_type) {
    plugin_data_remover_mime_type_ = mime_type;
  }

 private:
  // User-data-dir based profiles.
  std::vector<std::wstring> user_data_dir_profiles_;

  // Used for testing plugin data removal at shutdown.
  std::string plugin_data_remover_mime_type_;

  DISALLOW_COPY_AND_ASSIGN(BrowserProcess);
};

extern BrowserProcess* g_browser_process;

#endif  // CHROME_BROWSER_BROWSER_PROCESS_H_
