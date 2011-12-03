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
#include "base/memory/ref_counted.h"
#include "ipc/ipc_message.h"

class AutomationProviderList;
class BackgroundModeManager;
class ChromeNetLog;
class CRLSetFetcher;
class ComponentUpdateService;
class DownloadRequestLimiter;
class DownloadStatusUpdater;
class ExtensionEventRouterForwarder;
class GoogleURLTracker;
class IconManager;
class IntranetRedirectDetector;
class IOThread;
class MetricsService;
class MHTMLGenerationManager;
class NotificationUIManager;
class PrefService;
class Profile;
class ProfileManager;
class ResourceDispatcherHost;
class SafeBrowsingService;
class SidebarManager;
class StatusTray;
class TabCloseableStateWatcher;
class ThumbnailGenerator;
class WatchDogThread;

namespace base {
class Thread;
}

#if defined(OS_CHROMEOS)
namespace browser {
class OomPriorityManager;
}
#endif  // defined(OS_CHROMEOS)

namespace net {
class URLRequestContextGetter;
}

namespace prerender {
class PrerenderTracker;
}

namespace printing {
class BackgroundPrintingManager;
class PrintJobManager;
class PrintPreviewTabController;
}

namespace policy {
class BrowserPolicyConnector;
}

namespace safe_browsing {
class ClientSideDetectionService;
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
  virtual SidebarManager* sidebar_manager() = 0;
  virtual ui::Clipboard* clipboard() = 0;
  virtual net::URLRequestContextGetter* system_request_context() = 0;

#if defined(OS_CHROMEOS)
  // Returns the out-of-memory priority manager.
  virtual browser::OomPriorityManager* oom_priority_manager() = 0;
#endif  // defined(OS_CHROMEOS)

  virtual ExtensionEventRouterForwarder*
      extension_event_router_forwarder() = 0;

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

  // Returns the thread that is used for health check of all browser threads.
  virtual WatchDogThread* watchdog_thread() = 0;

#if defined(OS_CHROMEOS)
  // Returns thread for websocket to TCP proxy.
  // TODO(dilmah): remove this thread.  Instead provide this functionality via
  // hooks into websocket bridge layer.
  virtual base::Thread* web_socket_proxy_thread() = 0;
#endif

  virtual policy::BrowserPolicyConnector* browser_policy_connector() = 0;

  virtual IconManager* icon_manager() = 0;

  virtual ThumbnailGenerator* GetThumbnailGenerator() = 0;

  virtual AutomationProviderList* GetAutomationProviderList() = 0;

  virtual void InitDevToolsHttpProtocolHandler(
      Profile* profile,
      const std::string& ip,
      int port,
      const std::string& frontend_url) = 0;

  virtual unsigned int AddRefModule() = 0;
  virtual unsigned int ReleaseModule() = 0;

  virtual bool IsShuttingDown() = 0;

  virtual printing::PrintJobManager* print_job_manager() = 0;
  virtual printing::PrintPreviewTabController*
      print_preview_tab_controller() = 0;
  virtual printing::BackgroundPrintingManager*
      background_printing_manager() = 0;

  virtual GoogleURLTracker* google_url_tracker() = 0;
  virtual IntranetRedirectDetector* intranet_redirect_detector() = 0;

  // Returns the locale used by the application.
  virtual const std::string& GetApplicationLocale() = 0;
  virtual void SetApplicationLocale(const std::string& locale) = 0;

  virtual DownloadStatusUpdater* download_status_updater() = 0;
  virtual DownloadRequestLimiter* download_request_limiter() = 0;

  // Returns the object that watches for changes in the closeable state of tab.
  virtual TabCloseableStateWatcher* tab_closeable_state_watcher() = 0;

  // Returns the object that manages background applications.
  virtual BackgroundModeManager* background_mode_manager() = 0;

  // Returns the StatusTray, which provides an API for displaying status icons
  // in the system status tray. Returns NULL if status icons are not supported
  // on this platform (or this is a unit test).
  virtual StatusTray* status_tray() = 0;

  // Returns the SafeBrowsing service.
  virtual SafeBrowsingService* safe_browsing_service() = 0;

  // Returns an object which handles communication with the SafeBrowsing
  // client-side detection servers.
  virtual safe_browsing::ClientSideDetectionService*
      safe_browsing_detection_service() = 0;

  // Returns the state of the disable plugin finder policy. Callable only on
  // the IO thread.
  virtual bool plugin_finder_disabled() const = 0;

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

  virtual ChromeNetLog* net_log() = 0;

  virtual prerender::PrerenderTracker* prerender_tracker() = 0;

  virtual MHTMLGenerationManager* mhtml_generation_manager() = 0;

  virtual ComponentUpdateService* component_updater() = 0;

  virtual CRLSetFetcher* crl_set_fetcher() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserProcess);
};

extern BrowserProcess* g_browser_process;

#endif  // CHROME_BROWSER_BROWSER_PROCESS_H_
