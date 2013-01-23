// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_browser_process.h"

#include "base/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/bookmarks/bookmark_prompt_controller.h"
#include "content/public/browser/notification_service.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(OS_IOS)
#include "chrome/browser/media_gallery/media_file_system_registry.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/thumbnails/render_widget_snapshot_taker.h"
#endif

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/browser_policy_connector.h"
#else
#include "chrome/browser/policy/policy_service_stub.h"
#endif  // defined(ENABLE_CONFIGURATION_POLICY)

#if defined(ENABLE_MESSAGE_CENTER) && defined(USE_ASH)
#include "ash/shell.h"
#endif

#if defined(ENABLE_MESSAGE_CENTER)
#include "ui/message_center/message_center.h"
#endif

// static
TestingBrowserProcess* TestingBrowserProcess::GetGlobal() {
  return static_cast<TestingBrowserProcess*>(g_browser_process);
}

TestingBrowserProcess::TestingBrowserProcess()
    : notification_service_(content::NotificationService::Create()),
      module_ref_count_(0),
      app_locale_("en"),
#if !defined(OS_IOS)
      render_widget_snapshot_taker_(new RenderWidgetSnapshotTaker),
#endif
      local_state_(NULL),
      io_thread_(NULL),
      system_request_context_(NULL) {
}

TestingBrowserProcess::~TestingBrowserProcess() {
  EXPECT_FALSE(local_state_);
#if defined(ENABLE_CONFIGURATION_POLICY)
  if (browser_policy_connector_)
    browser_policy_connector_->Shutdown();
#endif
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
#if defined(OS_IOS)
  NOTIMPLEMENTED();
  return NULL;
#else
  return profile_manager_.get();
#endif
}

void TestingBrowserProcess::SetProfileManager(ProfileManager* profile_manager) {
#if !defined(OS_IOS)
  profile_manager_.reset(profile_manager);
#endif
}

PrefServiceSimple* TestingBrowserProcess::local_state() {
  return local_state_;
}

chrome_variations::VariationsService*
    TestingBrowserProcess::variations_service() {
  return NULL;
}

policy::BrowserPolicyConnector*
    TestingBrowserProcess::browser_policy_connector() {
#if defined(ENABLE_CONFIGURATION_POLICY)
  if (!browser_policy_connector_)
    browser_policy_connector_.reset(new policy::BrowserPolicyConnector());
  return browser_policy_connector_.get();
#else
  return NULL;
#endif
}

policy::PolicyService* TestingBrowserProcess::policy_service() {
#if defined(OS_IOS)
  NOTIMPLEMENTED();
  return NULL;
#elif defined(ENABLE_CONFIGURATION_POLICY)
  return browser_policy_connector()->GetPolicyService();
#else
  if (!policy_service_)
    policy_service_.reset(new policy::PolicyServiceStub());
  return policy_service_.get();
#endif
}

IconManager* TestingBrowserProcess::icon_manager() {
  return NULL;
}

GLStringManager* TestingBrowserProcess::gl_string_manager() {
  return NULL;
}

RenderWidgetSnapshotTaker*
TestingBrowserProcess::GetRenderWidgetSnapshotTaker() {
#if defined(OS_IOS)
  NOTREACHED();
  return NULL;
#else
  return render_widget_snapshot_taker_.get();
#endif
}

BackgroundModeManager* TestingBrowserProcess::background_mode_manager() {
  return NULL;
}

StatusTray* TestingBrowserProcess::status_tray() {
  return NULL;
}

SafeBrowsingService* TestingBrowserProcess::safe_browsing_service() {
#if defined(OS_IOS)
  NOTIMPLEMENTED();
  return NULL;
#else
  return sb_service_.get();
#endif
}

safe_browsing::ClientSideDetectionService*
TestingBrowserProcess::safe_browsing_detection_service() {
  return NULL;
}

net::URLRequestContextGetter* TestingBrowserProcess::system_request_context() {
  return system_request_context_;
}

#if defined(OS_CHROMEOS)
chromeos::OomPriorityManager* TestingBrowserProcess::oom_priority_manager() {
  return NULL;
}
#endif  // defined(OS_CHROMEOS)

extensions::EventRouterForwarder*
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

#if defined(ENABLE_MESSAGE_CENTER)
message_center::MessageCenter* TestingBrowserProcess::message_center() {
#if defined(USE_ASH)
    return ash::Shell::GetInstance()->message_center();
#else
  if (!message_center_.get())
    message_center_.reset(new message_center::MessageCenter());
  return message_center_.get();
#endif
}
#endif

