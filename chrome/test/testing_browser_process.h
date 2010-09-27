// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

#include "app/clipboard/clipboard.h"
#include "base/string_util.h"
#include "base/waitable_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/notification_service.h"

class IOThread;

class TestingBrowserProcess : public BrowserProcess {
 public:
  TestingBrowserProcess()
      : shutdown_event_(new base::WaitableEvent(true, false)),
        module_ref_count_(0),
        app_locale_("en"),
        pref_service_(NULL) {
  }

  virtual ~TestingBrowserProcess() {
  }

  virtual void EndSession() {
  }

  virtual ResourceDispatcherHost* resource_dispatcher_host() {
    return NULL;
  }

  virtual MetricsService* metrics_service() {
    return NULL;
  }

  virtual IOThread* io_thread() {
    return NULL;
  }

#if defined(OS_LINUX)
  virtual base::Thread* background_x11_thread() {
    return NULL;
  }
#endif

  virtual base::Thread* file_thread() {
    return NULL;
  }

  virtual base::Thread* db_thread() {
    return NULL;
  }

  virtual base::Thread* cache_thread() {
    return NULL;
  }

  virtual ProfileManager* profile_manager() {
    return NULL;
  }

  virtual PrefService* local_state() {
    return pref_service_;
  }

  virtual IconManager* icon_manager() {
    return NULL;
  }

  virtual ThumbnailGenerator* GetThumbnailGenerator() {
    return NULL;
  }

  virtual DevToolsManager* devtools_manager() {
    return NULL;
  }

  virtual SidebarManager* sidebar_manager() {
    return NULL;
  }

  virtual TabCloseableStateWatcher* tab_closeable_state_watcher() {
    return NULL;
  }

  virtual Clipboard* clipboard() {
    if (!clipboard_.get()) {
      // Note that we need a MessageLoop for the next call to work.
      clipboard_.reset(new Clipboard);
    }
    return clipboard_.get();
  }

  virtual NotificationUIManager* notification_ui_manager() {
    return NULL;
  }

  virtual GoogleURLTracker* google_url_tracker() {
    return google_url_tracker_.get();
  }

  virtual IntranetRedirectDetector* intranet_redirect_detector() {
    return NULL;
  }

  virtual AutomationProviderList* InitAutomationProviderList() {
    return NULL;
  }

  virtual void InitDebuggerWrapper(int port, bool useHttp) {
  }

  virtual unsigned int AddRefModule() {
    return ++module_ref_count_;
  }
  virtual unsigned int ReleaseModule() {
    DCHECK(module_ref_count_ > 0);
    return --module_ref_count_;
  }

  virtual bool IsShuttingDown() {
    return false;
  }

  virtual printing::PrintJobManager* print_job_manager() {
    return NULL;
  }

  virtual const std::string& GetApplicationLocale() {
    return app_locale_;
  }

  virtual void SetApplicationLocale(const std::string& app_locale) {
    app_locale_ = app_locale;
  }

  virtual DownloadStatusUpdater* download_status_updater() {
    return NULL;
  }

  virtual base::WaitableEvent* shutdown_event() {
    return shutdown_event_.get();
  }

  virtual void CheckForInspectorFiles() {}

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
  virtual void StartAutoupdateTimer() {}
#endif

  virtual bool have_inspector_files() const { return true; }
#if defined(IPC_MESSAGE_LOG_ENABLED)
  virtual void SetIPCLoggingEnabled(bool enable) {}
#endif

  void SetPrefService(PrefService* pref_service) {
    pref_service_ = pref_service;
  }
  void SetGoogleURLTracker(GoogleURLTracker* google_url_tracker) {
    google_url_tracker_.reset(google_url_tracker);
  }

 private:
  NotificationService notification_service_;
  scoped_ptr<base::WaitableEvent> shutdown_event_;
  unsigned int module_ref_count_;
  scoped_ptr<Clipboard> clipboard_;
  std::string app_locale_;

  PrefService* pref_service_;
  scoped_ptr<GoogleURLTracker> google_url_tracker_;

  DISALLOW_COPY_AND_ASSIGN(TestingBrowserProcess);
};

#endif  // CHROME_TEST_TESTING_BROWSER_PROCESS_H_
