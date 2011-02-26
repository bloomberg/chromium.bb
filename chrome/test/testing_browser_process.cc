// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/testing_browser_process.h"

#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/dummy_configuration_policy_provider.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "ui/base/clipboard/clipboard.h"

TestingBrowserProcess::TestingBrowserProcess()
    : shutdown_event_(new base::WaitableEvent(true, false)),
      module_ref_count_(0),
      app_locale_("en"),
      pref_service_(NULL) {
}

TestingBrowserProcess::~TestingBrowserProcess() {
}

void TestingBrowserProcess::EndSession() {
}

ResourceDispatcherHost* TestingBrowserProcess::resource_dispatcher_host() {
  return NULL;
}

MetricsService* TestingBrowserProcess::metrics_service() {
  return NULL;
}

IOThread* TestingBrowserProcess::io_thread() {
  return NULL;
}

#if defined(OS_LINUX)
base::Thread* TestingBrowserProcess::background_x11_thread() {
  return NULL;
}
#endif

base::Thread* TestingBrowserProcess::file_thread() {
  return NULL;
}

base::Thread* TestingBrowserProcess::db_thread() {
  return NULL;
}

base::Thread* TestingBrowserProcess::cache_thread() {
  return NULL;
}

ProfileManager* TestingBrowserProcess::profile_manager() {
  return profile_manager_.get();
}

void TestingBrowserProcess::SetProfileManager(ProfileManager* profile_manager) {
  profile_manager_.reset(profile_manager);
}

PrefService* TestingBrowserProcess::local_state() {
  return pref_service_;
}

policy::BrowserPolicyConnector*
    TestingBrowserProcess::browser_policy_connector() {
  if (!browser_policy_connector_.get()) {
    const policy::ConfigurationPolicyProvider::PolicyDefinitionList*
        policy_list = policy::ConfigurationPolicyPrefStore::
        GetChromePolicyDefinitionList();
    browser_policy_connector_.reset(
        new policy::BrowserPolicyConnector(
            new policy::DummyConfigurationPolicyProvider(policy_list),
            new policy::DummyConfigurationPolicyProvider(policy_list)));
  }
  return browser_policy_connector_.get();
}

IconManager* TestingBrowserProcess::icon_manager() {
  return NULL;
}

ThumbnailGenerator* TestingBrowserProcess::GetThumbnailGenerator() {
  return NULL;
}

DevToolsManager* TestingBrowserProcess::devtools_manager() {
  return NULL;
}

SidebarManager* TestingBrowserProcess::sidebar_manager() {
  return NULL;
}

TabCloseableStateWatcher* TestingBrowserProcess::tab_closeable_state_watcher() {
  return NULL;
}

safe_browsing::ClientSideDetectionService*
TestingBrowserProcess::safe_browsing_detection_service() {
  return NULL;
}

ui::Clipboard* TestingBrowserProcess::clipboard() {
  if (!clipboard_.get()) {
    // Note that we need a MessageLoop for the next call to work.
    clipboard_.reset(new ui::Clipboard);
  }
  return clipboard_.get();
}

NotificationUIManager* TestingBrowserProcess::notification_ui_manager() {
  return NULL;
}

GoogleURLTracker* TestingBrowserProcess::google_url_tracker() {
  return google_url_tracker_.get();
}

IntranetRedirectDetector* TestingBrowserProcess::intranet_redirect_detector() {
  return NULL;
}

AutomationProviderList* TestingBrowserProcess::InitAutomationProviderList() {
  return NULL;
}

void TestingBrowserProcess::InitDevToolsHttpProtocolHandler(
    const std::string& ip,
    int port,
    const std::string& frontend_url) {
}

void TestingBrowserProcess::InitDevToolsLegacyProtocolHandler(int port) {
}

unsigned int TestingBrowserProcess::AddRefModule() {
  return ++module_ref_count_;
}

unsigned int TestingBrowserProcess::ReleaseModule() {
  DCHECK(module_ref_count_ > 0);
  return --module_ref_count_;
}

bool TestingBrowserProcess::IsShuttingDown() {
  return false;
}

printing::PrintJobManager* TestingBrowserProcess::print_job_manager() {
  return NULL;
}

printing::PrintPreviewTabController*
TestingBrowserProcess::print_preview_tab_controller() {
  return NULL;
}

const std::string& TestingBrowserProcess::GetApplicationLocale() {
  return app_locale_;
}

void TestingBrowserProcess::SetApplicationLocale(
    const std::string& app_locale) {
  app_locale_ = app_locale;
}

DownloadStatusUpdater* TestingBrowserProcess::download_status_updater() {
  return NULL;
}

base::WaitableEvent* TestingBrowserProcess::shutdown_event() {
  return shutdown_event_.get();
}

bool TestingBrowserProcess::plugin_finder_disabled() const {
  return false;
}

bool TestingBrowserProcess::have_inspector_files() const {
  return true;
}

ChromeNetLog* TestingBrowserProcess::net_log() {
  return NULL;
}

void TestingBrowserProcess::SetPrefService(PrefService* pref_service) {
  pref_service_ = pref_service;
}

void TestingBrowserProcess::SetGoogleURLTracker(
    GoogleURLTracker* google_url_tracker) {
  google_url_tracker_.reset(google_url_tracker);
}

ScopedTestingBrowserProcess::ScopedTestingBrowserProcess() {
  // TODO(phajdan.jr): Temporary, for http://crbug.com/61062.
  // ChromeTestSuite sets up a global TestingBrowserProcess
  // for all tests. We need to get rid of it, because it contains
  // a NotificationService, and there can be only one NotificationService
  // per thread.
  DCHECK(g_browser_process);
  delete g_browser_process;

  browser_process_.reset(new TestingBrowserProcess);
  g_browser_process = browser_process_.get();
}

ScopedTestingBrowserProcess::~ScopedTestingBrowserProcess() {
  DCHECK_EQ(browser_process_.get(), g_browser_process);

  // TODO(phajdan.jr): Temporary, for http://crbug.com/61062.
  // After the transition is over, we should just
  // reset |g_browser_process| to NULL.
  browser_process_.reset();
  g_browser_process = new TestingBrowserProcess();
}