IntranetRedirectDetector* TestingBrowserProcess::intranet_redirect_detector() {
  return NULL;
}

AutomationProviderList* TestingBrowserProcess::GetAutomationProviderList() {
  return NULL;
}

void TestingBrowserProcess::CreateDevToolsHttpProtocolHandler(
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

printing::PrintPreviewDialogController*
TestingBrowserProcess::print_preview_dialog_controller() {
#if defined(ENABLE_PRINTING)
  if (!print_preview_dialog_controller_.get())
    print_preview_dialog_controller_ =
        new printing::PrintPreviewDialogController();
  return print_preview_dialog_controller_.get();
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

printing::BackgroundPrintingManager*
TestingBrowserProcess::background_printing_manager() {
#if defined(ENABLE_PRINTING)
  if (!background_printing_manager_.get()) {
    background_printing_manager_.reset(
        new printing::BackgroundPrintingManager());
  }
  return background_printing_manager_.get();
#else
  NOTIMPLEMENTED();
  return NULL;
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

ChromeNetLog* TestingBrowserProcess::net_log() {
  return NULL;
}

prerender::PrerenderTracker* TestingBrowserProcess::prerender_tracker() {
#if defined(OS_IOS)
  NOTIMPLEMENTED();
  return NULL;
#else
  if (!prerender_tracker_.get())
    prerender_tracker_.reset(new prerender::PrerenderTracker());
  return prerender_tracker_.get();
#endif
}

ComponentUpdateService* TestingBrowserProcess::component_updater() {
  return NULL;
}

CRLSetFetcher* TestingBrowserProcess::crl_set_fetcher() {
  return NULL;
}

BookmarkPromptController* TestingBrowserProcess::bookmark_prompt_controller() {
#if defined(OS_IOS)
  NOTIMPLEMENTED();
  return NULL;
#else
  return bookmark_prompt_controller_.get();
#endif
}

chrome::MediaFileSystemRegistry*
TestingBrowserProcess::media_file_system_registry() {
#if defined(OS_IOS)
  NOTIMPLEMENTED();
  return NULL;
#else
  if (!media_file_system_registry_)
    media_file_system_registry_.reset(new chrome::MediaFileSystemRegistry());
  return media_file_system_registry_.get();
#endif
}

void TestingBrowserProcess::PlatformSpecificCommandLineProcessing(
    const CommandLine& command_line) {
}

void TestingBrowserProcess::SetBookmarkPromptController(
    BookmarkPromptController* controller) {
#if !defined(OS_IOS)
  bookmark_prompt_controller_.reset(controller);
#endif
}

void TestingBrowserProcess::SetSystemRequestContext(
    net::URLRequestContextGetter* context_getter) {
  system_request_context_ = context_getter;
}

void TestingBrowserProcess::SetLocalState(PrefServiceSimple* local_state) {
  if (!local_state) {
    // The local_state_ PrefService is owned outside of TestingBrowserProcess,
    // but some of the members of TestingBrowserProcess hold references to it
    // (for example, via PrefNotifier members). But given our test
    // infrastructure which tears down individual tests before freeing the
    // TestingBrowserProcess, there's not a good way to make local_state outlive
    // these dependencies. As a workaround, whenever local_state_ is cleared
    // (assumedly as part of exiting the test and freeing TestingBrowserProcess)
    // any components owned by TestingBrowserProcess that depend on local_state
    // are also freed.
#if !defined(OS_IOS)
    notification_ui_manager_.reset();
#endif
#if defined(ENABLE_CONFIGURATION_POLICY)
    SetBrowserPolicyConnector(NULL);
#endif
  }
  local_state_ = local_state;
}

void TestingBrowserProcess::SetIOThread(IOThread* io_thread) {
  io_thread_ = io_thread;
}

void TestingBrowserProcess::SetBrowserPolicyConnector(
    policy::BrowserPolicyConnector* connector) {
#if defined(ENABLE_CONFIGURATION_POLICY)
  if (browser_policy_connector_)
    browser_policy_connector_->Shutdown();
  browser_policy_connector_.reset(connector);
#else
  CHECK(false);
#endif
}

void TestingBrowserProcess::SetSafeBrowsingService(
    SafeBrowsingService* sb_service) {
#if !defined(OS_IOS)
  NOTIMPLEMENTED();
  sb_service_ = sb_service;
#endif
}
