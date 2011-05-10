// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/testing_browser_process.h"

#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/dummy_configuration_policy_provider.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/clipboard/clipboard.h"
#include "testing/gtest/include/gtest/gtest.h"

TestingBrowserProcess::TestingBrowserProcess()
    : shutdown_event_(new base::WaitableEvent(true, false)),
      module_ref_count_(0),
      app_locale_("en"),
      local_state_(NULL) {
}

TestingBrowserProcess::~TestingBrowserProcess() {
  EXPECT_FALSE(local_state_);
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

base::Thread* TestingBrowserProcess::gpu_thread() {
  return NULL;
}

WatchDogThread* TestingBrowserProcess::watchdog_thread() {
  return NULL;
}

#if defined(OS_CHROMEOS)
base::Thread* TestingBrowserProcess::web_socket_proxy_thread() {
  return NULL;
}
#endif

ProfileManager* TestingBrowserProcess::profile_manager() {
  return profile_manager_.get();
}

void TestingBrowserProcess::SetProfileManager(ProfileManager* profile_manager) {
  profile_manager_.reset(profile_manager);
}

PrefService* TestingBrowserProcess::local_state() {
  return local_state_;
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

net::URLRequestContextGetter* TestingBrowserProcess::system_request_context() {
  return NULL;
}

#if defined(OS_CHROMEOS)
chromeos::ProxyConfigServiceImpl*
TestingBrowserProcess::chromeos_proxy_config_service_impl() {
  return NULL;
}
#endif  // defined(OS_CHROMEOS)

ui::Clipboard* TestingBrowserProcess::clipboard() {
  if (!clipboard_.get()) {
    // Note that we need a MessageLoop for the next call to work.
    clipboard_.reset(new ui::Clipboard);
  }
  return clipboard_.get();
}

ExtensionEventRouterForwarder*
TestingBrowserProcess::extension_event_router_forwarder() {
  return NULL;
}

NotificationUIManager* TestingBrowserProcess::notification_ui_manager() {
  if (!notification_ui_manager_.get())
    notification_ui_manager_.reset(
        NotificationUIManager::Create(local_state()));
  return notification_ui_manager_.get();
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
  DCHECK_GT(module_ref_count_, 0U);
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

printing::BackgroundPrintingManager*
TestingBrowserProcess::background_printing_manager() {
  if (!background_printing_manager_.get()) {
    background_printing_manager_.reset(
        new printing::BackgroundPrintingManager());
  }
  return background_printing_manager_.get();
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

ChromeNetLog* TestingBrowserProcess::net_log() {
  return NULL;
}

void TestingBrowserProcess::SetLocalState(PrefService* local_state) {
  local_state_ = local_state;
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
