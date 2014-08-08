// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_browser_process.h"

#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/apps/chrome_apps_client.h"
#include "chrome/test/base/testing_browser_process_platform_part.h"
#include "components/network_time/network_time_tracker.h"
#include "content/public/browser/notification_service.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"

#if !defined(OS_IOS)
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#endif

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "components/policy/core/browser/browser_policy_connector.h"
#else
#include "components/policy/core/common/policy_service_stub.h"
#endif  // defined(ENABLE_CONFIGURATION_POLICY)

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_extensions_browser_client.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "components/storage_monitor/storage_monitor.h"
#include "components/storage_monitor/test_storage_monitor.h"
#endif

#if defined(ENABLE_FULL_PRINTING)
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#endif

// static
TestingBrowserProcess* TestingBrowserProcess::GetGlobal() {
  return static_cast<TestingBrowserProcess*>(g_browser_process);
}

// static
void TestingBrowserProcess::CreateInstance() {
  DCHECK(!g_browser_process);
  g_browser_process = new TestingBrowserProcess;
}

// static
void TestingBrowserProcess::DeleteInstance() {
  // g_browser_process must be NULL during its own destruction.
  BrowserProcess* browser_process = g_browser_process;
  g_browser_process = NULL;
  delete browser_process;
}

TestingBrowserProcess::TestingBrowserProcess()
    : notification_service_(content::NotificationService::Create()),
      module_ref_count_(0),
      app_locale_("en"),
      local_state_(NULL),
      io_thread_(NULL),
      system_request_context_(NULL),
      platform_part_(new TestingBrowserProcessPlatformPart()) {
#if defined(ENABLE_EXTENSIONS)
  extensions_browser_client_.reset(
      new extensions::ChromeExtensionsBrowserClient);
  apps::AppsClient::Set(ChromeAppsClient::GetInstance());
  extensions::ExtensionsBrowserClient::Set(extensions_browser_client_.get());
#endif
}

TestingBrowserProcess::~TestingBrowserProcess() {
  EXPECT_FALSE(local_state_);
#if defined(ENABLE_CONFIGURATION_POLICY)
  SetBrowserPolicyConnector(NULL);
#endif
#if defined(ENABLE_EXTENSIONS)
  extensions::ExtensionsBrowserClient::Set(NULL);
#endif

  // Destructors for some objects owned by TestingBrowserProcess will use
  // g_browser_process if it is not NULL, so it must be NULL before proceeding.
  DCHECK_EQ(static_cast<BrowserProcess*>(NULL), g_browser_process);
}

void TestingBrowserProcess::ResourceDispatcherHostCreated() {
}

void TestingBrowserProcess::EndSession() {
}

MetricsServicesManager* TestingBrowserProcess::GetMetricsServicesManager() {
  return NULL;
}

MetricsService* TestingBrowserProcess::metrics_service() {
  return NULL;
}

rappor::RapporService* TestingBrowserProcess::rappor_service() {
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
  // NotificationUIManager can contain references to elements in the current
  // ProfileManager (for example, the MessageCenterSettingsController maintains
  // a pointer to the ProfileInfoCache). So when we change the ProfileManager
  // (typically during test shutdown) make sure to reset any objects that might
  // maintain references to it. See SetLocalState() for a description of a
  // similar situation.
  notification_ui_manager_.reset();
  profile_manager_.reset(profile_manager);
#endif
}

PrefService* TestingBrowserProcess::local_state() {
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
    browser_policy_connector_ = platform_part_->CreateBrowserPolicyConnector();
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

GpuModeManager* TestingBrowserProcess::gpu_mode_manager() {
  return NULL;
}

BackgroundModeManager* TestingBrowserProcess::background_mode_manager() {
  return NULL;
}

void TestingBrowserProcess::set_background_mode_manager_for_test(
    scoped_ptr<BackgroundModeManager> manager) {
  NOTREACHED();
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

BrowserProcessPlatformPart* TestingBrowserProcess::platform_part() {
  return platform_part_.get();
}

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

message_center::MessageCenter* TestingBrowserProcess::message_center() {
  return message_center::MessageCenter::Get();
}

IntranetRedirectDetector* TestingBrowserProcess::intranet_redirect_detector() {
  return NULL;
}
void TestingBrowserProcess::CreateDevToolsHttpProtocolHandler(
    chrome::HostDesktopType host_desktop_type,
    const std::string& ip,
    int port) {
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
#if defined(ENABLE_FULL_PRINTING)
  if (!print_job_manager_.get())
    print_job_manager_.reset(new printing::PrintJobManager());
  return print_job_manager_.get();
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

printing::PrintPreviewDialogController*
TestingBrowserProcess::print_preview_dialog_controller() {
#if defined(ENABLE_FULL_PRINTING)
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
#if defined(ENABLE_FULL_PRINTING)
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

component_updater::ComponentUpdateService*
TestingBrowserProcess::component_updater() {
  return NULL;
}

CRLSetFetcher* TestingBrowserProcess::crl_set_fetcher() {
  return NULL;
}

component_updater::PnaclComponentInstaller*
TestingBrowserProcess::pnacl_component_installer() {
  return NULL;
}

MediaFileSystemRegistry* TestingBrowserProcess::media_file_system_registry() {
#if defined(OS_IOS) || defined(OS_ANDROID)
  NOTIMPLEMENTED();
  return NULL;
#else
  if (!media_file_system_registry_)
    media_file_system_registry_.reset(new MediaFileSystemRegistry());
  return media_file_system_registry_.get();
#endif
}

bool TestingBrowserProcess::created_local_state() const {
    return (local_state_ != NULL);
}

#if defined(ENABLE_WEBRTC)
WebRtcLogUploader* TestingBrowserProcess::webrtc_log_uploader() {
  return NULL;
}
#endif

network_time::NetworkTimeTracker*
TestingBrowserProcess::network_time_tracker() {
  if (!network_time_tracker_) {
    DCHECK(local_state_);
    network_time_tracker_.reset(new network_time::NetworkTimeTracker(
        scoped_ptr<base::TickClock>(new base::DefaultTickClock()),
        local_state_));
  }
  return network_time_tracker_.get();
}

gcm::GCMDriver* TestingBrowserProcess::gcm_driver() {
  return NULL;
}

void TestingBrowserProcess::SetSystemRequestContext(
    net::URLRequestContextGetter* context_getter) {
  system_request_context_ = context_getter;
}

void TestingBrowserProcess::SetLocalState(PrefService* local_state) {
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
    network_time_tracker_.reset();
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
  if (browser_policy_connector_) {
    browser_policy_connector_->Shutdown();
  }
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

///////////////////////////////////////////////////////////////////////////////

TestingBrowserProcessInitializer::TestingBrowserProcessInitializer() {
  TestingBrowserProcess::CreateInstance();
}

TestingBrowserProcessInitializer::~TestingBrowserProcessInitializer() {
  TestingBrowserProcess::DeleteInstance();
}
