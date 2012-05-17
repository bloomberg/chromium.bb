// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_browser_process.h"

#include "base/string_util.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/policy_service_impl.h"
#include "chrome/browser/policy/policy_service_stub.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "content/public/browser/notification_service.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/clipboard/clipboard.h"
#include "testing/gtest/include/gtest/gtest.h"

TestingBrowserProcess::TestingBrowserProcess()
    : notification_service_(content::NotificationService::Create()),
      module_ref_count_(0),
      app_locale_("en"),
      local_state_(NULL),
      io_thread_(NULL) {
}

TestingBrowserProcess::~TestingBrowserProcess() {
  EXPECT_FALSE(local_state_);
}

void TestingBrowserProcess::ResourceDispatcherHostCreated() {
}

void TestingBrowserProcess::EndSession() {
}

MetricsService* TestingBrowserProcess::metrics_service() {
  return NULL;
}

IOThread* TestingBrowserProcess::io_thread() {
  return io_thread_;
}

WatchDogThread* TestingBrowserProcess::watchdog_thread() {
  return NULL;
}

ProfileManager* TestingBrowserProcess::profile_manager() {
  return profile_manager_.get();
}

void TestingBrowserProcess::SetProfileManager(ProfileManager* profile_manager) {
  profile_manager_.reset(profile_manager);
}

PrefService* TestingBrowserProcess::local_state() {
  return local_state_;
}

VariationsService* TestingBrowserProcess::variations_service() {
  return NULL;
}

policy::BrowserPolicyConnector*
    TestingBrowserProcess::browser_policy_connector() {
#if defined(ENABLE_CONFIGURATION_POLICY)
  if (!browser_policy_connector_.get())
    browser_policy_connector_.reset(new policy::BrowserPolicyConnector());
#endif
  return browser_policy_connector_.get();
}

policy::PolicyService* TestingBrowserProcess::policy_service() {
  if (!policy_service_.get()) {
#if defined(ENABLE_CONFIGURATION_POLICY)
    policy_service_.reset(browser_policy_connector()->CreatePolicyService());
#else
    policy_service_.reset(new policy::PolicyServiceStub());
#endif
  }
  return policy_service_.get();
}

IconManager* TestingBrowserProcess::icon_manager() {
  return NULL;
}

ThumbnailGenerator* TestingBrowserProcess::GetThumbnailGenerator() {
  return NULL;
}

TabCloseableStateWatcher* TestingBrowserProcess::tab_closeable_state_watcher() {
  return NULL;
}

BackgroundModeManager* TestingBrowserProcess::background_mode_manager() {
  return NULL;
}

StatusTray* TestingBrowserProcess::status_tray() {
  return NULL;
}

SafeBrowsingService* TestingBrowserProcess::safe_browsing_service() {
  return sb_service_.get();
}

safe_browsing::ClientSideDetectionService*
TestingBrowserProcess::safe_browsing_detection_service() {
  return NULL;
}

net::URLRequestContextGetter* TestingBrowserProcess::system_request_context() {
  return NULL;
}

#if defined(OS_CHROMEOS)
chromeos::OomPriorityManager* TestingBrowserProcess::oom_priority_manager() {
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
#if defined(ENABLE_NOTIFICATIONS)
  if (!notification_ui_manager_.get())
    notification_ui_manager_.reset(
        NotificationUIManager::Create(local_state()));
  return notification_ui_manager_.get();
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

IntranetRedirectDetector* TestingBrowserProcess::intranet_redirect_detector() {
  return NULL;
}

AutomationProviderList* TestingBrowserProcess::GetAutomationProviderList() {
  return NULL;
}

void TestingBrowserProcess::InitDevToolsHttpProtocolHandler(
    Profile* profile,
    const std::string& ip,
    int port,
    const std::string& frontend_url) {
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
#if defined(OS_ANDROID)
  NOTIMPLEMENTED();
  return NULL;
#else
  if (!print_preview_tab_controller_.get())
    print_preview_tab_controller_ = new printing::PrintPreviewTabController();
  return print_preview_tab_controller_.get();
#endif
}

printing::BackgroundPrintingManager*
TestingBrowserProcess::background_printing_manager() {
#if defined(OS_ANDROID)
  NOTIMPLEMENTED();
  return NULL;
#else
  if (!background_printing_manager_.get()) {
    background_printing_manager_.reset(
        new printing::BackgroundPrintingManager());
  }
  return background_printing_manager_.get();
#endif
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

DownloadRequestLimiter* TestingBrowserProcess::download_request_limiter() {
  return NULL;
}

bool TestingBrowserProcess::plugin_finder_disabled() const {
  return false;
}

ChromeNetLog* TestingBrowserProcess::net_log() {
  return NULL;
}

prerender::PrerenderTracker* TestingBrowserProcess::prerender_tracker() {
  if (!prerender_tracker_.get())
    prerender_tracker_.reset(new prerender::PrerenderTracker());
  return prerender_tracker_.get();
}

ComponentUpdateService* TestingBrowserProcess::component_updater() {
  return NULL;
}

CRLSetFetcher* TestingBrowserProcess::crl_set_fetcher() {
  return NULL;
}

void TestingBrowserProcess::SetLocalState(PrefService* local_state) {
  if (!local_state && notification_ui_manager_.get())
    notification_ui_manager_.reset();  // Used local_state_.
  local_state_ = local_state;
}

void TestingBrowserProcess::SetIOThread(IOThread* io_thread) {
  io_thread_ = io_thread;
}

void TestingBrowserProcess::SetBrowserPolicyConnector(
    policy::BrowserPolicyConnector* connector) {
  browser_policy_connector_.reset(connector);
}

void TestingBrowserProcess::SetSafeBrowsingService(
    SafeBrowsingService* sb_service) {
  sb_service_ = sb_service;
}
