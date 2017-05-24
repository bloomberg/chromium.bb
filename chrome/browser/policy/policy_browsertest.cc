// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/accelerators/accelerator_controller_delegate_aura.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/component_updater/chrome_component_updater_configurator.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/api/messaging/message_service.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_cache_fake.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_devices_controller.h"
#include "chrome/browser/metrics/variations/chrome_variations_service_client.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/policy/cloud/test_request_interceptor.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/component_updater/component_updater_service.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/infobars/core/infobar.h"
#include "components/network_time/network_time_tracker.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/url_request_post_interceptor.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/mock_notification_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/scoped_ignore_content_verifier_for_test.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "media/media_features.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_stream_factory.h"
#include "net/http/transport_security_state.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/manager/display_manager.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_CHROMEOS)
#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/accessibility_types.h"
#include "ash/aura/shell_port_classic.h"
#include "ash/shell.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/system/timezone_resolver_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/chrome_screenshot_grabber.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "components/arc/arc_session_runner.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/snapshot/screenshot_grabber.h"
#endif

#if !defined(OS_MACOSX)
#include "base/compiler_specific.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "ui/base/window_open_disposition.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/media_router_action_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#endif  // !defined(OS_ANDROID)

using content::BrowserThread;
using net::URLRequestMockHTTPJob;
using testing::Mock;
using testing::Return;
using testing::_;

namespace policy {

namespace {

#if defined(OS_CHROMEOS)
const int kOneHourInMs = 60 * 60 * 1000;
const int kThreeHoursInMs = 180 * 60 * 1000;
#endif

const char kURL[] = "http://example.com";
const char kCookieValue[] = "converted=true";
// Assigned to Philip J. Fry to fix eventually.
// TODO(maksims): use year 3000 when we get rid off the 32-bit
// versions. https://crbug.com/619828
const char kCookieOptions[] = ";expires=Wed Jan 01 2038 00:00:00 GMT";

const base::FilePath::CharType kTestExtensionsDir[] =
    FILE_PATH_LITERAL("extensions");
const base::FilePath::CharType kGoodCrxName[] = FILE_PATH_LITERAL("good.crx");
const base::FilePath::CharType kAdBlockCrxName[] =
    FILE_PATH_LITERAL("adblock.crx");
const base::FilePath::CharType kHostedAppCrxName[] =
    FILE_PATH_LITERAL("hosted_app.crx");

const char kGoodCrxId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char kAdBlockCrxId[] = "dojnnbeimaimaojcialkkgajdnefpgcn";
const char kHostedAppCrxId[] = "kbmnembihfiondgfjekmnmcbddelicoi";

const base::FilePath::CharType kGood2CrxManifestName[] =
    FILE_PATH_LITERAL("good2_update_manifest.xml");
const base::FilePath::CharType kGoodV1CrxManifestName[] =
    FILE_PATH_LITERAL("good_v1_update_manifest.xml");
const base::FilePath::CharType kGoodV1CrxName[] =
    FILE_PATH_LITERAL("good_v1.crx");
const base::FilePath::CharType kGoodUnpackedExt[] =
    FILE_PATH_LITERAL("good_unpacked");
const base::FilePath::CharType kAppUnpackedExt[] =
    FILE_PATH_LITERAL("app");

#if !defined(OS_MACOSX)
const base::FilePath::CharType kUnpackedFullscreenAppName[] =
    FILE_PATH_LITERAL("fullscreen_app");
#endif  // !defined(OS_MACOSX)

#if BUILDFLAG(ENABLE_WEBRTC)
// Arbitrary port range for testing the WebRTC UDP port policy.
const char kTestWebRtcUdpPortRange[] = "10000-10100";
#endif

// Filters requests to the hosts in |urls| and redirects them to the test data
// dir through URLRequestMockHTTPJobs.
void RedirectHostsToTestData(const char* const urls[], size_t size) {
  // Map the given hosts to the test data dir.
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  base::FilePath base_path;
  PathService::Get(chrome::DIR_TEST_DATA, &base_path);
  for (size_t i = 0; i < size; ++i) {
    const GURL url(urls[i]);
    EXPECT_TRUE(url.is_valid());
    filter->AddUrlInterceptor(url,
                              URLRequestMockHTTPJob::CreateInterceptor(
                                  base_path, BrowserThread::GetBlockingPool()));
  }
}

// Remove filters for requests to the hosts in |urls|.
void UndoRedirectHostsToTestData(const char* const urls[], size_t size) {
  // Map the given hosts to the test data dir.
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  for (size_t i = 0; i < size; ++i) {
    const GURL url(urls[i]);
    EXPECT_TRUE(url.is_valid());
    filter->RemoveUrlHandler(url);
  }
}

// Fails requests using ERR_CONNECTION_RESET.
class FailedJobInterceptor : public net::URLRequestInterceptor {
 public:
  FailedJobInterceptor() {}
  ~FailedJobInterceptor() override {}

  // URLRequestInterceptor implementation:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new net::URLRequestFailedJob(request, network_delegate,
                                        net::ERR_CONNECTION_RESET);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FailedJobInterceptor);
};

// While |MakeRequestFail| is in scope URLRequests to |host| will fail.
class MakeRequestFail {
 public:
  // Sets up the filter on IO thread such that requests to |host| fail.
  explicit MakeRequestFail(const std::string& host) : host_(host) {
    BrowserThread::PostTaskAndReply(BrowserThread::IO, FROM_HERE,
                                    base::BindOnce(MakeRequestFailOnIO, host_),
                                    base::MessageLoop::QuitWhenIdleClosure());
    content::RunMessageLoop();
  }
  ~MakeRequestFail() {
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(UndoMakeRequestFailOnIO, host_),
        base::MessageLoop::QuitWhenIdleClosure());
    content::RunMessageLoop();
  }

 private:
  // Filters requests to the |host| such that they fail. Run on IO thread.
  static void MakeRequestFailOnIO(const std::string& host) {
    net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
    filter->AddHostnameInterceptor("http", host,
                                   std::unique_ptr<net::URLRequestInterceptor>(
                                       new FailedJobInterceptor()));
    filter->AddHostnameInterceptor("https", host,
                                   std::unique_ptr<net::URLRequestInterceptor>(
                                       new FailedJobInterceptor()));
  }

  // Remove filters for requests to the |host|. Run on IO thread.
  static void UndoMakeRequestFailOnIO(const std::string& host) {
    net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
    filter->RemoveHostnameHandler("http", host);
    filter->RemoveHostnameHandler("https", host);
  }

  const std::string host_;
};

// Verifies that the given url |spec| can be opened. This assumes that |spec|
// points at empty.html in the test data dir.
void CheckCanOpenURL(Browser* browser, const char* spec) {
  GURL url(spec);
  ui_test_utils::NavigateToURL(browser, url);
  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(url, contents->GetURL());

  base::string16 blocked_page_title;
  if (url.has_host()) {
    blocked_page_title = base::UTF8ToUTF16(url.host());
  } else {
    // Local file paths show the full URL.
    blocked_page_title = base::UTF8ToUTF16(url.spec());
  }
  EXPECT_NE(blocked_page_title, contents->GetTitle());
}

// Verifies that access to the given url |spec| is blocked.
void CheckURLIsBlocked(Browser* browser, const char* spec) {
  GURL url(spec);
  ui_test_utils::NavigateToURL(browser, url);
  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(url, contents->GetURL());

  base::string16 blocked_page_title;
  if (url.has_host()) {
    blocked_page_title = base::UTF8ToUTF16(url.host());
  } else {
    // Local file paths show the full URL.
    blocked_page_title = base::UTF8ToUTF16(url.spec());
  }
  EXPECT_EQ(blocked_page_title, contents->GetTitle());

  // Verify that the expected error page is being displayed.
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      contents,
      "var textContent = document.body.textContent;"
      "var hasError = textContent.indexOf('ERR_BLOCKED_BY_ADMINISTRATOR') >= 0;"
      "domAutomationController.send(hasError);",
      &result));
  EXPECT_TRUE(result);
}

// Downloads a file named |file| and expects it to be saved to |dir|, which
// must be empty.
void DownloadAndVerifyFile(
    Browser* browser, const base::FilePath& dir, const base::FilePath& file) {
  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(browser->profile());
  content::DownloadTestObserverTerminal observer(
      download_manager, 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file.MaybeAsASCII()));
  base::FilePath downloaded = dir.Append(file);
  EXPECT_FALSE(base::PathExists(downloaded));
  ui_test_utils::NavigateToURL(browser, url);
  observer.WaitForFinished();
  EXPECT_EQ(
      1u, observer.NumDownloadsSeenInState(content::DownloadItem::COMPLETE));
  EXPECT_TRUE(base::PathExists(downloaded));
  base::FileEnumerator enumerator(dir, false, base::FileEnumerator::FILES);
  EXPECT_EQ(file, enumerator.Next().BaseName());
  EXPECT_EQ(base::FilePath(), enumerator.Next());
}

#if defined(OS_CHROMEOS)
int CountScreenshots() {
  DownloadPrefs* download_prefs = DownloadPrefs::FromBrowserContext(
      ProfileManager::GetActiveUserProfile());
  base::FileEnumerator enumerator(download_prefs->DownloadPath(),
                                  false, base::FileEnumerator::FILES,
                                  "Screenshot*");
  int count = 0;
  while (!enumerator.Next().empty())
    count++;
  return count;
}
#endif

// Checks if WebGL is enabled in the given WebContents.
bool IsWebGLEnabled(content::WebContents* contents) {
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      contents,
      "var canvas = document.createElement('canvas');"
      "var context = canvas.getContext('webgl');"
      "domAutomationController.send(context != null);",
      &result));
  return result;
}

bool IsJavascriptEnabled(content::WebContents* contents) {
  std::unique_ptr<base::Value> value =
      content::ExecuteScriptAndGetValue(contents->GetMainFrame(), "123");
  int result = 0;
  if (!value->GetAsInteger(&result))
    EXPECT_EQ(base::Value::Type::NONE, value->GetType());
  return result == 123;
}

bool IsNetworkPredictionEnabled(PrefService* prefs) {
  return chrome_browser_net::CanPrefetchAndPrerenderUI(prefs) ==
      chrome_browser_net::NetworkPredictionStatus::ENABLED;
}

void FlushBlacklistPolicy() {
  // Updates of the URLBlacklist are done on IO, after building the blacklist
  // on the blocking pool, which is initiated from IO.
  content::RunAllPendingInMessageLoop(BrowserThread::IO);
  BrowserThread::GetBlockingPool()->FlushForTesting();
  content::RunAllPendingInMessageLoop(BrowserThread::IO);
}

bool ContainsVisibleElement(content::WebContents* contents,
                            const std::string& id) {
  bool result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      contents,
      "var elem = document.getElementById('" + id + "');"
      "domAutomationController.send(!!elem && !elem.hidden);",
      &result));
  return result;
}

#if defined(OS_CHROMEOS)
class TestAudioObserver : public chromeos::CrasAudioHandler::AudioObserver {
 public:
  TestAudioObserver() : output_mute_changed_count_(0) {
  }

  int output_mute_changed_count() const {
    return output_mute_changed_count_;
  }

  ~TestAudioObserver() override {}

 protected:
  // chromeos::CrasAudioHandler::AudioObserver overrides.
  void OnOutputMuteChanged(bool /* mute_on */,
                           bool /* system_adjust */) override {
    ++output_mute_changed_count_;
  }

 private:
  int output_mute_changed_count_;

  DISALLOW_COPY_AND_ASSIGN(TestAudioObserver);
};
#endif

// This class waits until either a load stops or the WebContents is destroyed.
class WebContentsLoadedOrDestroyedWatcher
    : public content::WebContentsObserver {
 public:
  explicit WebContentsLoadedOrDestroyedWatcher(
      content::WebContents* web_contents);
  ~WebContentsLoadedOrDestroyedWatcher() override;

  // Waits until the WebContents's load is done or until it is destroyed.
  void Wait();

  // Overridden WebContentsObserver methods.
  void WebContentsDestroyed() override;
  void DidStopLoading() override;

 private:
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsLoadedOrDestroyedWatcher);
};

WebContentsLoadedOrDestroyedWatcher::WebContentsLoadedOrDestroyedWatcher(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      message_loop_runner_(new content::MessageLoopRunner) {
}

WebContentsLoadedOrDestroyedWatcher::~WebContentsLoadedOrDestroyedWatcher() {}

void WebContentsLoadedOrDestroyedWatcher::Wait() {
  message_loop_runner_->Run();
}

void WebContentsLoadedOrDestroyedWatcher::WebContentsDestroyed() {
  message_loop_runner_->Quit();
}

void WebContentsLoadedOrDestroyedWatcher::DidStopLoading() {
  message_loop_runner_->Quit();
}

#if !defined(OS_MACOSX)

// Observer used to wait for the creation of a new app window.
class TestAddAppWindowObserver
    : public extensions::AppWindowRegistry::Observer {
 public:
  explicit TestAddAppWindowObserver(extensions::AppWindowRegistry* registry);
  ~TestAddAppWindowObserver() override;

  // extensions::AppWindowRegistry::Observer:
  void OnAppWindowAdded(extensions::AppWindow* app_window) override;

  extensions::AppWindow* WaitForAppWindow();

 private:
  extensions::AppWindowRegistry* registry_;  // Not owned.
  extensions::AppWindow* window_;            // Not owned.
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestAddAppWindowObserver);
};

TestAddAppWindowObserver::TestAddAppWindowObserver(
    extensions::AppWindowRegistry* registry)
    : registry_(registry), window_(NULL) {
  registry_->AddObserver(this);
}

TestAddAppWindowObserver::~TestAddAppWindowObserver() {
  registry_->RemoveObserver(this);
}

void TestAddAppWindowObserver::OnAppWindowAdded(
    extensions::AppWindow* app_window) {
  window_ = app_window;
  run_loop_.Quit();
}

extensions::AppWindow* TestAddAppWindowObserver::WaitForAppWindow() {
  run_loop_.Run();
  return window_;
}

#endif

}  // namespace

class PolicyTest : public InProcessBrowserTest {
 protected:
  PolicyTest() {}
  ~PolicyTest() override {}

  void SetUp() override {
    test_extension_cache_.reset(new extensions::ExtensionCacheFake());
    InProcessBrowserTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch("noerrdialogs");
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetUpOnMainThread() override {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(chrome_browser_net::SetUrlRequestMocksEnabled, true));
    if (extension_service()->updater()) {
      extension_service()->updater()->SetExtensionCacheForTesting(
          test_extension_cache_.get());
    }
  }

  // Makes URLRequestMockHTTPJobs serve data from content::DIR_TEST_DATA
  // instead of chrome::DIR_TEST_DATA.
  void ServeContentTestData() {
    base::FilePath root_http;
    PathService::Get(content::DIR_TEST_DATA, &root_http);
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(URLRequestMockHTTPJob::AddUrlHandlers, root_http,
                       make_scoped_refptr(BrowserThread::GetBlockingPool())),
        base::MessageLoop::current()->QuitWhenIdleClosure());
    content::RunMessageLoop();
  }

  void SetScreenshotPolicy(bool enabled) {
    PolicyMap policies;
    policies.Set(key::kDisableScreenshots, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::MakeUnique<base::Value>(!enabled), nullptr);
    UpdateProviderPolicy(policies);
  }

#if defined(OS_CHROMEOS)
  class QuitMessageLoopAfterScreenshot : public ui::ScreenshotGrabberObserver {
   public:
    void OnScreenshotCompleted(
        ScreenshotGrabberObserver::Result screenshot_result,
        const base::FilePath& screenshot_path) override {
      BrowserThread::PostTaskAndReply(BrowserThread::IO, FROM_HERE,
                                      base::Bind(base::DoNothing),
                                      base::MessageLoop::QuitWhenIdleClosure());
    }

    ~QuitMessageLoopAfterScreenshot() override {}
  };

  void TestScreenshotFile(bool enabled) {
    // AddObserver is an ash-specific method, so just replace the screenshot
    // grabber with one we've created here.
    std::unique_ptr<ChromeScreenshotGrabber> chrome_screenshot_grabber(
        new ChromeScreenshotGrabber);
    // ScreenshotGrabber doesn't own this observer, so the observer's lifetime
    // is tied to the test instead.
    chrome_screenshot_grabber->screenshot_grabber()->AddObserver(&observer_);
    ash::ShellPortClassic::Get()
        ->accelerator_controller_delegate()
        ->SetScreenshotDelegate(std::move(chrome_screenshot_grabber));

    SetScreenshotPolicy(enabled);
    ash::Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
        ash::TAKE_SCREENSHOT);

    content::RunMessageLoop();
    static_cast<ChromeScreenshotGrabber*>(
        ash::ShellPortClassic::Get()
            ->accelerator_controller_delegate()
            ->screenshot_delegate())
        ->screenshot_grabber()
        ->RemoveObserver(&observer_);
  }
#endif

  ExtensionService* extension_service() {
    extensions::ExtensionSystem* system =
        extensions::ExtensionSystem::Get(browser()->profile());
    return system->extension_service();
  }

  const extensions::Extension* InstallExtension(
      const base::FilePath::StringType& name) {
    base::FilePath extension_path(ui_test_utils::GetTestFilePath(
        base::FilePath(kTestExtensionsDir), base::FilePath(name)));
    scoped_refptr<extensions::CrxInstaller> installer =
        extensions::CrxInstaller::CreateSilent(extension_service());
    installer->set_allow_silent_install(true);
    installer->set_install_cause(extension_misc::INSTALL_CAUSE_UPDATE);
    installer->set_creation_flags(extensions::Extension::FROM_WEBSTORE);

    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    installer->InstallCrx(extension_path);
    observer.Wait();
    content::Details<const extensions::Extension> details = observer.details();
    return details.ptr();
  }

  scoped_refptr<const extensions::Extension> LoadUnpackedExtension(
      const base::FilePath::StringType& name) {
    base::FilePath extension_path(ui_test_utils::GetTestFilePath(
        base::FilePath(kTestExtensionsDir), base::FilePath(name)));
    extensions::ChromeTestExtensionLoader loader(browser()->profile());
    return loader.LoadExtension(extension_path);
  }

  void UninstallExtension(const std::string& id, bool expect_success) {
    if (expect_success) {
      extensions::TestExtensionRegistryObserver observer(
          extensions::ExtensionRegistry::Get(browser()->profile()));
      extension_service()->UninstallExtension(
          id, extensions::UNINSTALL_REASON_FOR_TESTING,
          base::Bind(&base::DoNothing), NULL);
      observer.WaitForExtensionUninstalled();
    } else {
      content::WindowedNotificationObserver observer(
          extensions::NOTIFICATION_EXTENSION_UNINSTALL_NOT_ALLOWED,
          content::NotificationService::AllSources());
      extension_service()->UninstallExtension(
          id,
          extensions::UNINSTALL_REASON_FOR_TESTING,
          base::Bind(&base::DoNothing),
          NULL);
      observer.Wait();
    }
  }

  void DisableExtension(const std::string& id) {
    extensions::TestExtensionRegistryObserver observer(
        extensions::ExtensionRegistry::Get(browser()->profile()));
    extension_service()->DisableExtension(id,
                                          extensions::Extension::DISABLE_NONE);
    observer.WaitForExtensionUnloaded();
  }

  void UpdateProviderPolicy(const PolicyMap& policy) {
    provider_.UpdateChromePolicy(policy);
    DCHECK(base::MessageLoop::current());
    base::RunLoop loop;
    loop.RunUntilIdle();
  }

  // Sends a mouse click at the given coordinates to the current renderer.
  void PerformClick(int x, int y) {
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    blink::WebMouseEvent click_event(
        blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::kTimeStampForTesting);
    click_event.button = blink::WebMouseEvent::Button::kLeft;
    click_event.click_count = 1;
    click_event.SetPositionInWidget(x, y);
    contents->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(click_event);
    click_event.SetType(blink::WebInputEvent::kMouseUp);
    contents->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(click_event);
  }

  void SetPolicy(PolicyMap* policies,
                 const char* key,
                 std::unique_ptr<base::Value> value) {
    if (value) {
      policies->Set(key, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                    POLICY_SOURCE_CLOUD, std::move(value), nullptr);
    } else {
      policies->Erase(key);
    }
  }

  void ApplySafeSearchPolicy(std::unique_ptr<base::Value> legacy_safe_search,
                             std::unique_ptr<base::Value> google_safe_search,
                             std::unique_ptr<base::Value> legacy_youtube,
                             std::unique_ptr<base::Value> youtube_restrict) {
    PolicyMap policies;
    SetPolicy(&policies, key::kForceSafeSearch, std::move(legacy_safe_search));
    SetPolicy(&policies, key::kForceGoogleSafeSearch,
              std::move(google_safe_search));
    SetPolicy(&policies, key::kForceYouTubeSafetyMode,
              std::move(legacy_youtube));
    SetPolicy(&policies, key::kForceYouTubeRestrict,
              std::move(youtube_restrict));
    UpdateProviderPolicy(policies);
  }

  void CheckSafeSearch(bool expect_safe_search) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver observer(web_contents);
    chrome::FocusLocationBar(browser());
    LocationBar* location_bar = browser()->window()->GetLocationBar();
    ui_test_utils::SendToOmniboxAndSubmit(location_bar, "http://google.com/");
    OmniboxEditModel* model = location_bar->GetOmniboxView()->model();
    observer.Wait();
    EXPECT_TRUE(model->CurrentMatch(NULL).destination_url.is_valid());

    std::string expected_url("http://google.com/");
    if (expect_safe_search) {
      expected_url += "?" + std::string(chrome::kSafeSearchSafeParameter) +
                      "&" + chrome::kSafeSearchSsuiParameter;
    }
    EXPECT_EQ(GURL(expected_url), web_contents->GetURL());
  }

  MockConfigurationPolicyProvider provider_;
  std::unique_ptr<extensions::ExtensionCacheFake> test_extension_cache_;
  extensions::ScopedIgnoreContentVerifierForTest ignore_content_verifier_;
#if defined(OS_CHROMEOS)
  QuitMessageLoopAfterScreenshot observer_;
#endif
};

#if defined(OS_WIN)
// This policy only exists on Windows.

// Sets the locale policy before the browser is started.
class LocalePolicyTest : public PolicyTest {
 public:
  LocalePolicyTest() {}
  ~LocalePolicyTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(key::kApplicationLocaleValue, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::MakeUnique<base::Value>("fr"), nullptr);
    provider_.UpdateChromePolicy(policies);
    // The "en-US" ResourceBundle is always loaded before this step for tests,
    // but in this test we want the browser to load the bundle as it
    // normally would.
    ResourceBundle::CleanupSharedInstance();
  }
};

IN_PROC_BROWSER_TEST_F(LocalePolicyTest, ApplicationLocaleValue) {
  // Verifies that the default locale can be overridden with policy.
  EXPECT_EQ("fr", g_browser_process->GetApplicationLocale());
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  base::string16 french_title = l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);
  base::string16 title;
  EXPECT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &title));
  EXPECT_EQ(french_title, title);

  // Make sure this is really French and differs from the English title.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  std::string loaded =
      ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources("en-US");
  EXPECT_EQ("en-US", loaded);
  base::string16 english_title = l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);
  EXPECT_NE(french_title, english_title);
}
#endif

IN_PROC_BROWSER_TEST_F(PolicyTest, BookmarkBarEnabled) {
  // Verifies that the bookmarks bar can be forced to always or never show up.

  // Test starts in about:blank.
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_FALSE(prefs->IsManagedPreference(bookmarks::prefs::kShowBookmarkBar));
  EXPECT_FALSE(prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar));
  EXPECT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());

  PolicyMap policies;
  policies.Set(key::kBookmarkBarEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(true), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(bookmarks::prefs::kShowBookmarkBar));
  EXPECT_TRUE(prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar));
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());

  // The NTP has special handling of the bookmark bar.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());

  policies.Set(key::kBookmarkBarEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(bookmarks::prefs::kShowBookmarkBar));
  EXPECT_FALSE(prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar));
  // The bookmark bar is hidden in the NTP when disabled by policy.
  EXPECT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());

  policies.Clear();
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(prefs->IsManagedPreference(bookmarks::prefs::kShowBookmarkBar));
  EXPECT_FALSE(prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar));
  // The bookmark bar is shown detached in the NTP, when disabled by prefs only.
  EXPECT_EQ(BookmarkBar::DETACHED, browser()->bookmark_bar_state());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, PRE_PRE_DefaultCookiesSetting) {
  // Verifies that cookies are deleted on shutdown. This test is split in 3
  // parts because it spans 2 browser restarts.

  Profile* profile = browser()->profile();
  GURL url(kURL);
  // No cookies at startup.
  EXPECT_TRUE(content::GetCookies(profile, url).empty());
  // Set a cookie now.
  std::string value = std::string(kCookieValue) + std::string(kCookieOptions);
  EXPECT_TRUE(content::SetCookie(profile, url, value));
  // Verify it was set.
  EXPECT_EQ(kCookieValue, GetCookies(profile, url));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, PRE_DefaultCookiesSetting) {
  // Verify that the cookie persists across restarts.
  EXPECT_EQ(kCookieValue, GetCookies(browser()->profile(), GURL(kURL)));
  // Now set the policy and the cookie should be gone after another restart.
  PolicyMap policies;
  policies.Set(key::kDefaultCookiesSetting, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(4), nullptr);
  UpdateProviderPolicy(policies);
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DefaultCookiesSetting) {
  // Verify that the cookie is gone.
  EXPECT_TRUE(GetCookies(browser()->profile(), GURL(kURL)).empty());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DefaultSearchProvider) {
  MakeRequestFail make_request_fail("search.example");

  // Verifies that a default search is made using the provider configured via
  // policy. Also checks that default search can be completely disabled.
  const base::string16 kKeyword(base::ASCIIToUTF16("testsearch"));
  const std::string kSearchURL("http://search.example/search?q={searchTerms}");
  const std::string kInstantURL("http://does/not/exist");
  const std::string kAlternateURL0(
      "http://search.example/search#q={searchTerms}");
  const std::string kAlternateURL1("http://search.example/#q={searchTerms}");
  const std::string kSearchTermsReplacementKey("zekey");
  const std::string kImageURL("http://test.com/searchbyimage/upload");
  const std::string kImageURLPostParams(
      "image_content=content,image_url=http://test.com/test.png");
  const std::string kNewTabURL("http://search.example/newtab");

  TemplateURLService* service = TemplateURLServiceFactory::GetForProfile(
      browser()->profile());
  search_test_utils::WaitForTemplateURLServiceToLoad(service);
  const TemplateURL* default_search = service->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);
  EXPECT_NE(kKeyword, default_search->keyword());
  EXPECT_NE(kSearchURL, default_search->url());
  EXPECT_NE(kInstantURL, default_search->instant_url());
  EXPECT_FALSE(
    default_search->alternate_urls().size() == 2 &&
    default_search->alternate_urls()[0] == kAlternateURL0 &&
    default_search->alternate_urls()[1] == kAlternateURL1 &&
    default_search->search_terms_replacement_key() ==
        kSearchTermsReplacementKey &&
    default_search->image_url() == kImageURL &&
    default_search->image_url_post_params() == kImageURLPostParams &&
    default_search->new_tab_url() == kNewTabURL);

  // Override the default search provider using policies.
  PolicyMap policies;
  policies.Set(key::kDefaultSearchProviderEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(true), nullptr);
  policies.Set(key::kDefaultSearchProviderKeyword, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(kKeyword), nullptr);
  policies.Set(key::kDefaultSearchProviderSearchURL, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(kSearchURL), nullptr);
  policies.Set(key::kDefaultSearchProviderInstantURL, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(kInstantURL), nullptr);
  std::unique_ptr<base::ListValue> alternate_urls(new base::ListValue);
  alternate_urls->AppendString(kAlternateURL0);
  alternate_urls->AppendString(kAlternateURL1);
  policies.Set(key::kDefaultSearchProviderAlternateURLs, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::move(alternate_urls), nullptr);
  policies.Set(key::kDefaultSearchProviderSearchTermsReplacementKey,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(kSearchTermsReplacementKey),
               nullptr);
  policies.Set(key::kDefaultSearchProviderImageURL, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(kImageURL), nullptr);
  policies.Set(key::kDefaultSearchProviderImageURLPostParams,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(kImageURLPostParams), nullptr);
  policies.Set(key::kDefaultSearchProviderNewTabURL, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(kNewTabURL), nullptr);
  UpdateProviderPolicy(policies);
  default_search = service->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);
  EXPECT_EQ(kKeyword, default_search->keyword());
  EXPECT_EQ(kSearchURL, default_search->url());
  EXPECT_EQ(kInstantURL, default_search->instant_url());
  EXPECT_EQ(2U, default_search->alternate_urls().size());
  EXPECT_EQ(kAlternateURL0, default_search->alternate_urls()[0]);
  EXPECT_EQ(kAlternateURL1, default_search->alternate_urls()[1]);
  EXPECT_EQ(kSearchTermsReplacementKey,
            default_search->search_terms_replacement_key());
  EXPECT_EQ(kImageURL, default_search->image_url());
  EXPECT_EQ(kImageURLPostParams, default_search->image_url_post_params());
  EXPECT_EQ(kNewTabURL, default_search->new_tab_url());

  // Verify that searching from the omnibox uses kSearchURL.
  chrome::FocusLocationBar(browser());
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  ui_test_utils::SendToOmniboxAndSubmit(location_bar, "stuff to search for");
  OmniboxEditModel* model = location_bar->GetOmniboxView()->model();
  EXPECT_TRUE(model->CurrentMatch(NULL).destination_url.is_valid());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL expected("http://search.example/search?q=stuff+to+search+for");
  EXPECT_EQ(expected, web_contents->GetURL());

  // Verify that searching from the omnibox can be disabled.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  policies.Set(key::kDefaultSearchProviderEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(false), nullptr);
  EXPECT_TRUE(service->GetDefaultSearchProvider());
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(service->GetDefaultSearchProvider());
  ui_test_utils::SendToOmniboxAndSubmit(location_bar, "should not work");
  // This means that submitting won't trigger any action.
  EXPECT_FALSE(model->CurrentMatch(NULL).destination_url.is_valid());
  EXPECT_EQ(GURL(url::kAboutBlankURL), web_contents->GetURL());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, SeparateProxyPoliciesMerging) {
  // Add an individual proxy policy value.
  PolicyMap policies;
  policies.Set(key::kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::MakeUnique<base::Value>(3), nullptr);
  UpdateProviderPolicy(policies);

  // It should be removed and replaced with a dictionary.
  PolicyMap expected;
  std::unique_ptr<base::DictionaryValue> expected_value(
      new base::DictionaryValue);
  expected_value->SetInteger(key::kProxyServerMode, 3);
  expected.Set(key::kProxySettings, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, std::move(expected_value), nullptr);

  // Check both the browser and the profile.
  const PolicyMap& actual_from_browser =
      g_browser_process->browser_policy_connector()
          ->GetPolicyService()
          ->GetPolicies(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  EXPECT_TRUE(expected.Equals(actual_from_browser));
  const PolicyMap& actual_from_profile =
      ProfilePolicyConnectorFactory::GetForBrowserContext(browser()->profile())
          ->policy_service()
          ->GetPolicies(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  EXPECT_TRUE(expected.Equals(actual_from_profile));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, LegacySafeSearch) {
  static_assert(safe_search_util::YOUTUBE_RESTRICT_OFF      == 0 &&
                safe_search_util::YOUTUBE_RESTRICT_MODERATE == 1 &&
                safe_search_util::YOUTUBE_RESTRICT_STRICT   == 2 &&
                safe_search_util::YOUTUBE_RESTRICT_COUNT    == 3,
                "This test relies on mapping ints to enum values.");

  // Go over all combinations of (undefined, true, false) for the policies
  // ForceSafeSearch, ForceGoogleSafeSearch and ForceYouTubeSafetyMode as well
  // as (undefined, off, moderate, strict) for ForceYouTubeRestrict and make
  // sure the prefs are set as expected.
  const int num_restrict_modes = 1 + safe_search_util::YOUTUBE_RESTRICT_COUNT;
  for (int i = 0; i < 3 * 3 * 3 * num_restrict_modes; i++) {
    int val = i;
    int legacy_safe_search = val % 3; val /= 3;
    int google_safe_search = val % 3; val /= 3;
    int legacy_youtube     = val % 3; val /= 3;
    int youtube_restrict   = val % num_restrict_modes;

    // Override the default SafeSearch setting using policies.
    ApplySafeSearchPolicy(
        legacy_safe_search == 0
            ? nullptr
            : base::MakeUnique<base::Value>(legacy_safe_search == 1),
        google_safe_search == 0
            ? nullptr
            : base::MakeUnique<base::Value>(google_safe_search == 1),
        legacy_youtube == 0
            ? nullptr
            : base::MakeUnique<base::Value>(legacy_youtube == 1),
        youtube_restrict == 0
            ? nullptr  // subtracting 1 gives 0,1,2, see above
            : base::MakeUnique<base::Value>(youtube_restrict - 1));

    // The legacy ForceSafeSearch policy should only have an effect if none of
    // the other 3 policies are defined.
    bool legacy_safe_search_in_effect =
        google_safe_search == 0 && legacy_youtube == 0 &&
        youtube_restrict == 0 && legacy_safe_search != 0;
    bool legacy_safe_search_enabled =
        legacy_safe_search_in_effect && legacy_safe_search == 1;

    // Likewise, ForceYouTubeSafetyMode should only have an effect if
    // ForceYouTubeRestrict is not set.
    bool legacy_youtube_in_effect =
        youtube_restrict == 0 && legacy_youtube != 0;
    bool legacy_youtube_enabled =
        legacy_youtube_in_effect && legacy_youtube == 1;

    // Consistency check, can't have both legacy modes at the same time.
    EXPECT_FALSE(legacy_youtube_in_effect && legacy_safe_search_in_effect);

    // Google safe search can be triggered by the ForceGoogleSafeSearch policy
    // or the legacy safe search mode.
    PrefService* prefs = browser()->profile()->GetPrefs();
    EXPECT_EQ(google_safe_search != 0 || legacy_safe_search_in_effect,
              prefs->IsManagedPreference(prefs::kForceGoogleSafeSearch));
    EXPECT_EQ(google_safe_search == 1 || legacy_safe_search_enabled,
              prefs->GetBoolean(prefs::kForceGoogleSafeSearch));

    // YouTube restrict mode can be triggered by the ForceYouTubeRestrict policy
    // or any of the legacy modes.
    EXPECT_EQ(youtube_restrict != 0 || legacy_safe_search_in_effect ||
              legacy_youtube_in_effect,
              prefs->IsManagedPreference(prefs::kForceYouTubeRestrict));

    if (youtube_restrict != 0) {
      // The ForceYouTubeRestrict policy should map directly to the pref.
      EXPECT_EQ(youtube_restrict - 1,
          prefs->GetInteger(prefs::kForceYouTubeRestrict));
    } else {
      // The legacy modes should result in MODERATE strictness, if enabled.
      safe_search_util::YouTubeRestrictMode expected_mode =
          legacy_safe_search_enabled || legacy_youtube_enabled
            ? safe_search_util::YOUTUBE_RESTRICT_MODERATE
            : safe_search_util::YOUTUBE_RESTRICT_OFF;
      EXPECT_EQ(prefs->GetInteger(prefs::kForceYouTubeRestrict), expected_mode);
    }
  }
}

IN_PROC_BROWSER_TEST_F(PolicyTest, ForceGoogleSafeSearch) {
  // Makes the requests fail since all we want to check is that the redirection
  // is done properly.
  MakeRequestFail make_request_fail("google.com");

  // Verifies that requests to Google Search engine with the SafeSearch
  // enabled set the safe=active&ssui=on parameters at the end of the query.
  // First check that nothing happens.
  CheckSafeSearch(false);

  // Go over all combinations of (undefined, true, false) for the
  // ForceGoogleSafeSearch policy.
  for (int safe_search = 0; safe_search < 3; safe_search++) {
    // Override the Google safe search policy.
    ApplySafeSearchPolicy(nullptr,          // ForceSafeSearch
                          safe_search == 0  // ForceGoogleSafeSearch
                              ? nullptr
                              : base::MakeUnique<base::Value>(safe_search == 1),
                          nullptr,  // ForceYouTubeSafetyMode
                          nullptr   // ForceYouTubeRestrict
                          );
    // Verify that the safe search pref behaves the way we expect.
    PrefService* prefs = browser()->profile()->GetPrefs();
    EXPECT_EQ(safe_search != 0,
              prefs->IsManagedPreference(prefs::kForceGoogleSafeSearch));
    EXPECT_EQ(safe_search == 1,
              prefs->GetBoolean(prefs::kForceGoogleSafeSearch));

    // Verify that safe search actually works.
    CheckSafeSearch(safe_search == 1);
  }
}

IN_PROC_BROWSER_TEST_F(PolicyTest, Disable3DAPIs) {
  // This test assumes Gpu access.
  if (!content::GpuDataManager::GetInstance()->HardwareAccelerationEnabled())
    return;

  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  // WebGL is enabled by default.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsWebGLEnabled(contents));
  // Disable with a policy.
  PolicyMap policies;
  policies.Set(key::kDisable3DAPIs, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::MakeUnique<base::Value>(true),
               nullptr);
  UpdateProviderPolicy(policies);
  // Crash and reload the tab to get a new renderer.
  content::CrashTab(contents);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_RELOAD));
  EXPECT_FALSE(IsWebGLEnabled(contents));
  // Enable with a policy.
  policies.Set(key::kDisable3DAPIs, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::MakeUnique<base::Value>(false),
               nullptr);
  UpdateProviderPolicy(policies);
  content::CrashTab(contents);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_RELOAD));
  EXPECT_TRUE(IsWebGLEnabled(contents));
}

namespace {

// The following helpers retrieve whether https:// URL stripping is
// enabled for PAC scripts. It needs to run on the IO thread.
void GetPacHttpsUrlStrippingEnabledOnIOThread(IOThread* io_thread,
                                              bool* enabled) {
  *enabled = io_thread->PacHttpsUrlStrippingEnabled();
}

bool GetPacHttpsUrlStrippingEnabled() {
  bool enabled;
  base::RunLoop loop;
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&GetPacHttpsUrlStrippingEnabledOnIOThread,
                     g_browser_process->io_thread(),
                     base::Unretained(&enabled)),
      loop.QuitClosure());
  loop.Run();
  return enabled;
}

}  // namespace

// Verifies that stripping of https:// URLs before sending to PAC scripts can
// be disabled via a policy. Also verifies that stripping is enabled by
// default.
IN_PROC_BROWSER_TEST_F(PolicyTest, DisablePacHttpsUrlStripping) {
  // Stripping is enabled by default.
  EXPECT_TRUE(GetPacHttpsUrlStrippingEnabled());

  // Disable it via a policy.
  PolicyMap policies;
  policies.Set(key::kPacHttpsUrlStrippingEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::WrapUnique(new base::Value(false)), nullptr);
  UpdateProviderPolicy(policies);
  content::RunAllPendingInMessageLoop();

  // It should now reflect as disabled.
  EXPECT_FALSE(GetPacHttpsUrlStrippingEnabled());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DeveloperToolsDisabled) {
  // Verifies that access to the developer tools can be disabled.

  // Open devtools.
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_DEV_TOOLS));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DevToolsWindow* devtools_window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);
  EXPECT_TRUE(devtools_window);

  // Disable devtools via policy.
  PolicyMap policies;
  policies.Set(key::kDeveloperToolsDisabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(true), nullptr);
  content::WindowedNotificationObserver close_observer(
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<content::WebContents>(
          DevToolsWindowTesting::Get(devtools_window)->main_web_contents()));
  UpdateProviderPolicy(policies);
  // wait for devtools close
  close_observer.Wait();
  // The existing devtools window should have closed.
  EXPECT_FALSE(DevToolsWindow::GetInstanceForInspectedWebContents(contents));
  // And it's not possible to open it again.
  EXPECT_FALSE(chrome::ExecuteCommand(browser(), IDC_DEV_TOOLS));
  EXPECT_FALSE(DevToolsWindow::GetInstanceForInspectedWebContents(contents));
}

namespace {

// Utility for waiting until the dev-mode controls are visible/hidden
// Uses a MutationObserver on the attributes of the DOM element.
void WaitForExtensionsDevModeControlsVisibility(
    content::WebContents* contents,
    const char* dev_controls_accessor_js,
    const char* dev_controls_visibility_check_js,
    bool expected_visible) {
  bool done = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      contents,
      base::StringPrintf(
          "var screenElement = %s;"
          "function SendReplyIfAsExpected() {"
          "  var is_visible = %s;"
          "  if (is_visible != %s)"
          "    return false;"
          "  observer.disconnect();"
          "  domAutomationController.send(true);"
          "  return true;"
          "}"
          "var observer = new MutationObserver(SendReplyIfAsExpected);"
          "if (!SendReplyIfAsExpected()) {"
          "  var options = { 'attributes': true };"
          "  observer.observe(screenElement, options);"
          "}",
          dev_controls_accessor_js,
          dev_controls_visibility_check_js,
          (expected_visible ? "true" : "false")),
      &done));
}

}  // namespace

IN_PROC_BROWSER_TEST_F(PolicyTest, DeveloperToolsDisabledExtensionsDevMode) {
  // Verifies that when DeveloperToolsDisabled policy is set, the "dev mode"
  // in chrome://extensions-frame is actively turned off and the checkbox
  // is disabled.
  // Note: We don't test the indicator as it is tested in the policy pref test
  // for kDeveloperToolsDisabled.

  // This test currently depends on the following assumptions about the webui:
  // (1) The ID of the checkbox to toggle dev mode
  const char toggle_dev_mode_accessor_js[] =
      "document.getElementById('toggle-dev-on')";
  // (2) The ID of the dev controls containing element
  const char dev_controls_accessor_js[] =
      "document.getElementById('dev-controls')";
  // (3) the fact that dev-controls is displayed/hidden using its height attr
  const char dev_controls_visibility_check_js[] =
      "parseFloat(document.getElementById('dev-controls').style.height) > 0";

  // Navigate to the extensions frame and enabled "Developer mode"
  ui_test_utils::NavigateToURL(browser(),
                               GURL(chrome::kChromeUIExtensionsFrameURL));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScript(
      contents, base::StringPrintf("domAutomationController.send(%s.click());",
                                   toggle_dev_mode_accessor_js)));

  WaitForExtensionsDevModeControlsVisibility(contents,
                                             dev_controls_accessor_js,
                                             dev_controls_visibility_check_js,
                                             true);

  // Disable devtools via policy.
  PolicyMap policies;
  policies.Set(key::kDeveloperToolsDisabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(true), nullptr);
  UpdateProviderPolicy(policies);

  // Expect devcontrols to be hidden now...
  WaitForExtensionsDevModeControlsVisibility(contents,
                                             dev_controls_accessor_js,
                                             dev_controls_visibility_check_js,
                                             false);

  // ... and checkbox is not disabled
  bool is_toggle_dev_mode_checkbox_enabled = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      contents,
      base::StringPrintf(
          "domAutomationController.send(!%s.hasAttribute('disabled'))",
          toggle_dev_mode_accessor_js),
      &is_toggle_dev_mode_checkbox_enabled));
  EXPECT_FALSE(is_toggle_dev_mode_checkbox_enabled);
}

// TODO(samarth): remove along with rest of NTP4 code.
IN_PROC_BROWSER_TEST_F(PolicyTest, DISABLED_WebStoreIconHidden) {
  // Verifies that the web store icons can be hidden from the new tab page.

  // Open new tab page and look for the web store icons.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  content::WebContents* contents =
    browser()->tab_strip_model()->GetActiveWebContents();

#if !defined(OS_CHROMEOS)
  // Look for web store's app ID in the apps page.
  EXPECT_TRUE(ContainsVisibleElement(contents,
                                     "ahfgeienlihckogmohjhadlkjgocpleb"));
#endif

  // The next NTP has no footer.
  if (ContainsVisibleElement(contents, "footer"))
    EXPECT_TRUE(ContainsVisibleElement(contents, "chrome-web-store-link"));

  // Turn off the web store icons.
  PolicyMap policies;
  policies.Set(key::kHideWebStoreIcon, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::WrapUnique(new base::Value(true)), nullptr);
  UpdateProviderPolicy(policies);

  // The web store icons should now be hidden.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_FALSE(ContainsVisibleElement(contents,
                                      "ahfgeienlihckogmohjhadlkjgocpleb"));
  EXPECT_FALSE(ContainsVisibleElement(contents, "chrome-web-store-link"));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DownloadDirectory) {
  // Verifies that the download directory can be forced by policy.

  // Set the initial download directory.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::ScopedTempDir initial_dir;
  ASSERT_TRUE(initial_dir.CreateUniqueTempDir());
  browser()->profile()->GetPrefs()->SetFilePath(
      prefs::kDownloadDefaultDirectory, initial_dir.GetPath());
  // Don't prompt for the download location during this test.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPromptForDownload, false);

  // Verify that downloads end up on the default directory.
  base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  DownloadAndVerifyFile(browser(), initial_dir.GetPath(), file);
  base::DieFileDie(initial_dir.GetPath().Append(file), false);

  // Override the download directory with the policy and verify a download.
  base::ScopedTempDir forced_dir;
  ASSERT_TRUE(forced_dir.CreateUniqueTempDir());
  PolicyMap policies;
  policies.Set(key::kDownloadDirectory, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(forced_dir.GetPath().value()),
               nullptr);
  UpdateProviderPolicy(policies);
  DownloadAndVerifyFile(browser(), forced_dir.GetPath(), file);
  // Verify that the first download location wasn't affected.
  EXPECT_FALSE(base::PathExists(initial_dir.GetPath().Append(file)));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, ExtensionInstallBlacklistSelective) {
  // Verifies that blacklisted extensions can't be installed.
  ExtensionService* service = extension_service();
  ASSERT_FALSE(service->GetExtensionById(kGoodCrxId, true));
  ASSERT_FALSE(service->GetExtensionById(kAdBlockCrxId, true));
  base::ListValue blacklist;
  blacklist.AppendString(kGoodCrxId);
  PolicyMap policies;
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               blacklist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);

  // "good.crx" is blacklisted.
  EXPECT_FALSE(InstallExtension(kGoodCrxName));
  EXPECT_FALSE(service->GetExtensionById(kGoodCrxId, true));

  // "adblock.crx" is not.
  const extensions::Extension* adblock = InstallExtension(kAdBlockCrxName);
  ASSERT_TRUE(adblock);
  EXPECT_EQ(kAdBlockCrxId, adblock->id());
  EXPECT_EQ(adblock,
            service->GetExtensionById(kAdBlockCrxId, true));
}

// Flaky on windows; http://crbug.com/307994.
#if defined(OS_WIN)
#define MAYBE_ExtensionInstallBlacklistWildcard DISABLED_ExtensionInstallBlacklistWildcard
#else
#define MAYBE_ExtensionInstallBlacklistWildcard ExtensionInstallBlacklistWildcard
#endif
IN_PROC_BROWSER_TEST_F(PolicyTest, MAYBE_ExtensionInstallBlacklistWildcard) {
  // Verify that a wildcard blacklist takes effect.
  EXPECT_TRUE(InstallExtension(kAdBlockCrxName));
  ExtensionService* service = extension_service();
  ASSERT_FALSE(service->GetExtensionById(kGoodCrxId, true));
  ASSERT_TRUE(service->GetExtensionById(kAdBlockCrxId, true));
  base::ListValue blacklist;
  blacklist.AppendString("*");
  PolicyMap policies;
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               blacklist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);

  // AdBlock was automatically removed.
  ASSERT_FALSE(service->GetExtensionById(kAdBlockCrxId, true));

  // And can't be installed again, nor can good.crx.
  EXPECT_FALSE(InstallExtension(kAdBlockCrxName));
  EXPECT_FALSE(service->GetExtensionById(kAdBlockCrxId, true));
  EXPECT_FALSE(InstallExtension(kGoodCrxName));
  EXPECT_FALSE(service->GetExtensionById(kGoodCrxId, true));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, ExtensionInstallBlacklistSharedModules) {
  // Verifies that shared_modules are not affected by the blacklist.

  const char kImporterId[] = "pchakhniekfaeoddkifplhnfbffomabh";
  const char kSharedModuleId[] = "nfgclafboonjbiafbllihiailjlhelpm";

  // Make sure that "import" and "export" are available to these extension IDs
  // by mocking the release channel.
  extensions::ScopedCurrentChannel channel(version_info::Channel::DEV);

  // Verify that the extensions are not installed initially.
  ExtensionService* service = extension_service();
  ASSERT_FALSE(service->GetExtensionById(kImporterId, true));
  ASSERT_FALSE(service->GetExtensionById(kSharedModuleId, true));

  // Mock the webstore update URL. This is where the shared module extension
  // will be installed from.
  base::FilePath update_xml_path = base::FilePath(kTestExtensionsDir)
                                       .AppendASCII("policy_shared_module")
                                       .AppendASCII("update.xml");
  GURL update_xml_url(
      URLRequestMockHTTPJob::GetMockUrl(update_xml_path.MaybeAsASCII()));
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAppsGalleryUpdateURL, update_xml_url.spec());
  ui_test_utils::NavigateToURL(browser(), update_xml_url);

  // Blacklist "*" but force-install the importer extension. The shared module
  // should be automatically installed too.
  base::ListValue blacklist;
  blacklist.AppendString("*");
  base::ListValue forcelist;
  forcelist.AppendString(
      base::StringPrintf("%s;%s", kImporterId, update_xml_url.spec().c_str()));
  PolicyMap policies;
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               blacklist.CreateDeepCopy(), nullptr);
  policies.Set(key::kExtensionInstallForcelist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               forcelist.CreateDeepCopy(), nullptr);

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  extensions::TestExtensionRegistryObserver observe_importer(
      registry, kImporterId);
  extensions::TestExtensionRegistryObserver observe_shared_module(
      registry, kSharedModuleId);
  UpdateProviderPolicy(policies);
  observe_importer.WaitForExtensionLoaded();
  observe_shared_module.WaitForExtensionLoaded();

  // Verify that both extensions got installed.
  const extensions::Extension* importer =
      service->GetExtensionById(kImporterId, true);
  ASSERT_TRUE(importer);
  EXPECT_EQ(kImporterId, importer->id());
  const extensions::Extension* shared_module =
      service->GetExtensionById(kSharedModuleId, true);
  ASSERT_TRUE(shared_module);
  EXPECT_EQ(kSharedModuleId, shared_module->id());
  EXPECT_TRUE(shared_module->is_shared_module());

  // Verify the dependency.
  std::unique_ptr<extensions::ExtensionSet> set =
      service->shared_module_service()->GetDependentExtensions(shared_module);
  ASSERT_TRUE(set);
  EXPECT_EQ(1u, set->size());
  EXPECT_TRUE(set->Contains(importer->id()));

  std::vector<extensions::SharedModuleInfo::ImportInfo> imports =
      extensions::SharedModuleInfo::GetImports(importer);
  ASSERT_EQ(1u, imports.size());
  EXPECT_EQ(kSharedModuleId, imports[0].extension_id);
}

IN_PROC_BROWSER_TEST_F(PolicyTest, ExtensionInstallWhitelist) {
  // Verifies that the whitelist can open exceptions to the blacklist.
  ExtensionService* service = extension_service();
  ASSERT_FALSE(service->GetExtensionById(kGoodCrxId, true));
  ASSERT_FALSE(service->GetExtensionById(kAdBlockCrxId, true));
  base::ListValue blacklist;
  blacklist.AppendString("*");
  base::ListValue whitelist;
  whitelist.AppendString(kGoodCrxId);
  PolicyMap policies;
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               blacklist.CreateDeepCopy(), nullptr);
  policies.Set(key::kExtensionInstallWhitelist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               whitelist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);
  // "adblock.crx" is blacklisted.
  EXPECT_FALSE(InstallExtension(kAdBlockCrxName));
  EXPECT_FALSE(service->GetExtensionById(kAdBlockCrxId, true));
  // "good.crx" has a whitelist exception.
  const extensions::Extension* good = InstallExtension(kGoodCrxName);
  ASSERT_TRUE(good);
  EXPECT_EQ(kGoodCrxId, good->id());
  EXPECT_EQ(good, service->GetExtensionById(kGoodCrxId, true));
  // The user can also remove this extension.
  UninstallExtension(kGoodCrxId, true);
}

IN_PROC_BROWSER_TEST_F(PolicyTest, ExtensionInstallForcelist) {
  // Verifies that extensions that are force-installed by policies are
  // installed and can't be uninstalled.
  ExtensionService* service = extension_service();
  ASSERT_FALSE(service->GetExtensionById(kGoodCrxId, true));

  // Extensions that are force-installed come from an update URL, which defaults
  // to the webstore. Use a mock URL for this test with an update manifest
  // that includes "good_v1.crx".
  base::FilePath path =
      base::FilePath(kTestExtensionsDir).Append(kGoodV1CrxManifestName);
  GURL url(URLRequestMockHTTPJob::GetMockUrl(path.MaybeAsASCII()));

  // Setting the forcelist extension should install "good_v1.crx".
  base::ListValue forcelist;
  forcelist.AppendString(
      base::StringPrintf("%s;%s", kGoodCrxId, url.spec().c_str()));
  PolicyMap policies;
  policies.Set(key::kExtensionInstallForcelist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               forcelist.CreateDeepCopy(), nullptr);
  extensions::TestExtensionRegistryObserver observer(
      extensions::ExtensionRegistry::Get(browser()->profile()));
  UpdateProviderPolicy(policies);
  observer.WaitForExtensionWillBeInstalled();
  // Note: Cannot check that the notification details match the expected
  // exception, since the details object has already been freed prior to
  // the completion of observer.WaitForExtensionWillBeInstalled().

  EXPECT_TRUE(service->GetExtensionById(kGoodCrxId, true));

  // The user is not allowed to uninstall force-installed extensions.
  UninstallExtension(kGoodCrxId, false);

  scoped_refptr<extensions::UnpackedInstaller> installer =
      extensions::UnpackedInstaller::Create(extension_service());

  // The user is not allowed to load an unpacked extension with the
  // same ID as a force-installed extension.
  base::FilePath good_extension_path(ui_test_utils::GetTestFilePath(
      base::FilePath(kTestExtensionsDir), base::FilePath(kGoodUnpackedExt)));
  content::WindowedNotificationObserver extension_load_error_observer(
      extensions::NOTIFICATION_EXTENSION_LOAD_ERROR,
      content::NotificationService::AllSources());
  installer->Load(good_extension_path);
  extension_load_error_observer.Wait();

  // Loading other unpacked extensions are not blocked.
  scoped_refptr<const extensions::Extension> extension =
      LoadUnpackedExtension(kAppUnpackedExt);
  ASSERT_TRUE(extension);

  const std::string old_version_number =
      service->GetExtensionById(kGoodCrxId, true)->version()->GetString();

  base::FilePath test_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_path));

  TestRequestInterceptor interceptor(
      "update.extension",
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));
  interceptor.PushJobCallback(
      TestRequestInterceptor::FileJob(
          test_path.Append(kTestExtensionsDir).Append(kGood2CrxManifestName)));

  // Updating the force-installed extension.
  extensions::ExtensionUpdater* updater = service->updater();
  extensions::ExtensionUpdater::CheckParams params;
  params.install_immediately = true;
  extensions::TestExtensionRegistryObserver update_observer(
      extensions::ExtensionRegistry::Get(browser()->profile()));
  updater->CheckNow(params);
  update_observer.WaitForExtensionWillBeInstalled();

  const base::Version* new_version =
      service->GetExtensionById(kGoodCrxId, true)->version();
  ASSERT_TRUE(new_version->IsValid());
  base::Version old_version(old_version_number);
  ASSERT_TRUE(old_version.IsValid());

  EXPECT_EQ(1, new_version->CompareTo(old_version));

  EXPECT_EQ(0u, interceptor.GetPendingSize());

  // Wait until any background pages belonging to force-installed extensions
  // have been loaded.
  extensions::ProcessManager* manager =
      extensions::ProcessManager::Get(browser()->profile());
  extensions::ProcessManager::FrameSet all_frames = manager->GetAllFrames();
  for (extensions::ProcessManager::FrameSet::const_iterator iter =
           all_frames.begin();
       iter != all_frames.end();) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(*iter);
    ASSERT_TRUE(web_contents);
    if (!web_contents->IsLoading()) {
      ++iter;
    } else {
      WebContentsLoadedOrDestroyedWatcher(web_contents).Wait();

      // Test activity may have modified the set of extension processes during
      // message processing, so re-start the iteration to catch added/removed
      // processes.
      all_frames = manager->GetAllFrames();
      iter = all_frames.begin();
    }
  }

  // Test policy-installed extensions are reloaded when killed.
  BackgroundContentsService::
      SetRestartDelayForForceInstalledAppsAndExtensionsForTesting(0);
  content::WindowedNotificationObserver extension_crashed_observer(
      extensions::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
      content::NotificationService::AllSources());
  extensions::TestExtensionRegistryObserver extension_loaded_observer(
      extensions::ExtensionRegistry::Get(browser()->profile()), kGoodCrxId);
  extensions::ExtensionHost* extension_host =
      extensions::ProcessManager::Get(browser()->profile())
          ->GetBackgroundHostForExtension(kGoodCrxId);
  extension_host->render_process_host()->Shutdown(content::RESULT_CODE_KILLED,
                                                  false);
  extension_crashed_observer.Wait();
  extension_loaded_observer.WaitForExtensionLoaded();
}

IN_PROC_BROWSER_TEST_F(PolicyTest, ExtensionRecommendedInstallationMode) {
  // Verifies that extensions that are recommended-installed by policies are
  // installed, can be disabled but not uninstalled.
  ExtensionService* service = extension_service();
  ASSERT_FALSE(service->GetExtensionById(kGoodCrxId, true));

  base::FilePath path =
      base::FilePath(kTestExtensionsDir).Append(kGoodV1CrxManifestName);
  GURL url(URLRequestMockHTTPJob::GetMockUrl(path.MaybeAsASCII()));

  // Setting the forcelist extension should install "good_v1.crx".
  base::DictionaryValue dict_value;
  dict_value.SetString(std::string(kGoodCrxId) + "." +
                           extensions::schema_constants::kInstallationMode,
                       extensions::schema_constants::kNormalInstalled);
  dict_value.SetString(
      std::string(kGoodCrxId) + "." + extensions::schema_constants::kUpdateUrl,
      url.spec());
  PolicyMap policies;
  policies.Set(key::kExtensionSettings, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               dict_value.CreateDeepCopy(), nullptr);
  extensions::TestExtensionRegistryObserver observer(
      extensions::ExtensionRegistry::Get(browser()->profile()));
  UpdateProviderPolicy(policies);
  observer.WaitForExtensionWillBeInstalled();

  EXPECT_TRUE(service->GetExtensionById(kGoodCrxId, true));

  // The user is not allowed to uninstall recommended-installed extensions.
  UninstallExtension(kGoodCrxId, false);

  // Explictly re-enables the extension.
  service->EnableExtension(kGoodCrxId);

  // But the user is allowed to disable them.
  EXPECT_TRUE(service->IsExtensionEnabled(kGoodCrxId));
  DisableExtension(kGoodCrxId);
  EXPECT_FALSE(service->IsExtensionEnabled(kGoodCrxId));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, ExtensionAllowedTypes) {
  // Verifies that extensions are blocked if policy specifies an allowed types
  // list and the extension's type is not on that list.
  ExtensionService* service = extension_service();
  ASSERT_FALSE(service->GetExtensionById(kGoodCrxId, true));
  ASSERT_FALSE(service->GetExtensionById(kHostedAppCrxId, true));

  base::ListValue allowed_types;
  allowed_types.AppendString("hosted_app");
  PolicyMap policies;
  policies.Set(key::kExtensionAllowedTypes, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               allowed_types.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);

  // "good.crx" is blocked.
  EXPECT_FALSE(InstallExtension(kGoodCrxName));
  EXPECT_FALSE(service->GetExtensionById(kGoodCrxId, true));

  // "hosted_app.crx" is of a whitelisted type.
  const extensions::Extension* hosted_app = InstallExtension(kHostedAppCrxName);
  ASSERT_TRUE(hosted_app);
  EXPECT_EQ(kHostedAppCrxId, hosted_app->id());
  EXPECT_EQ(hosted_app, service->GetExtensionById(kHostedAppCrxId, true));

  // The user can remove the extension.
  UninstallExtension(kHostedAppCrxId, true);
}

// Checks that a click on an extension CRX download triggers the extension
// installation prompt without further user interaction when the source is
// whitelisted by policy.
// Flaky on windows; http://crbug.com/295729 .
#if defined(OS_WIN)
#define MAYBE_ExtensionInstallSources DISABLED_ExtensionInstallSources
#else
#define MAYBE_ExtensionInstallSources ExtensionInstallSources
#endif
IN_PROC_BROWSER_TEST_F(PolicyTest, MAYBE_ExtensionInstallSources) {
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);

  const GURL install_source_url(
      URLRequestMockHTTPJob::GetMockUrl("extensions/*"));
  const GURL referrer_url(URLRequestMockHTTPJob::GetMockUrl("policy/*"));

  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::ScopedTempDir download_directory;
  ASSERT_TRUE(download_directory.CreateUniqueTempDir());
  DownloadPrefs* download_prefs =
      DownloadPrefs::FromBrowserContext(browser()->profile());
  download_prefs->SetDownloadPath(download_directory.GetPath());

  const GURL download_page_url(URLRequestMockHTTPJob::GetMockUrl(
      "policy/extension_install_sources_test.html"));
  ui_test_utils::NavigateToURL(browser(), download_page_url);

  // As long as the policy is not present, extensions are considered dangerous.
  content::DownloadTestObserverTerminal download_observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()), 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_DENY);
  PerformClick(0, 0);
  download_observer.WaitForFinished();

  // Install the policy and trigger another download.
  base::ListValue install_sources;
  install_sources.AppendString(install_source_url.spec());
  install_sources.AppendString(referrer_url.spec());
  PolicyMap policies;
  policies.Set(key::kExtensionInstallSources, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               install_sources.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);

  extensions::TestExtensionRegistryObserver observer(
      extensions::ExtensionRegistry::Get(browser()->profile()));
  PerformClick(1, 0);
  observer.WaitForExtensionWillBeInstalled();
  // Note: Cannot check that the notification details match the expected
  // exception, since the details object has already been freed prior to
  // the completion of observer.WaitForExtensionWillBeInstalled().

  // The first extension shouldn't be present, the second should be there.
  EXPECT_FALSE(extension_service()->GetExtensionById(kGoodCrxId, true));
  EXPECT_TRUE(extension_service()->GetExtensionById(kAdBlockCrxId, false));
}

// Verifies that extensions with version older than the minimum version required
// by policy will get disabled, and will be auto-updated and/or re-enabled upon
// policy changes as well as regular auto-updater scheduled updates.
IN_PROC_BROWSER_TEST_F(PolicyTest, ExtensionMinimumVersionRequired) {
  ExtensionService* service = extension_service();
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(browser()->profile());

  // Explicitly stop the timer to avoid all scheduled extension auto-updates.
  service->updater()->StopTimerForTesting();

  // Setup interceptor for extension updates.
  base::FilePath test_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_path));
  TestRequestInterceptor interceptor(
      "update.extension",
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));
  interceptor.PushJobCallback(TestRequestInterceptor::BadRequestJob());
  interceptor.PushJobCallback(TestRequestInterceptor::FileJob(
      test_path.Append(kTestExtensionsDir).Append(kGood2CrxManifestName)));

  // Install the extension.
  EXPECT_TRUE(InstallExtension(kGoodV1CrxName));
  EXPECT_TRUE(registry->enabled_extensions().Contains(kGoodCrxId));

  // Update policy to set a minimum version of 1.0.0.0, the extension (with
  // version 1.0.0.0) should still be enabled.
  {
    extensions::ExtensionManagementPolicyUpdater management_policy(&provider_);
    management_policy.SetMinimumVersionRequired(kGoodCrxId, "1.0.0.0");
  }
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(registry->enabled_extensions().Contains(kGoodCrxId));

  // Update policy to set a minimum version of 1.0.0.1, the extension (with
  // version 1.0.0.0) should now be disabled.
  EXPECT_EQ(2u, interceptor.GetPendingSize());
  base::RunLoop service_request_run_loop;
  interceptor.AddRequestServicedCallback(
      service_request_run_loop.QuitClosure());
  {
    extensions::ExtensionManagementPolicyUpdater management_policy(&provider_);
    management_policy.SetMinimumVersionRequired(kGoodCrxId, "1.0.0.1");
  }
  service_request_run_loop.Run();
  EXPECT_EQ(1u, interceptor.GetPendingSize());

  EXPECT_TRUE(registry->disabled_extensions().Contains(kGoodCrxId));
  EXPECT_EQ(extensions::Extension::DISABLE_UPDATE_REQUIRED_BY_POLICY,
            extension_prefs->GetDisableReasons(kGoodCrxId));

  // Provide a new version (1.0.0.1) which is expected to be auto updated to
  // via the update URL in the manifest of the older version.
  EXPECT_EQ(1u, interceptor.GetPendingSize());
  {
    extensions::TestExtensionRegistryObserver update_observer(
        extensions::ExtensionRegistry::Get(browser()->profile()));
    service->updater()->CheckSoon();
    update_observer.WaitForExtensionWillBeInstalled();
  }
  EXPECT_EQ(0u, interceptor.GetPendingSize());

  // The extension should be auto-updated to newer version and re-enabled.
  EXPECT_EQ("1.0.0.1",
            service->GetInstalledExtension(kGoodCrxId)->version()->GetString());
  EXPECT_TRUE(registry->enabled_extensions().Contains(kGoodCrxId));
}

// Similar to ExtensionMinimumVersionRequired test, but with different settings
// and orders.
IN_PROC_BROWSER_TEST_F(PolicyTest, ExtensionMinimumVersionRequiredAlt) {
  ExtensionService* service = extension_service();
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(browser()->profile());

  // Explicitly stop the timer to avoid all scheduled extension auto-updates.
  service->updater()->StopTimerForTesting();

  // Setup interceptor for extension updates.
  base::FilePath test_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_path));
  TestRequestInterceptor interceptor(
      "update.extension",
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));
  interceptor.PushJobCallback(TestRequestInterceptor::FileJob(
      test_path.Append(kTestExtensionsDir).Append(kGood2CrxManifestName)));

  // Set the policy to require an even higher minimum version this time.
  {
    extensions::ExtensionManagementPolicyUpdater management_policy(&provider_);
    management_policy.SetMinimumVersionRequired(kGoodCrxId, "1.0.0.2");
  }
  base::RunLoop().RunUntilIdle();

  // Install the 1.0.0.0 version, it should be installed but disabled.
  EXPECT_TRUE(InstallExtension(kGoodV1CrxName));
  EXPECT_TRUE(registry->disabled_extensions().Contains(kGoodCrxId));
  EXPECT_EQ(extensions::Extension::DISABLE_UPDATE_REQUIRED_BY_POLICY,
            extension_prefs->GetDisableReasons(kGoodCrxId));
  EXPECT_EQ("1.0.0.0",
            service->GetInstalledExtension(kGoodCrxId)->version()->GetString());

  // An extension management policy update should trigger an update as well.
  EXPECT_EQ(1u, interceptor.GetPendingSize());
  {
    extensions::TestExtensionRegistryObserver update_observer(
        extensions::ExtensionRegistry::Get(browser()->profile()));
    {
      // Set a higher minimum version, just intend to trigger a policy update.
      extensions::ExtensionManagementPolicyUpdater management_policy(
          &provider_);
      management_policy.SetMinimumVersionRequired(kGoodCrxId, "1.0.0.3");
    }
    base::RunLoop().RunUntilIdle();
    update_observer.WaitForExtensionWillBeInstalled();
  }
  EXPECT_EQ(0u, interceptor.GetPendingSize());

  // It should be updated to 1.0.0.1 but remain disabled.
  EXPECT_EQ("1.0.0.1",
            service->GetInstalledExtension(kGoodCrxId)->version()->GetString());
  EXPECT_TRUE(registry->disabled_extensions().Contains(kGoodCrxId));
  EXPECT_EQ(extensions::Extension::DISABLE_UPDATE_REQUIRED_BY_POLICY,
            extension_prefs->GetDisableReasons(kGoodCrxId));

  // Remove the minimum version requirement. The extension should be re-enabled.
  {
    extensions::ExtensionManagementPolicyUpdater management_policy(&provider_);
    management_policy.UnsetMinimumVersionRequired(kGoodCrxId);
  }
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(registry->enabled_extensions().Contains(kGoodCrxId));
  EXPECT_FALSE(extension_prefs->HasDisableReason(
      kGoodCrxId, extensions::Extension::DISABLE_UPDATE_REQUIRED_BY_POLICY));
}

// Verifies that a force-installed extension which does not meet a subsequently
// set minimum version requirement is handled well.
IN_PROC_BROWSER_TEST_F(PolicyTest, ExtensionMinimumVersionForceInstalled) {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(browser()->profile());

  // Prepare the update URL for force installing.
  const base::FilePath path =
      base::FilePath(kTestExtensionsDir).Append(kGoodV1CrxManifestName);
  const GURL url(URLRequestMockHTTPJob::GetMockUrl(path.MaybeAsASCII()));

  // Set policy to force-install the extension, it should be installed and
  // enabled.
  extensions::TestExtensionRegistryObserver install_observer(
      extensions::ExtensionRegistry::Get(browser()->profile()));
  EXPECT_FALSE(registry->enabled_extensions().Contains(kGoodCrxId));
  {
    extensions::ExtensionManagementPolicyUpdater management_policy(&provider_);
    management_policy.SetIndividualExtensionAutoInstalled(kGoodCrxId,
                                                          url.spec(), true);
  }
  base::RunLoop().RunUntilIdle();
  install_observer.WaitForExtensionWillBeInstalled();

  EXPECT_TRUE(registry->enabled_extensions().Contains(kGoodCrxId));

  // Set policy a minimum version of "1.0.0.1", the extension now should be
  // disabled.
  {
    extensions::ExtensionManagementPolicyUpdater management_policy(&provider_);
    management_policy.SetMinimumVersionRequired(kGoodCrxId, "1.0.0.1");
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(registry->enabled_extensions().Contains(kGoodCrxId));
  EXPECT_TRUE(registry->disabled_extensions().Contains(kGoodCrxId));
  EXPECT_EQ(extensions::Extension::DISABLE_UPDATE_REQUIRED_BY_POLICY,
            extension_prefs->GetDisableReasons(kGoodCrxId));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, HomepageLocation) {
  // Verifies that the homepage can be configured with policies.
  // Set a default, and check that the home button navigates there.
  browser()->profile()->GetPrefs()->SetString(
      prefs::kHomePage, chrome::kChromeUIPolicyURL);
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kHomePageIsNewTabPage, false);
  EXPECT_EQ(GURL(chrome::kChromeUIPolicyURL),
            browser()->profile()->GetHomePage());
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(GURL(url::kAboutBlankURL), contents->GetURL());
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_HOME));
  EXPECT_EQ(GURL(chrome::kChromeUIPolicyURL), contents->GetURL());

  // Now override with policy.
  PolicyMap policies;
  policies.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(chrome::kChromeUICreditsURL),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_HOME));
  content::WaitForLoadStop(contents);
  EXPECT_EQ(GURL(chrome::kChromeUICreditsURL), contents->GetURL());

  policies.Set(key::kHomepageIsNewTabPage, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(true), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_HOME));
  content::WaitForLoadStop(contents);
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), contents->GetURL());
}

#if defined(OS_MACOSX) && defined(ADDRESS_SANITIZER)
// Flaky on ASAN on Mac. See https://crbug.com/674497.
#define MAYBE_IncognitoEnabled DISABLED_IncognitoEnabled
#else
#define MAYBE_IncognitoEnabled IncognitoEnabled
#endif
IN_PROC_BROWSER_TEST_F(PolicyTest, MAYBE_IncognitoEnabled) {
  // Verifies that incognito windows can't be opened when disabled by policy.

  const BrowserList* active_browser_list = BrowserList::GetInstance();

  // Disable incognito via policy and verify that incognito windows can't be
  // opened.
  EXPECT_EQ(1u, active_browser_list->size());
  EXPECT_FALSE(BrowserList::IsIncognitoSessionActive());
  PolicyMap policies;
  policies.Set(key::kIncognitoEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(chrome::ExecuteCommand(browser(), IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_EQ(1u, active_browser_list->size());
  EXPECT_FALSE(BrowserList::IsIncognitoSessionActive());

  // Enable via policy and verify that incognito windows can be opened.
  policies.Set(key::kIncognitoEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(true), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_EQ(2u, active_browser_list->size());
  EXPECT_TRUE(BrowserList::IsIncognitoSessionActive());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, Javascript) {
  // Verifies that Javascript can be disabled.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsJavascriptEnabled(contents));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS_CONSOLE));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS_DEVICES));

  // Disable Javascript via policy.
  PolicyMap policies;
  policies.Set(key::kJavascriptEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);
  // Reload the page.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  EXPECT_FALSE(IsJavascriptEnabled(contents));
  // Developer tools still work when javascript is disabled.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS_CONSOLE));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS_DEVICES));
  // Javascript is always enabled for the internal pages.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL));
  EXPECT_TRUE(IsJavascriptEnabled(contents));

  // The javascript content setting policy overrides the javascript policy.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  EXPECT_FALSE(IsJavascriptEnabled(contents));
  policies.Set(key::kDefaultJavaScriptSetting, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(CONTENT_SETTING_ALLOW), nullptr);
  UpdateProviderPolicy(policies);
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  EXPECT_TRUE(IsJavascriptEnabled(contents));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, NetworkPrediction) {
  PrefService* prefs = browser()->profile()->GetPrefs();

  // Enabled by default.
  EXPECT_TRUE(IsNetworkPredictionEnabled(prefs));

  // Disable by old, deprecated policy.
  PolicyMap policies;
  policies.Set(key::kDnsPrefetchingEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_FALSE(IsNetworkPredictionEnabled(prefs));

  // Enabled by new policy, this should override old one.
  policies.Set(key::kNetworkPredictionOptions, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(
                   chrome_browser_net::NETWORK_PREDICTION_ALWAYS),
               nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_TRUE(IsNetworkPredictionEnabled(prefs));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, SavingBrowserHistoryDisabled) {
  // Verifies that browsing history is not saved.
  PolicyMap policies;
  policies.Set(key::kSavingBrowserHistoryDisabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(true), nullptr);
  UpdateProviderPolicy(policies);
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("empty.html")));
  ui_test_utils::NavigateToURL(browser(), url);
  // Verify that the navigation wasn't saved in the history.
  ui_test_utils::HistoryEnumerator enumerator1(browser()->profile());
  EXPECT_EQ(0u, enumerator1.urls().size());

  // Now flip the policy and try again.
  policies.Set(key::kSavingBrowserHistoryDisabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);
  ui_test_utils::NavigateToURL(browser(), url);
  // Verify that the navigation was saved in the history.
  ui_test_utils::HistoryEnumerator enumerator2(browser()->profile());
  ASSERT_EQ(1u, enumerator2.urls().size());
  EXPECT_EQ(url, enumerator2.urls()[0]);
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DeletingBrowsingHistoryDisabled) {
  // Verifies that deleting the browsing history can be disabled.

  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kAllowDeletingBrowserHistory));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory));

  EXPECT_TRUE(prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistory));
  EXPECT_TRUE(prefs->GetBoolean(browsing_data::prefs::kDeleteDownloadHistory));
  EXPECT_TRUE(
      prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistoryBasic));

  PolicyMap policies;
  policies.Set(key::kAllowDeletingBrowserHistory, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(true), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kAllowDeletingBrowserHistory));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory));

  EXPECT_TRUE(prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistory));
  EXPECT_TRUE(prefs->GetBoolean(browsing_data::prefs::kDeleteDownloadHistory));
  EXPECT_TRUE(
      prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistoryBasic));

  policies.Set(key::kAllowDeletingBrowserHistory, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kAllowDeletingBrowserHistory));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory));

  EXPECT_FALSE(prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistory));
  EXPECT_FALSE(prefs->GetBoolean(browsing_data::prefs::kDeleteDownloadHistory));
  EXPECT_FALSE(
      prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistoryBasic));

  policies.Clear();
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kAllowDeletingBrowserHistory));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory));

  EXPECT_TRUE(prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistory));
  EXPECT_TRUE(prefs->GetBoolean(browsing_data::prefs::kDeleteDownloadHistory));
  EXPECT_TRUE(
      prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistoryBasic));
}

// TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
#if !defined(USE_AURA)
// http://crbug.com/241691 PolicyTest.TranslateEnabled is failing regularly.
IN_PROC_BROWSER_TEST_F(PolicyTest, DISABLED_TranslateEnabled) {
  // Verifies that translate can be forced enabled or disabled by policy.

  // Get the InfoBarService, and verify that there are no infobars on startup.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  InfoBarService* infobar_service = InfoBarService::FromWebContents(contents);
  ASSERT_TRUE(infobar_service);
  EXPECT_EQ(0u, infobar_service->infobar_count());

  // Force enable the translate feature.
  PolicyMap policies;
  policies.Set(key::kTranslateEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::WrapUnique(new base::Value(true)), nullptr);
  UpdateProviderPolicy(policies);
  // Instead of waiting for NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED, this test
  // waits for NOTIFICATION_TAB_LANGUAGE_DETERMINED because that's what the
  // TranslateManager observes. This allows checking that an infobar is NOT
  // shown below, without polling for infobars for some indeterminate amount
  // of time.
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(),
      base::FilePath(FILE_PATH_LITERAL("translate/fr_test.html")));
  content::WindowedNotificationObserver language_observer1(
      chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(browser(), url);
  language_observer1.Wait();

  // Verify the translation detected for this tab.
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(contents);
  ASSERT_TRUE(chrome_translate_client);
  translate::LanguageState& language_state =
      chrome_translate_client->GetLanguageState();
  EXPECT_EQ("fr", language_state.original_language());
  EXPECT_TRUE(language_state.page_needs_translation());
  EXPECT_FALSE(language_state.translation_pending());
  EXPECT_FALSE(language_state.translation_declined());
  EXPECT_FALSE(language_state.IsPageTranslated());

  // Verify that the translate infobar showed up.
  ASSERT_EQ(1u, infobar_service->infobar_count());
  infobars::InfoBar* infobar = infobar_service->infobar_at(0);
  translate::TranslateInfoBarDelegate* translate_infobar_delegate =
      infobar->delegate()->AsTranslateInfoBarDelegate();
  ASSERT_TRUE(translate_infobar_delegate);
  EXPECT_EQ(translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
            translate_infobar_delegate->translate_step());
  EXPECT_EQ("fr", translate_infobar_delegate->original_language_code());

  // Now force disable translate.
  infobar_service->RemoveInfoBar(infobar);
  EXPECT_EQ(0u, infobar_service->infobar_count());
  policies.Set(key::kTranslateEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::WrapUnique(new base::Value(false)), nullptr);
  UpdateProviderPolicy(policies);
  // Navigating to the same URL now doesn't trigger an infobar.
  content::WindowedNotificationObserver language_observer2(
      chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(browser(), url);
  language_observer2.Wait();
  EXPECT_EQ(0u, infobar_service->infobar_count());
}
#endif  // !defined(USE_AURA)

IN_PROC_BROWSER_TEST_F(PolicyTest, URLBlacklist) {
  // Checks that URLs can be blacklisted, and that exceptions can be made to
  // the blacklist.

  // Filter |kURLS| on IO thread, so that requests to those hosts end up
  // as URLRequestMockHTTPJobs.
  const char* kURLS[] = {
    "http://aaa.com/empty.html",
    "http://bbb.com/empty.html",
    "http://sub.bbb.com/empty.html",
    "http://bbb.com/policy/blank.html",
    "http://bbb.com./policy/blank.html",
  };
  {
    base::RunLoop loop;
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(RedirectHostsToTestData, kURLS, arraysize(kURLS)),
        loop.QuitClosure());
    loop.Run();
  }

  // Verify that "bbb.com" opens before applying the blacklist.
  CheckCanOpenURL(browser(), kURLS[1]);

  // Set a blacklist.
  base::ListValue blacklist;
  blacklist.AppendString("bbb.com");
  PolicyMap policies;
  policies.Set(key::kURLBlacklist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, blacklist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();
  // All bbb.com URLs are blocked, and "aaa.com" is still unblocked.
  CheckCanOpenURL(browser(), kURLS[0]);
  for (size_t i = 1; i < arraysize(kURLS); ++i)
    CheckURLIsBlocked(browser(), kURLS[i]);

  // Whitelist some sites of bbb.com.
  base::ListValue whitelist;
  whitelist.AppendString("sub.bbb.com");
  whitelist.AppendString("bbb.com/policy");
  policies.Set(key::kURLWhitelist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, whitelist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();
  CheckURLIsBlocked(browser(), kURLS[1]);
  CheckCanOpenURL(browser(), kURLS[2]);
  CheckCanOpenURL(browser(), kURLS[3]);
  CheckCanOpenURL(browser(), kURLS[4]);

  {
    base::RunLoop loop;
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(UndoRedirectHostsToTestData, kURLS, arraysize(kURLS)),
        loop.QuitClosure());
    loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(PolicyTest, URLBlacklistSubresources) {
  // Checks that an image with a blacklisted URL is loaded, but an iframe with a
  // blacklisted URL is not.

  GURL main_url =
      URLRequestMockHTTPJob::GetMockUrl("policy/blacklist-subresources.html");
  GURL image_url = URLRequestMockHTTPJob::GetMockUrl("policy/pixel.png");
  GURL subframe_url = URLRequestMockHTTPJob::GetMockUrl("policy/blank.html");

  // Set a blacklist containing the image and the iframe which are used by the
  // main document.
  base::ListValue blacklist;
  blacklist.AppendString(image_url.spec().c_str());
  blacklist.AppendString(subframe_url.spec().c_str());
  PolicyMap policies;
  policies.Set(key::kURLBlacklist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, blacklist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();

  std::string blacklisted_image_load_result;
  ui_test_utils::NavigateToURL(browser(), main_url);
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(imageLoadResult)",
      &blacklisted_image_load_result));
  EXPECT_EQ("success", blacklisted_image_load_result);

  std::string blacklisted_iframe_load_result;
  ui_test_utils::NavigateToURL(browser(), main_url);
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(iframeLoadResult)",
      &blacklisted_iframe_load_result));
  EXPECT_EQ("error", blacklisted_iframe_load_result);
}

#if defined(OS_MACOSX)
// http://crbug.com/339240
#define MAYBE_FileURLBlacklist DISABLED_FileURLBlacklist
#else
#define MAYBE_FileURLBlacklist FileURLBlacklist
#endif
IN_PROC_BROWSER_TEST_F(PolicyTest, MAYBE_FileURLBlacklist) {
  // Check that FileURLs can be blacklisted and DisabledSchemes works together
  // with URLblacklisting and URLwhitelisting.

  base::FilePath test_path;
  PathService::Get(chrome::DIR_TEST_DATA, &test_path);
  const std::string base_path = "file://" + test_path.AsUTF8Unsafe() +"/";
  const std::string folder_path = base_path + "apptest/";
  const std::string file_path1 = base_path + "title1.html";
  const std::string file_path2 = folder_path + "basic.html";

  CheckCanOpenURL(browser(), file_path1.c_str());
  CheckCanOpenURL(browser(), file_path2.c_str());

  // Set a blacklist for all the files.
  base::ListValue blacklist;
  blacklist.AppendString("file://*");
  PolicyMap policies;
  policies.Set(key::kURLBlacklist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, blacklist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();

  CheckURLIsBlocked(browser(), file_path1.c_str());
  CheckURLIsBlocked(browser(), file_path2.c_str());

  // Replace the URLblacklist with disabling the file scheme.
  blacklist.Remove(base::Value("file://*"), NULL);
  policies.Set(key::kURLBlacklist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, blacklist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();

  PrefService* prefs = browser()->profile()->GetPrefs();
  const base::ListValue* list_url = prefs->GetList(policy_prefs::kUrlBlacklist);
  EXPECT_EQ(list_url->Find(base::Value("file://*")), list_url->end());

  base::ListValue disabledscheme;
  disabledscheme.AppendString("file");
  policies.Set(key::kDisabledSchemes, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, disabledscheme.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();

  list_url = prefs->GetList(policy_prefs::kUrlBlacklist);
  EXPECT_NE(list_url->Find(base::Value("file://*")), list_url->end());

  // Whitelist one folder and blacklist an another just inside.
  base::ListValue whitelist;
  whitelist.AppendString(base_path);
  policies.Set(key::kURLWhitelist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, whitelist.CreateDeepCopy(), nullptr);
  blacklist.AppendString(folder_path);
  policies.Set(key::kURLBlacklist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, blacklist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();

  CheckCanOpenURL(browser(), file_path1.c_str());
  CheckURLIsBlocked(browser(), file_path2.c_str());
}

#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(PolicyTest, FullscreenAllowedBrowser) {
  PolicyMap policies;
  policies.Set(key::kFullscreenAllowed, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);

  BrowserWindow* browser_window = browser()->window();
  ASSERT_TRUE(browser_window);

  EXPECT_FALSE(browser_window->IsFullscreen());
  chrome::ToggleFullscreenMode(browser());
  EXPECT_FALSE(browser_window->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, FullscreenAllowedApp) {
  PolicyMap policies;
  policies.Set(key::kFullscreenAllowed, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);

  scoped_refptr<const extensions::Extension> extension =
      LoadUnpackedExtension(kUnpackedFullscreenAppName);
  ASSERT_TRUE(extension);

  // Launch an app that tries to open a fullscreen window.
  TestAddAppWindowObserver add_window_observer(
      extensions::AppWindowRegistry::Get(browser()->profile()));
  OpenApplication(AppLaunchParams(
      browser()->profile(), extension.get(), extensions::LAUNCH_CONTAINER_NONE,
      WindowOpenDisposition::NEW_WINDOW, extensions::SOURCE_TEST));
  extensions::AppWindow* window = add_window_observer.WaitForAppWindow();
  ASSERT_TRUE(window);

  // Verify that the window is not in fullscreen mode.
  EXPECT_FALSE(window->GetBaseWindow()->IsFullscreen());

  // For PlzNavigate case, we have to wait for the navigation to commit since
  // the JS object registration is delayed (see
  // AppWindowCreateFunction::RunAsync).
  content::WaitForLoadStop(window->web_contents());

  // Verify that the window cannot be toggled into fullscreen mode via apps
  // APIs.
  EXPECT_TRUE(content::ExecuteScript(
      window->web_contents(),
      "chrome.app.window.current().fullscreen();"));
  EXPECT_FALSE(window->GetBaseWindow()->IsFullscreen());

  // Verify that the window cannot be toggled into fullscreen mode from within
  // Chrome (e.g., using keyboard accelerators).
  window->Fullscreen();
  EXPECT_FALSE(window->GetBaseWindow()->IsFullscreen());
}
#endif

#if defined(OS_CHROMEOS)

// Flaky on MSan (crbug.com/476964) and regular Chrome OS (crbug.com/645769).
IN_PROC_BROWSER_TEST_F(PolicyTest, DISABLED_DisableScreenshotsFile) {
  int screenshot_count = CountScreenshots();

  // Make sure screenshots are counted correctly.
  TestScreenshotFile(true);
  ASSERT_EQ(CountScreenshots(), screenshot_count + 1);

  // Check if trying to take a screenshot fails when disabled by policy.
  TestScreenshotFile(false);
  ASSERT_EQ(CountScreenshots(), screenshot_count + 1);
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DisableAudioOutput) {
  // Set up the mock observer.
  chromeos::CrasAudioHandler* audio_handler = chromeos::CrasAudioHandler::Get();
  std::unique_ptr<TestAudioObserver> test_observer(new TestAudioObserver);
  audio_handler->AddAudioObserver(test_observer.get());

  bool prior_state = audio_handler->IsOutputMuted();
  // Make sure the audio is not muted and then toggle the policy and observe
  // if the output mute changed event is fired.
  audio_handler->SetOutputMute(false);
  EXPECT_FALSE(audio_handler->IsOutputMuted());
  EXPECT_EQ(1, test_observer->output_mute_changed_count());
  PolicyMap policies;
  policies.Set(key::kAudioOutputAllowed, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(audio_handler->IsOutputMuted());
  // This should not change the state now and should not trigger output mute
  // changed event.
  audio_handler->SetOutputMute(false);
  EXPECT_TRUE(audio_handler->IsOutputMuted());
  EXPECT_EQ(1, test_observer->output_mute_changed_count());

  // Toggle back and observe if the output mute changed event is fired.
  policies.Set(key::kAudioOutputAllowed, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(true), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(audio_handler->IsOutputMuted());
  EXPECT_EQ(1, test_observer->output_mute_changed_count());
  audio_handler->SetOutputMute(true);
  EXPECT_TRUE(audio_handler->IsOutputMuted());
  EXPECT_EQ(2, test_observer->output_mute_changed_count());
  // Revert the prior state.
  audio_handler->SetOutputMute(prior_state);
  audio_handler->RemoveAudioObserver(test_observer.get());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, PRE_SessionLengthLimit) {
  // Indicate that the session started 2 hours ago and no user activity has
  // occurred yet.
  g_browser_process->local_state()->SetInt64(
      prefs::kSessionStartTime,
      (base::TimeTicks::Now() - base::TimeDelta::FromHours(2))
          .ToInternalValue());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, SessionLengthLimit) {
  content::MockNotificationObserver observer;
  content::NotificationRegistrar registrar;
  registrar.Add(&observer,
                chrome::NOTIFICATION_APP_TERMINATING,
                content::NotificationService::AllSources());

  // Set the session length limit to 3 hours. Verify that the session is not
  // terminated.
  EXPECT_CALL(observer, Observe(chrome::NOTIFICATION_APP_TERMINATING, _, _))
      .Times(0);
  PolicyMap policies;
  policies.Set(key::kSessionLengthLimit, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(kThreeHoursInMs), nullptr);
  UpdateProviderPolicy(policies);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Decrease the session length limit to 1 hour. Verify that the session is
  // terminated immediately.
  EXPECT_CALL(observer, Observe(chrome::NOTIFICATION_APP_TERMINATING, _, _));
  policies.Set(key::kSessionLengthLimit, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(kOneHourInMs), nullptr);
  UpdateProviderPolicy(policies);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);
}

// Disabled, see http://crbug.com/554728.
IN_PROC_BROWSER_TEST_F(PolicyTest,
                       DISABLED_PRE_WaitForInitialUserActivityUnsatisfied) {
  // Indicate that the session started 2 hours ago and no user activity has
  // occurred yet.
  g_browser_process->local_state()->SetInt64(
      prefs::kSessionStartTime,
      (base::TimeTicks::Now() - base::TimeDelta::FromHours(2))
          .ToInternalValue());
}

// Disabled, see http://crbug.com/554728.
IN_PROC_BROWSER_TEST_F(PolicyTest,
                       DISABLED_WaitForInitialUserActivityUnsatisfied) {
  content::MockNotificationObserver observer;
  content::NotificationRegistrar registrar;
  registrar.Add(&observer,
                chrome::NOTIFICATION_APP_TERMINATING,
                content::NotificationService::AllSources());

  // Require initial user activity.
  PolicyMap policies;
  policies.Set(key::kWaitForInitialUserActivity, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::WrapUnique(new base::Value(true)), nullptr);
  UpdateProviderPolicy(policies);
  base::RunLoop().RunUntilIdle();

  // Set the session length limit to 1 hour. Verify that the session is not
  // terminated.
  EXPECT_CALL(observer, Observe(chrome::NOTIFICATION_APP_TERMINATING, _, _))
      .Times(0);
  policies.Set(key::kSessionLengthLimit, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::WrapUnique(new base::Value(kOneHourInMs)), nullptr);
  UpdateProviderPolicy(policies);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);
}

IN_PROC_BROWSER_TEST_F(PolicyTest,
                       PRE_WaitForInitialUserActivitySatisfied) {
  // Indicate that initial user activity in this session occurred 2 hours ago.
  g_browser_process->local_state()->SetInt64(
      prefs::kSessionStartTime,
      (base::TimeTicks::Now() - base::TimeDelta::FromHours(2))
          .ToInternalValue());
  g_browser_process->local_state()->SetBoolean(
      prefs::kSessionUserActivitySeen,
      true);
}

IN_PROC_BROWSER_TEST_F(PolicyTest,
                       WaitForInitialUserActivitySatisfied) {
  content::MockNotificationObserver observer;
  content::NotificationRegistrar registrar;
  registrar.Add(&observer,
                chrome::NOTIFICATION_APP_TERMINATING,
                content::NotificationService::AllSources());

  // Require initial user activity and set the session length limit to 3 hours.
  // Verify that the session is not terminated.
  EXPECT_CALL(observer, Observe(chrome::NOTIFICATION_APP_TERMINATING, _, _))
      .Times(0);
  PolicyMap policies;
  policies.Set(key::kWaitForInitialUserActivity, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(true), nullptr);
  policies.Set(key::kSessionLengthLimit, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(kThreeHoursInMs), nullptr);
  UpdateProviderPolicy(policies);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Decrease the session length limit to 1 hour. Verify that the session is
  // terminated immediately.
  EXPECT_CALL(observer, Observe(chrome::NOTIFICATION_APP_TERMINATING, _, _));
  policies.Set(key::kSessionLengthLimit, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(kOneHourInMs), nullptr);
  UpdateProviderPolicy(policies);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);
}

IN_PROC_BROWSER_TEST_F(PolicyTest, LargeCursorEnabled) {
  // Verifies that the large cursor accessibility feature can be controlled
  // through policy.
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();

  // Manually enable the large cursor.
  accessibility_manager->EnableLargeCursor(true);
  EXPECT_TRUE(accessibility_manager->IsLargeCursorEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kLargeCursorEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(accessibility_manager->IsLargeCursorEnabled());

  // Verify that the large cursor cannot be enabled manually anymore.
  accessibility_manager->EnableLargeCursor(true);
  EXPECT_FALSE(accessibility_manager->IsLargeCursorEnabled());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, SpokenFeedbackEnabled) {
  // Verifies that the spoken feedback accessibility feature can be controlled
  // through policy.
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();

  // Manually enable spoken feedback.
  accessibility_manager->EnableSpokenFeedback(true,
                                              ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(accessibility_manager->IsSpokenFeedbackEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kSpokenFeedbackEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(accessibility_manager->IsSpokenFeedbackEnabled());

  // Verify that spoken feedback cannot be enabled manually anymore.
  accessibility_manager->EnableSpokenFeedback(true,
                                              ash::A11Y_NOTIFICATION_NONE);
  EXPECT_FALSE(accessibility_manager->IsSpokenFeedbackEnabled());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, HighContrastEnabled) {
  // Verifies that the high contrast mode accessibility feature can be
  // controlled through policy.
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();

  // Manually enable high contrast mode.
  accessibility_manager->EnableHighContrast(true);
  EXPECT_TRUE(accessibility_manager->IsHighContrastEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kHighContrastEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(accessibility_manager->IsHighContrastEnabled());

  // Verify that high contrast mode cannot be enabled manually anymore.
  accessibility_manager->EnableHighContrast(true);
  EXPECT_FALSE(accessibility_manager->IsHighContrastEnabled());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, ScreenMagnifierTypeNone) {
  // Verifies that the screen magnifier can be disabled through policy.
  chromeos::MagnificationManager* magnification_manager =
      chromeos::MagnificationManager::Get();

  // Manually enable the full-screen magnifier.
  magnification_manager->SetMagnifierType(ash::MAGNIFIER_FULL);
  magnification_manager->SetMagnifierEnabled(true);
  EXPECT_EQ(ash::MAGNIFIER_FULL, magnification_manager->GetMagnifierType());
  EXPECT_TRUE(magnification_manager->IsMagnifierEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kScreenMagnifierType, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(0), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(magnification_manager->IsMagnifierEnabled());

  // Verify that the screen magnifier cannot be enabled manually anymore.
  magnification_manager->SetMagnifierEnabled(true);
  EXPECT_FALSE(magnification_manager->IsMagnifierEnabled());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, ScreenMagnifierTypeFull) {
  // Verifies that the full-screen magnifier can be enabled through policy.
  chromeos::MagnificationManager* magnification_manager =
      chromeos::MagnificationManager::Get();

  // Verify that the screen magnifier is initially disabled.
  EXPECT_FALSE(magnification_manager->IsMagnifierEnabled());

  // Verify that policy can enable the full-screen magnifier.
  PolicyMap policies;
  policies.Set(key::kScreenMagnifierType, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(ash::MAGNIFIER_FULL), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_EQ(ash::MAGNIFIER_FULL, magnification_manager->GetMagnifierType());
  EXPECT_TRUE(magnification_manager->IsMagnifierEnabled());

  // Verify that the screen magnifier cannot be disabled manually anymore.
  magnification_manager->SetMagnifierEnabled(false);
  EXPECT_TRUE(magnification_manager->IsMagnifierEnabled());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, AccessibilityVirtualKeyboardEnabled) {
  // Verifies that the on-screen keyboard accessibility feature can be
  // controlled through policy.
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();

  // Manually enable the on-screen keyboard.
  accessibility_manager->EnableVirtualKeyboard(true);
  EXPECT_TRUE(accessibility_manager->IsVirtualKeyboardEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kVirtualKeyboardEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(accessibility_manager->IsVirtualKeyboardEnabled());

  // Verify that the on-screen keyboard cannot be enabled manually anymore.
  accessibility_manager->EnableVirtualKeyboard(true);
  EXPECT_FALSE(accessibility_manager->IsVirtualKeyboardEnabled());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, VirtualKeyboardEnabled) {
  // Verify keyboard disabled by default.
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());
  // Verify keyboard can be toggled by default.
  keyboard::SetTouchKeyboardEnabled(true);
  EXPECT_TRUE(keyboard::IsKeyboardEnabled());
  keyboard::SetTouchKeyboardEnabled(false);
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());

  // Verify enabling the policy takes effect immediately and that that user
  // cannot disable the keyboard..
  PolicyMap policies;
  policies.Set(key::kTouchVirtualKeyboardEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(true), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(keyboard::IsKeyboardEnabled());
  keyboard::SetTouchKeyboardEnabled(false);
  EXPECT_TRUE(keyboard::IsKeyboardEnabled());

  // Verify that disabling the policy takes effect immediately and that the user
  // cannot enable the keyboard.
  policies.Set(key::kTouchVirtualKeyboardEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());
  keyboard::SetTouchKeyboardEnabled(true);
  EXPECT_FALSE(keyboard::IsKeyboardEnabled());
}

#endif

namespace {

static const char* kRestoredURLs[] = {
  "http://aaa.com/empty.html",
  "http://bbb.com/empty.html",
};

bool IsNonSwitchArgument(const base::CommandLine::StringType& s) {
  return s.empty() || s[0] != '-';
}

}  // namespace

// Similar to PolicyTest but allows setting policies before the browser is
// created. Each test parameter is a method that sets up the early policies
// and stores the expected startup URLs in |expected_urls_|.
class RestoreOnStartupPolicyTest
    : public PolicyTest,
      public testing::WithParamInterface<
          void (RestoreOnStartupPolicyTest::*)(void)> {
 public:
  RestoreOnStartupPolicyTest() {}
  virtual ~RestoreOnStartupPolicyTest() {}

#if defined(OS_CHROMEOS)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(nkostylev): Investigate if we can remove this switch.
    command_line->AppendSwitch(switches::kCreateBrowserOnStartupForTests);
    PolicyTest::SetUpCommandLine(command_line);
  }
#endif

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    // Set early policies now, before the browser is created.
    (this->*(GetParam()))();

    // Remove the non-switch arguments, so that session restore kicks in for
    // these tests.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    base::CommandLine::StringVector argv = command_line->argv();
    argv.erase(std::remove_if(++argv.begin(), argv.end(), IsNonSwitchArgument),
               argv.end());
    command_line->InitFromArgv(argv);
    ASSERT_TRUE(std::equal(argv.begin(), argv.end(),
                           command_line->argv().begin()));
  }

  void SetUpOnMainThread() override {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(RedirectHostsToTestData, kRestoredURLs,
                       arraysize(kRestoredURLs)));
  }

  void ListOfURLs() {
    // Verifies that policy can set the startup pages to a list of URLs.
    base::ListValue urls;
    for (size_t i = 0; i < arraysize(kRestoredURLs); ++i) {
      urls.AppendString(kRestoredURLs[i]);
      expected_urls_.push_back(GURL(kRestoredURLs[i]));
    }
    PolicyMap policies;
    policies.Set(
        key::kRestoreOnStartup, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        POLICY_SOURCE_CLOUD,
        base::WrapUnique(new base::Value(SessionStartupPref::kPrefValueURLs)),
        nullptr);
    policies.Set(key::kRestoreOnStartupURLs, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, urls.CreateDeepCopy(),
                 nullptr);
    provider_.UpdateChromePolicy(policies);
  }

  void NTP() {
    // Verifies that policy can set the startup page to the NTP.
    PolicyMap policies;
    policies.Set(
        key::kRestoreOnStartup, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        POLICY_SOURCE_CLOUD,
        base::WrapUnique(new base::Value(SessionStartupPref::kPrefValueNewTab)),
        nullptr);
    provider_.UpdateChromePolicy(policies);
    expected_urls_.push_back(GURL(chrome::kChromeUINewTabURL));
  }

  void Last() {
    // Verifies that policy can set the startup pages to the last session.
    PolicyMap policies;
    policies.Set(
        key::kRestoreOnStartup, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        POLICY_SOURCE_CLOUD,
        base::WrapUnique(new base::Value(SessionStartupPref::kPrefValueLast)),
        nullptr);
    provider_.UpdateChromePolicy(policies);
    // This should restore the tabs opened at PRE_RunTest below.
    for (size_t i = 0; i < arraysize(kRestoredURLs); ++i)
      expected_urls_.push_back(GURL(kRestoredURLs[i]));
  }

  std::vector<GURL> expected_urls_;
};

IN_PROC_BROWSER_TEST_P(RestoreOnStartupPolicyTest, PRE_RunTest) {
  // Do not show Welcome Page.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kHasSeenWelcomePage,
                                               true);

#if defined(OS_WIN)
  // Do not show the Windows 10 promo page.
  g_browser_process->local_state()->SetBoolean(prefs::kHasSeenWin10PromoPage,
                                               true);
#endif

  // Open some tabs to verify if they are restored after the browser restarts.
  // Most policy settings override this, except kPrefValueLast which enforces
  // a restore.
  ui_test_utils::NavigateToURL(browser(), GURL(kRestoredURLs[0]));
  for (size_t i = 1; i < arraysize(kRestoredURLs); ++i) {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::AddSelectedTabWithURL(browser(), GURL(kRestoredURLs[i]),
                                  ui::PAGE_TRANSITION_LINK);
    observer.Wait();
  }
}

// Flaky(crbug.com/701023)
IN_PROC_BROWSER_TEST_P(RestoreOnStartupPolicyTest, DISABLED_RunTest) {
  TabStripModel* model = browser()->tab_strip_model();
  int size = static_cast<int>(expected_urls_.size());
  EXPECT_EQ(size, model->count());
  for (int i = 0; i < size && i < model->count(); ++i) {
    EXPECT_EQ(expected_urls_[i], model->GetWebContentsAt(i)->GetURL());
  }
}

INSTANTIATE_TEST_CASE_P(
    RestoreOnStartupPolicyTestInstance,
    RestoreOnStartupPolicyTest,
    testing::Values(&RestoreOnStartupPolicyTest::ListOfURLs,
                    &RestoreOnStartupPolicyTest::NTP,
                    &RestoreOnStartupPolicyTest::Last));

// Similar to PolicyTest but sets a couple of policies before the browser is
// started.
class PolicyStatisticsCollectorTest : public PolicyTest {
 public:
  PolicyStatisticsCollectorTest() {}
  ~PolicyStatisticsCollectorTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(key::kShowHomeButton, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::MakeUnique<base::Value>(true), nullptr);
    policies.Set(key::kBookmarkBarEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::MakeUnique<base::Value>(false), nullptr);
    policies.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::MakeUnique<base::Value>("http://chromium.org"), nullptr);
    provider_.UpdateChromePolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(PolicyStatisticsCollectorTest, Startup) {
  // Verifies that policy usage histograms are collected at startup.

  // BrowserPolicyConnector::Init() has already been called. Make sure the
  // CompleteInitialization() task has executed as well.
  content::RunAllPendingInMessageLoop();

  GURL kAboutHistograms = GURL(std::string(url::kAboutScheme) +
                               std::string(url::kStandardSchemeSeparator) +
                               std::string(content::kChromeUIHistogramHost));
  ui_test_utils::NavigateToURL(browser(), kAboutHistograms);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::string text;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      contents,
      "var nodes = document.querySelectorAll('body > pre');"
      "var result = '';"
      "for (var i = 0; i < nodes.length; ++i) {"
      "  var text = nodes[i].innerHTML;"
      "  if (text.indexOf('Histogram: Enterprise.Policies') === 0) {"
      "    result = text;"
      "    break;"
      "  }"
      "}"
      "domAutomationController.send(result);",
      &text));
  ASSERT_FALSE(text.empty());
  const std::string kExpectedLabel =
      "Histogram: Enterprise.Policies recorded 3 samples";
  EXPECT_EQ(kExpectedLabel, text.substr(0, kExpectedLabel.size()));
  // HomepageLocation has policy ID 1.
  EXPECT_NE(std::string::npos, text.find("<br>1   ---"));
  // ShowHomeButton has policy ID 35.
  EXPECT_NE(std::string::npos, text.find("<br>35  ---"));
  // BookmarkBarEnabled has policy ID 82.
  EXPECT_NE(std::string::npos, text.find("<br>82  ---"));
}

class MediaStreamDevicesControllerBrowserTest
    : public PolicyTest,
      public testing::WithParamInterface<bool> {
 public:
  // TODO(raymes): When crbug.com/606138 is finished and the
  // PermissionRequestManager is used to show all prompts on Android/Desktop
  // we should remove PermissionPromptDelegate and just use
  // MockPermissionPromptFactory instead. The APIs are the same.
  class TestPermissionPromptDelegate
      : public MediaStreamDevicesController::PermissionPromptDelegate {
   public:
    void ShowPrompt(bool user_gesture,
                    content::WebContents* web_contents,
                    std::unique_ptr<MediaStreamDevicesController::Request>
                        request) override {
      if (response_type_ == PermissionRequestManager::ACCEPT_ALL)
        request->PermissionGranted();
      else if (response_type_ == PermissionRequestManager::DENY_ALL)
        request->PermissionDenied();
    }

    void set_response_type(
        PermissionRequestManager::AutoResponseType response_type) {
      response_type_ = response_type;
    }

   private:
    PermissionRequestManager::AutoResponseType response_type_ =
        PermissionRequestManager::NONE;
  };

  MediaStreamDevicesControllerBrowserTest()
      : request_url_allowed_via_whitelist_(false),
        request_url_("https://www.example.com/foo") {
    policy_value_ = GetParam();
    prompt_delegate_.set_response_type(PermissionRequestManager::ACCEPT_ALL);
  }
  virtual ~MediaStreamDevicesControllerBrowserTest() {}

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    ui_test_utils::NavigateToURL(browser(), request_url_);
  }

  content::MediaStreamRequest CreateRequest(
      content::MediaStreamType audio_request_type,
      content::MediaStreamType video_request_type) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(request_url_,
              web_contents->GetMainFrame()->GetLastCommittedURL());
    int render_process_id = web_contents->GetRenderProcessHost()->GetID();
    int render_frame_id = web_contents->GetMainFrame()->GetRoutingID();
    return content::MediaStreamRequest(
        render_process_id, render_frame_id, 0, request_url_.GetOrigin(), false,
        content::MEDIA_DEVICE_ACCESS, std::string(), std::string(),
        audio_request_type, video_request_type, false);
  }

  // Configure a given policy map. The |policy_name| is the name of either the
  // audio or video capture allow policy and must never be NULL.
  // |whitelist_policy| and |allow_rule| are optional.  If NULL, no whitelist
  // policy is set.  If non-NULL, the whitelist policy is set to contain either
  // the |allow_rule| (if non-NULL) or an "allow all" wildcard.
  void ConfigurePolicyMap(PolicyMap* policies, const char* policy_name,
                          const char* whitelist_policy,
                          const char* allow_rule) {
    policies->Set(policy_name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_CLOUD,
                  base::MakeUnique<base::Value>(policy_value_), nullptr);

    if (whitelist_policy) {
      // Add an entry to the whitelist that allows the specified URL regardless
      // of the setting of kAudioCapturedAllowed.
      std::unique_ptr<base::ListValue> list(new base::ListValue);
      if (allow_rule) {
        list->AppendString(allow_rule);
        request_url_allowed_via_whitelist_ = true;
      } else {
        list->AppendString(ContentSettingsPattern::Wildcard().ToString());
        // We should ignore all wildcard entries in the whitelist, so even
        // though we've added an entry, it should be ignored and our expectation
        // is that the request has not been allowed via the whitelist.
        request_url_allowed_via_whitelist_ = false;
      }
      policies->Set(whitelist_policy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                    POLICY_SOURCE_CLOUD, std::move(list), nullptr);
    }
  }

  void Accept(const content::MediaStreamDevices& devices,
              content::MediaStreamRequestResult result,
              std::unique_ptr<content::MediaStreamUI> ui) {
    if (policy_value_ || request_url_allowed_via_whitelist_) {
      ASSERT_EQ(1U, devices.size());
      ASSERT_EQ("fake_dev", devices[0].id);
    } else {
      ASSERT_EQ(0U, devices.size());
    }
  }

  void FinishAudioTest() {
    content::MediaStreamRequest request(CreateRequest(
        content::MEDIA_DEVICE_AUDIO_CAPTURE, content::MEDIA_NO_SERVICE));
    // TODO(raymes): Test MEDIA_DEVICE_OPEN (Pepper) which grants both webcam
    // and microphone permissions at the same time.
    MediaStreamDevicesController::RequestPermissionsWithDelegate(
        request,
        base::Bind(&MediaStreamDevicesControllerBrowserTest::Accept,
                   base::Unretained(this)),
        &prompt_delegate_);

    base::MessageLoop::current()->QuitWhenIdle();
  }

  void FinishVideoTest() {
    content::MediaStreamRequest request(CreateRequest(
        content::MEDIA_NO_SERVICE, content::MEDIA_DEVICE_VIDEO_CAPTURE));
    // TODO(raymes): Test MEDIA_DEVICE_OPEN (Pepper) which grants both webcam
    // and microphone permissions at the same time.
    MediaStreamDevicesController::RequestPermissionsWithDelegate(
        request,
        base::Bind(&MediaStreamDevicesControllerBrowserTest::Accept,
                   base::Unretained(this)),
        &prompt_delegate_);

    base::MessageLoop::current()->QuitWhenIdle();
  }

  TestPermissionPromptDelegate prompt_delegate_;
  bool policy_value_;
  bool request_url_allowed_via_whitelist_;
  GURL request_url_;
  static const char kExampleRequestPattern[];
};

// static
const char MediaStreamDevicesControllerBrowserTest::kExampleRequestPattern[] =
    "https://[*.]example.com/";

IN_PROC_BROWSER_TEST_P(MediaStreamDevicesControllerBrowserTest,
                       AudioCaptureAllowed) {
  content::MediaStreamDevices audio_devices;
  content::MediaStreamDevice fake_audio_device(
      content::MEDIA_DEVICE_AUDIO_CAPTURE, "fake_dev", "Fake Audio Device");
  audio_devices.push_back(fake_audio_device);

  PolicyMap policies;
  ConfigurePolicyMap(&policies, key::kAudioCaptureAllowed, NULL, NULL);
  UpdateProviderPolicy(policies);

  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &MediaCaptureDevicesDispatcher::SetTestAudioCaptureDevices,
          base::Unretained(MediaCaptureDevicesDispatcher::GetInstance()),
          audio_devices),
      base::BindOnce(&MediaStreamDevicesControllerBrowserTest::FinishAudioTest,
                     base::Unretained(this)));

  base::RunLoop().Run();
}

IN_PROC_BROWSER_TEST_P(MediaStreamDevicesControllerBrowserTest,
                       AudioCaptureAllowedUrls) {
  content::MediaStreamDevices audio_devices;
  content::MediaStreamDevice fake_audio_device(
      content::MEDIA_DEVICE_AUDIO_CAPTURE, "fake_dev", "Fake Audio Device");
  audio_devices.push_back(fake_audio_device);

  const char* allow_pattern[] = {
    kExampleRequestPattern,
    // This will set an allow-all policy whitelist.  Since we do not allow
    // setting an allow-all entry in the whitelist, this entry should be ignored
    // and therefore the request should be denied.
    NULL,
  };

  for (size_t i = 0; i < arraysize(allow_pattern); ++i) {
    PolicyMap policies;
    ConfigurePolicyMap(&policies, key::kAudioCaptureAllowed,
                       key::kAudioCaptureAllowedUrls, allow_pattern[i]);
    UpdateProviderPolicy(policies);

    content::BrowserThread::PostTaskAndReply(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(
            &MediaCaptureDevicesDispatcher::SetTestAudioCaptureDevices,
            base::Unretained(MediaCaptureDevicesDispatcher::GetInstance()),
            audio_devices),
        base::BindOnce(
            &MediaStreamDevicesControllerBrowserTest::FinishAudioTest,
            base::Unretained(this)));

    base::RunLoop().Run();
  }
}

IN_PROC_BROWSER_TEST_P(MediaStreamDevicesControllerBrowserTest,
                       VideoCaptureAllowed) {
  content::MediaStreamDevices video_devices;
  content::MediaStreamDevice fake_video_device(
      content::MEDIA_DEVICE_VIDEO_CAPTURE, "fake_dev", "Fake Video Device");
  video_devices.push_back(fake_video_device);

  PolicyMap policies;
  ConfigurePolicyMap(&policies, key::kVideoCaptureAllowed, NULL, NULL);
  UpdateProviderPolicy(policies);

  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &MediaCaptureDevicesDispatcher::SetTestVideoCaptureDevices,
          base::Unretained(MediaCaptureDevicesDispatcher::GetInstance()),
          video_devices),
      base::BindOnce(&MediaStreamDevicesControllerBrowserTest::FinishVideoTest,
                     base::Unretained(this)));

  base::RunLoop().Run();
}

IN_PROC_BROWSER_TEST_P(MediaStreamDevicesControllerBrowserTest,
                       VideoCaptureAllowedUrls) {
  content::MediaStreamDevices video_devices;
  content::MediaStreamDevice fake_video_device(
      content::MEDIA_DEVICE_VIDEO_CAPTURE, "fake_dev", "Fake Video Device");
  video_devices.push_back(fake_video_device);

  const char* allow_pattern[] = {
    kExampleRequestPattern,
    // This will set an allow-all policy whitelist.  Since we do not allow
    // setting an allow-all entry in the whitelist, this entry should be ignored
    // and therefore the request should be denied.
    NULL,
  };

  for (size_t i = 0; i < arraysize(allow_pattern); ++i) {
    PolicyMap policies;
    ConfigurePolicyMap(&policies, key::kVideoCaptureAllowed,
                       key::kVideoCaptureAllowedUrls, allow_pattern[i]);
    UpdateProviderPolicy(policies);

    content::BrowserThread::PostTaskAndReply(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(
            &MediaCaptureDevicesDispatcher::SetTestVideoCaptureDevices,
            base::Unretained(MediaCaptureDevicesDispatcher::GetInstance()),
            video_devices),
        base::BindOnce(
            &MediaStreamDevicesControllerBrowserTest::FinishVideoTest,
            base::Unretained(this)));

    base::RunLoop().Run();
  }
}

INSTANTIATE_TEST_CASE_P(MediaStreamDevicesControllerBrowserTestInstance,
                        MediaStreamDevicesControllerBrowserTest,
                        testing::Bool());

class WebBluetoothPolicyTest : public PolicyTest {
  void SetUpCommandLine(base::CommandLine* command_line)override {
    // TODO(juncai): Remove this switch once Web Bluetooth is supported on Linux
    // and Windows.
    // https://crbug.com/570344
    // https://crbug.com/507419
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    PolicyTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(WebBluetoothPolicyTest, Block) {
  // Fake the BluetoothAdapter to say it's present.
  scoped_refptr<device::MockBluetoothAdapter> adapter =
      new testing::NiceMock<device::MockBluetoothAdapter>;
  EXPECT_CALL(*adapter, IsPresent()).WillRepeatedly(testing::Return(true));
  auto bt_global_values =
      device::BluetoothAdapterFactory::Get().InitGlobalValuesForTesting();
  bt_global_values->SetLESupported(true);
  device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);

  // Navigate to a secure context.
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("localhost", "/simple_page.html"));
  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_THAT(
      web_contents->GetMainFrame()->GetLastCommittedOrigin().Serialize(),
      testing::StartsWith("http://localhost:"));

  // Set the policy to block Web Bluetooth.
  PolicyMap policies;
  policies.Set(key::kDefaultWebBluetoothGuardSetting, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(2), nullptr);
  UpdateProviderPolicy(policies);

  std::string rejection;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "navigator.bluetooth.requestDevice({filters: [{name: 'Hello'}]})"
      "  .then(() => { domAutomationController.send('Success'); },"
      "        reason => {"
      "      domAutomationController.send(reason.name + ': ' + reason.message);"
      "  });",
      &rejection));
  EXPECT_THAT(rejection, testing::MatchesRegex("NotFoundError: .*policy.*"));
}

IN_PROC_BROWSER_TEST_F(PolicyTest,
                       CertificateTransparencyEnforcementDisabledForUrls) {
  // Cleanup any globals even if the test fails.
  base::ScopedClosureRunner cleanup(base::Bind(
      base::IgnoreResult(&BrowserThread::PostTask), BrowserThread::IO,
      FROM_HERE,
      base::Bind(&net::TransportSecurityState::SetShouldRequireCTForTesting,
                 nullptr)));

  net::EmbeddedTestServer https_server_ok(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_ok.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  https_server_ok.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_ok.Start());

  // Require CT for all hosts (in the absence of policy).
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(net::TransportSecurityState::SetShouldRequireCTForTesting,
                     base::Owned(new bool(true))));

  ui_test_utils::NavigateToURL(browser(), https_server_ok.GetURL("/"));

  // The page should initially be blocked.
  const content::InterstitialPage* interstitial =
      content::InterstitialPage::GetInterstitialPage(
          browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(interstitial);
  ASSERT_TRUE(content::WaitForRenderFrameReady(interstitial->GetMainFrame()));

  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      interstitial, "proceed-link"));
  EXPECT_NE(base::UTF8ToUTF16("OK"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

  // Now exempt the URL from being blocked by setting policy.
  std::unique_ptr<base::ListValue> disabled_urls =
      base::MakeUnique<base::ListValue>();
  disabled_urls->AppendString(https_server_ok.host_port_pair().HostForURL());

  PolicyMap policies;
  policies.Set(key::kCertificateTransparencyEnforcementDisabledForUrls,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::move(disabled_urls), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();

  ui_test_utils::NavigateToURL(browser(),
                               https_server_ok.GetURL("/simple.html"));

  // There should be no interstitial after the page loads.
  interstitial = content::InterstitialPage::GetInterstitialPage(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_FALSE(interstitial);

  EXPECT_EQ(base::UTF8ToUTF16("OK"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
}

// Test that when extended reporting opt-in is disabled by policy, the
// opt-in checkbox does not appear on SSL blocking pages.
IN_PROC_BROWSER_TEST_F(PolicyTest, SafeBrowsingExtendedReportingOptInAllowed) {
  net::EmbeddedTestServer https_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server_expired.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_expired.Start());

  // Set the enterprise policy to disallow opt-in.
  const PrefService* const prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(
      prefs->GetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed));
  PolicyMap policies;
  policies.Set(key::kSafeBrowsingExtendedReportingOptInAllowed,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::WrapUnique(new base::Value(false)), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(
      prefs->GetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed));

  // Navigate to an SSL error page.
  ui_test_utils::NavigateToURL(browser(), https_server_expired.GetURL("/"));

  const content::InterstitialPage* const interstitial =
      content::InterstitialPage::GetInterstitialPage(
          browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(interstitial);
  ASSERT_TRUE(content::WaitForRenderFrameReady(interstitial->GetMainFrame()));
  content::RenderViewHost* const rvh =
      interstitial->GetMainFrame()->GetRenderViewHost();
  ASSERT_TRUE(rvh);

  // Check that the checkbox is not visible.
  int result = 0;
  const std::string command = base::StringPrintf(
      "var node = document.getElementById('extended-reporting-opt-in');"
      "if (node) {"
      "  window.domAutomationController.send(node.offsetWidth > 0 || "
      "      node.offsetHeight > 0 ? %d : %d);"
      "} else {"
      // The node should be present but not visible, so trigger an error
      // by sending false if it's not present.
      "  window.domAutomationController.send(%d);"
      "}",
      security_interstitials::CMD_TEXT_FOUND,
      security_interstitials::CMD_TEXT_NOT_FOUND,
      security_interstitials::CMD_ERROR);
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(rvh, command, &result));
  EXPECT_EQ(security_interstitials::CMD_TEXT_NOT_FOUND, result);
}

// Test that when SSL error overriding is allowed by policy (default), the
// proceed link appears on SSL blocking pages.
IN_PROC_BROWSER_TEST_F(PolicyTest, SSLErrorOverridingAllowed) {
  net::EmbeddedTestServer https_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server_expired.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_expired.Start());

  const PrefService* const prefs = browser()->profile()->GetPrefs();

  // Policy should allow overriding by default.
  EXPECT_TRUE(prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed));

  // Policy allows overriding - navigate to an SSL error page and expect the
  // proceed link.
  ui_test_utils::NavigateToURL(browser(), https_server_expired.GetURL("/"));

  const content::InterstitialPage* const interstitial =
      content::InterstitialPage::GetInterstitialPage(
          browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(interstitial);
  EXPECT_TRUE(content::WaitForRenderFrameReady(interstitial->GetMainFrame()));

  // The interstitial should display the proceed link.
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      interstitial, "proceed-link"));
}

// Test that when SSL error overriding is disallowed by policy, the
// proceed link does not appear on SSL blocking pages and users should not
// be able to proceed.
IN_PROC_BROWSER_TEST_F(PolicyTest, SSLErrorOverridingDisallowed) {
  net::EmbeddedTestServer https_server_expired(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_expired.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  https_server_expired.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_expired.Start());

  const PrefService* const prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed));

  // Disallowing the proceed link by setting the policy to |false|.
  PolicyMap policies;
  policies.Set(key::kSSLErrorOverrideAllowed, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::WrapUnique(new base::Value(false)), nullptr);
  UpdateProviderPolicy(policies);

  // Policy should not allow overriding anymore.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kSSLErrorOverrideAllowed));

  // Policy disallows overriding - navigate to an SSL error page and expect no
  // proceed link.
  ui_test_utils::NavigateToURL(browser(), https_server_expired.GetURL("/"));
  const content::InterstitialPage* const interstitial =
      content::InterstitialPage::GetInterstitialPage(
          browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(interstitial);
  EXPECT_TRUE(content::WaitForRenderFrameReady(interstitial->GetMainFrame()));

  // The interstitial should not display the proceed link.
  EXPECT_FALSE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      interstitial, "proceed-link"));

  // The interstitial should not proceed, even if the command is sent in
  // some other way (e.g., via the keyboard shortcut).
  content::InterstitialPageDelegate* interstitial_delegate =
      content::InterstitialPage::GetInterstitialPage(
          browser()->tab_strip_model()->GetActiveWebContents())
          ->GetDelegateForTesting();
  ASSERT_EQ(SSLBlockingPage::kTypeForTesting,
            interstitial_delegate->GetTypeForTesting());
  SSLBlockingPage* ssl_delegate =
      static_cast<SSLBlockingPage*>(interstitial_delegate);
  ssl_delegate->CommandReceived(
      base::IntToString(security_interstitials::CMD_PROCEED));
  EXPECT_TRUE(interstitial);
  EXPECT_TRUE(browser()
                  ->tab_strip_model()
                  ->GetActiveWebContents()
                  ->ShowingInterstitialPage());
}

// Test that TaskManagerInterface::IsEndProcessEnabled is controlled by
// TaskManagerEndProcessEnabled policy
IN_PROC_BROWSER_TEST_F(PolicyTest, TaskManagerEndProcessEnabled) {
  // By default it's allowed to end tasks.
  EXPECT_TRUE(task_manager::TaskManagerInterface::IsEndProcessEnabled());

  // Disabling ending tasks in task manager by policy
  PolicyMap policies1;
  policies1.Set(key::kTaskManagerEndProcessEnabled, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                base::WrapUnique(new base::Value(false)), nullptr);
  UpdateProviderPolicy(policies1);

  // Policy should not allow ending tasks anymore.
  EXPECT_FALSE(task_manager::TaskManagerInterface::IsEndProcessEnabled());

  // Enabling ending tasks in task manager by policy
  PolicyMap policies2;
  policies2.Set(key::kTaskManagerEndProcessEnabled, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                base::WrapUnique(new base::Value(true)), nullptr);
  UpdateProviderPolicy(policies2);

  // Policy should allow ending tasks again.
  EXPECT_TRUE(task_manager::TaskManagerInterface::IsEndProcessEnabled());
}

// Sets the proper policy before the browser is started.
template<bool enable>
class MediaRouterPolicyTest : public PolicyTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(key::kEnableMediaRouter, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::MakeUnique<base::Value>(enable), nullptr);
    provider_.UpdateChromePolicy(policies);
  }
};

using MediaRouterEnabledPolicyTest = MediaRouterPolicyTest<true>;
using MediaRouterDisabledPolicyTest = MediaRouterPolicyTest<false>;

IN_PROC_BROWSER_TEST_F(MediaRouterEnabledPolicyTest, MediaRouterEnabled) {
  EXPECT_TRUE(media_router::MediaRouterEnabled(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(MediaRouterDisabledPolicyTest, MediaRouterDisabled) {
  EXPECT_FALSE(media_router::MediaRouterEnabled(browser()->profile()));
}

#if !defined(OS_ANDROID)
template <bool enable>
class MediaRouterActionPolicyTest : public PolicyTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(key::kShowCastIconInToolbar, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::MakeUnique<base::Value>(enable), nullptr);
    provider_.UpdateChromePolicy(policies);
  }

 protected:
  bool HasMediaRouterActionAtInit() const {
    const std::set<std::string>& component_ids =
        ToolbarActionsModel::Get(browser()->profile())
            ->component_actions_factory()
            ->GetInitialComponentIds();

    return base::ContainsKey(
        component_ids, ComponentToolbarActionsFactory::kMediaRouterActionId);
  }
};

using MediaRouterActionEnabledPolicyTest = MediaRouterActionPolicyTest<true>;
using MediaRouterActionDisabledPolicyTest = MediaRouterActionPolicyTest<false>;

IN_PROC_BROWSER_TEST_F(MediaRouterActionEnabledPolicyTest,
                       MediaRouterActionEnabled) {
  EXPECT_TRUE(
      MediaRouterActionController::IsActionShownByPolicy(browser()->profile()));
  EXPECT_TRUE(HasMediaRouterActionAtInit());
}

IN_PROC_BROWSER_TEST_F(MediaRouterActionDisabledPolicyTest,
                       MediaRouterActionDisabled) {
  EXPECT_FALSE(
      MediaRouterActionController::IsActionShownByPolicy(browser()->profile()));
  EXPECT_FALSE(HasMediaRouterActionAtInit());
}
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_WEBRTC)
// Sets the proper policy before the browser is started.
template <bool enable>
class WebRtcUdpPortRangePolicyTest : public PolicyTest {
 public:
  WebRtcUdpPortRangePolicyTest() = default;
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    if (enable) {
      policies.Set(key::kWebRtcUdpPortRange, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                   base::MakeUnique<base::Value>(kTestWebRtcUdpPortRange),
                   nullptr);
    }
    provider_.UpdateChromePolicy(policies);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRtcUdpPortRangePolicyTest<enable>);
};

using WebRtcUdpPortRangeEnabledPolicyTest = WebRtcUdpPortRangePolicyTest<true>;
using WebRtcUdpPortRangeDisabledPolicyTest =
    WebRtcUdpPortRangePolicyTest<false>;

IN_PROC_BROWSER_TEST_F(WebRtcUdpPortRangeEnabledPolicyTest,
                       WebRtcUdpPortRangeEnabled) {
  std::string port_range;
  const PrefService::Preference* pref =
      user_prefs::UserPrefs::Get(browser()->profile())
          ->FindPreference(prefs::kWebRTCUDPPortRange);
  pref->GetValue()->GetAsString(&port_range);
  EXPECT_EQ(kTestWebRtcUdpPortRange, port_range);
}

IN_PROC_BROWSER_TEST_F(WebRtcUdpPortRangeDisabledPolicyTest,
                       WebRtcUdpPortRangeDisabled) {
  std::string port_range;
  const PrefService::Preference* pref =
      user_prefs::UserPrefs::Get(browser()->profile())
          ->FindPreference(prefs::kWebRTCUDPPortRange);
  pref->GetValue()->GetAsString(&port_range);
  EXPECT_TRUE(port_range.empty());
}
#endif  // BUILDFLAG(ENABLE_WEBRTC)

// Tests the ComponentUpdater's EnabledComponentUpdates group policy by
// calling the OnDemand interface. It uses the network interceptor to inspect
// the presence of the updatedisabled="true" attribute in the update check
// request. The update check request is expected to fail, since CUP fails.
class ComponentUpdaterPolicyTest : public PolicyTest {
 public:
  ComponentUpdaterPolicyTest();
  ~ComponentUpdaterPolicyTest() override;

 protected:
  using TestCaseAction = void (ComponentUpdaterPolicyTest::*)();
  using TestCase = std::pair<TestCaseAction, TestCaseAction>;

  // These test scenarios run as part of one test case by using the
  // CallAsync helper, which calls OnDemand, then chains up to the next
  // scenario when the OnDemandComplete callback fires.
  void DefaultPolicy_GroupPolicySupported();
  void FinishDefaultPolicy_GroupPolicySupported();

  void DefaultPolicy_GroupPolicyNotSupported();
  void FinishDefaultPolicy_GroupPolicyNotSupported();

  void EnabledPolicy_GroupPolicySupported();
  void FinishEnabledPolicy_GroupPolicySupported();

  void EnabledPolicy_GroupPolicyNotSupported();
  void FinishEnabledPolicy_GroupPolicyNotSupported();

  void DisabledPolicy_GroupPolicySupported();
  void FinishDisabled_PolicyGroupPolicySupported();

  void DisabledPolicy_GroupPolicyNotSupported();
  void FinishDisabledPolicy_GroupPolicyNotSupported();

  void BeginTest();
  void EndTest();

  void UpdateComponent(const update_client::CrxComponent& crx_component);
  void CallAsync(TestCaseAction action);
  void VerifyExpectations(bool update_disabled);

  void SetEnableComponentUpdates(bool enable_component_updates);

  static update_client::CrxComponent MakeCrxComponent(
      bool supports_group_policy_enable_component_updates);

  TestCase cur_test_case_;

  static const char component_id_[];

  static const bool kUpdateDisabled = true;

 private:
  void OnDemandComplete(update_client::Error error);

  std::unique_ptr<update_client::URLRequestPostInterceptorFactory>
      interceptor_factory_;

  // This member is owned by the |interceptor_factory_|.
  update_client::URLRequestPostInterceptor* post_interceptor_ = nullptr;

  // This member is owned by g_browser_process;
  component_updater::ComponentUpdateService* cus_;

  DISALLOW_COPY_AND_ASSIGN(ComponentUpdaterPolicyTest);
};

const char ComponentUpdaterPolicyTest::component_id_[] =
    "jebgalgnebhfojomionfpkfelancnnkf";

ComponentUpdaterPolicyTest::ComponentUpdaterPolicyTest() {}

ComponentUpdaterPolicyTest::~ComponentUpdaterPolicyTest() {}

void ComponentUpdaterPolicyTest::SetEnableComponentUpdates(
    bool enable_component_updates) {
  PolicyMap policies;
  policies.Set(key::kComponentUpdatesEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_MACHINE, POLICY_SOURCE_ENTERPRISE_DEFAULT,
               base::WrapUnique(new base::Value(enable_component_updates)),
               nullptr);
  UpdateProviderPolicy(policies);
}

update_client::CrxComponent ComponentUpdaterPolicyTest::MakeCrxComponent(
    bool supports_group_policy_enable_component_updates) {
  class MockInstaller : public update_client::CrxInstaller {
   public:
    MockInstaller() {}

    MOCK_METHOD1(OnUpdateError, void(int error));
    MOCK_METHOD2(Install,
                 update_client::CrxInstaller::Result(
                     const base::DictionaryValue& manifest,
                     const base::FilePath& unpack_path));
    MOCK_METHOD2(GetInstalledFile,
                 bool(const std::string& file, base::FilePath* installed_file));
    MOCK_METHOD0(Uninstall, bool());

   private:
    ~MockInstaller() override {}
  };

  // component id "jebgalgnebhfojomionfpkfelancnnkf".
  static const uint8_t jebg_hash[] = {
      0x94, 0x16, 0x0b, 0x6d, 0x41, 0x75, 0xe9, 0xec, 0x8e, 0xd5, 0xfa,
      0x54, 0xb0, 0xd2, 0xdd, 0xa5, 0x6e, 0x05, 0x6b, 0xe8, 0x73, 0x47,
      0xf6, 0xc4, 0x11, 0x9f, 0xbc, 0xb3, 0x09, 0xb3, 0x5b, 0x40};

  // The component uses HTTPS only for network interception purposes.
  update_client::CrxComponent crx_component;
  crx_component.pk_hash.assign(std::begin(jebg_hash), std::end(jebg_hash));
  crx_component.version = base::Version("0.9");
  crx_component.installer = scoped_refptr<MockInstaller>(new MockInstaller());
  crx_component.requires_network_encryption = true;
  crx_component.supports_group_policy_enable_component_updates =
      supports_group_policy_enable_component_updates;

  return crx_component;
}

void ComponentUpdaterPolicyTest::UpdateComponent(
    const update_client::CrxComponent& crx_component) {
  post_interceptor_->Reset();
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      new update_client::PartialMatch("updatecheck"), 200));
  EXPECT_TRUE(cus_->RegisterComponent(crx_component));
  cus_->GetOnDemandUpdater().OnDemandUpdate(
      component_id_, base::Bind(&ComponentUpdaterPolicyTest::OnDemandComplete,
                                base::Unretained(this)));
}

void ComponentUpdaterPolicyTest::CallAsync(TestCaseAction action) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(action, base::Unretained(this)));
}

void ComponentUpdaterPolicyTest::OnDemandComplete(update_client::Error error) {
  CallAsync(cur_test_case_.second);
}

void ComponentUpdaterPolicyTest::BeginTest() {
  cus_ = g_browser_process->component_updater();

  const auto config = component_updater::MakeChromeComponentUpdaterConfigurator(
      base::CommandLine::ForCurrentProcess(), nullptr,
      g_browser_process->local_state());
  const auto urls = config->UpdateUrl();
  ASSERT_TRUE(urls.size());
  const GURL url = urls.front();

  interceptor_factory_ =
      base::MakeUnique<update_client::URLRequestPostInterceptorFactory>(
          url.scheme(), url.host(),
          BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));

  post_interceptor_ = interceptor_factory_->CreateInterceptor(
      base::FilePath(FILE_PATH_LITERAL("service/update2")));

  cur_test_case_ = std::make_pair(
      &ComponentUpdaterPolicyTest::DefaultPolicy_GroupPolicySupported,
      &ComponentUpdaterPolicyTest::FinishDefaultPolicy_GroupPolicySupported);

  CallAsync(cur_test_case_.first);
}

void ComponentUpdaterPolicyTest::EndTest() {
  interceptor_factory_ = nullptr;
  cus_ = nullptr;

  base::MessageLoop::current()->QuitWhenIdle();
}

void ComponentUpdaterPolicyTest::VerifyExpectations(bool update_disabled) {
  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_NE(std::string::npos,
            post_interceptor_->GetRequests()[0].find(base::StringPrintf(
                "<updatecheck%s/>",
                update_disabled ? " updatedisabled=\"true\"" : "")));
}

void ComponentUpdaterPolicyTest::DefaultPolicy_GroupPolicySupported() {
  UpdateComponent(MakeCrxComponent(true));
}

void ComponentUpdaterPolicyTest::FinishDefaultPolicy_GroupPolicySupported() {
  // Default policy && policy support -> updates are enabled.
  VerifyExpectations(!kUpdateDisabled);

  cur_test_case_ = std::make_pair(
      &ComponentUpdaterPolicyTest::DefaultPolicy_GroupPolicyNotSupported,
      &ComponentUpdaterPolicyTest::FinishDefaultPolicy_GroupPolicyNotSupported);
  CallAsync(cur_test_case_.first);
}

void ComponentUpdaterPolicyTest::DefaultPolicy_GroupPolicyNotSupported() {
  UpdateComponent(MakeCrxComponent(false));
}

void ComponentUpdaterPolicyTest::FinishDefaultPolicy_GroupPolicyNotSupported() {
  // Default policy && no policy support -> updates are enabled.
  VerifyExpectations(!kUpdateDisabled);

  cur_test_case_ = std::make_pair(
      &ComponentUpdaterPolicyTest::EnabledPolicy_GroupPolicySupported,
      &ComponentUpdaterPolicyTest::FinishEnabledPolicy_GroupPolicySupported);
  CallAsync(cur_test_case_.first);
}

void ComponentUpdaterPolicyTest::EnabledPolicy_GroupPolicySupported() {
  SetEnableComponentUpdates(true);
  UpdateComponent(MakeCrxComponent(true));
}

void ComponentUpdaterPolicyTest::FinishEnabledPolicy_GroupPolicySupported() {
  // Updates enabled policy && policy support -> updates are enabled.
  VerifyExpectations(!kUpdateDisabled);

  cur_test_case_ = std::make_pair(
      &ComponentUpdaterPolicyTest::EnabledPolicy_GroupPolicyNotSupported,
      &ComponentUpdaterPolicyTest::FinishEnabledPolicy_GroupPolicyNotSupported);
  CallAsync(cur_test_case_.first);
}

void ComponentUpdaterPolicyTest::EnabledPolicy_GroupPolicyNotSupported() {
  SetEnableComponentUpdates(true);
  UpdateComponent(MakeCrxComponent(false));
}

void ComponentUpdaterPolicyTest::FinishEnabledPolicy_GroupPolicyNotSupported() {
  // Updates enabled policy && no policy support -> updates are enabled.
  VerifyExpectations(!kUpdateDisabled);

  cur_test_case_ = std::make_pair(
      &ComponentUpdaterPolicyTest::DisabledPolicy_GroupPolicySupported,
      &ComponentUpdaterPolicyTest::FinishDisabled_PolicyGroupPolicySupported);
  CallAsync(cur_test_case_.first);
}

void ComponentUpdaterPolicyTest::DisabledPolicy_GroupPolicySupported() {
  SetEnableComponentUpdates(false);
  UpdateComponent(MakeCrxComponent(true));
}

void ComponentUpdaterPolicyTest::FinishDisabled_PolicyGroupPolicySupported() {
  // Updates enabled policy && policy support -> updates are disabled.
  VerifyExpectations(kUpdateDisabled);

  cur_test_case_ = std::make_pair(
      &ComponentUpdaterPolicyTest::DisabledPolicy_GroupPolicyNotSupported,
      &ComponentUpdaterPolicyTest::
          FinishDisabledPolicy_GroupPolicyNotSupported);
  CallAsync(cur_test_case_.first);
}

void ComponentUpdaterPolicyTest::DisabledPolicy_GroupPolicyNotSupported() {
  SetEnableComponentUpdates(false);
  UpdateComponent(MakeCrxComponent(false));
}

void ComponentUpdaterPolicyTest::
    FinishDisabledPolicy_GroupPolicyNotSupported() {
  // Updates enabled policy && no policy support -> updates are enabled.
  VerifyExpectations(!kUpdateDisabled);

  cur_test_case_ = TestCase();
  CallAsync(&ComponentUpdaterPolicyTest::EndTest);
}

IN_PROC_BROWSER_TEST_F(ComponentUpdaterPolicyTest, EnabledComponentUpdates) {
  BeginTest();
  base::RunLoop().Run();
}

#if !defined(OS_CHROMEOS)
// Similar to PolicyTest but sets the proper policy before the browser is
// started.
class PolicyVariationsServiceTest : public PolicyTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(key::kVariationsRestrictParameter, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::MakeUnique<base::Value>("restricted"), nullptr);
    provider_.UpdateChromePolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(PolicyVariationsServiceTest, VariationsURLIsValid) {
  const std::string default_variations_url =
      variations::VariationsService::GetDefaultVariationsServerURLForTesting();

  // g_browser_process->variations_service() is null by default in Chromium
  // builds, so construct a VariationsService locally instead.
  std::unique_ptr<variations::VariationsService> service =
      variations::VariationsService::CreateForTesting(
          base::MakeUnique<ChromeVariationsServiceClient>(),
          g_browser_process->local_state());

  const GURL url = service->GetVariationsServerURL(
      g_browser_process->local_state(), std::string());
  EXPECT_TRUE(base::StartsWith(url.spec(), default_variations_url,
                               base::CompareCase::SENSITIVE));
  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "restrict", &value));
  EXPECT_EQ("restricted", value);
}

IN_PROC_BROWSER_TEST_F(PolicyTest, NativeMessagingBlacklistSelective) {
  base::ListValue blacklist;
  blacklist.AppendString("host.name");
  PolicyMap policies;
  policies.Set(key::kNativeMessagingBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               blacklist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);

  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_FALSE(extensions::MessageService::IsNativeMessagingHostAllowed(
      prefs, "host.name"));
  EXPECT_TRUE(extensions::MessageService::IsNativeMessagingHostAllowed(
      prefs, "other.host.name"));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, NativeMessagingBlacklistWildcard) {
  base::ListValue blacklist;
  blacklist.AppendString("*");
  PolicyMap policies;
  policies.Set(key::kNativeMessagingBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               blacklist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);

  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_FALSE(extensions::MessageService::IsNativeMessagingHostAllowed(
      prefs, "host.name"));
  EXPECT_FALSE(extensions::MessageService::IsNativeMessagingHostAllowed(
      prefs, "other.host.name"));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, NativeMessagingWhitelist) {
  base::ListValue blacklist;
  blacklist.AppendString("*");
  base::ListValue whitelist;
  whitelist.AppendString("host.name");
  PolicyMap policies;
  policies.Set(key::kNativeMessagingBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               blacklist.CreateDeepCopy(), nullptr);
  policies.Set(key::kNativeMessagingWhitelist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               whitelist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);

  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(extensions::MessageService::IsNativeMessagingHostAllowed(
      prefs, "host.name"));
  EXPECT_FALSE(extensions::MessageService::IsNativeMessagingHostAllowed(
      prefs, "other.host.name"));
}

#endif  // !defined(CHROME_OS)


#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
// Sets the hardware acceleration mode policy before the browser is started.
class HardwareAccelerationModePolicyTest : public PolicyTest {
 public:
  HardwareAccelerationModePolicyTest() {}

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(key::kHardwareAccelerationModeEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::MakeUnique<base::Value>(false), nullptr);
    provider_.UpdateChromePolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(HardwareAccelerationModePolicyTest,
                       HardwareAccelerationDisabled) {
  // Verifies that hardware acceleration can be disabled with policy.
  EXPECT_FALSE(
      content::GpuDataManager::GetInstance()->HardwareAccelerationEnabled());
}
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
// Policy is only available in ChromeOS
IN_PROC_BROWSER_TEST_F(PolicyTest, UnifiedDesktopEnabledByDefault) {
  // Verify that Unified Desktop can be enabled by policy
  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();

  // The policy description promises that Unified Desktop is not available
  // unless the policy is set (or a command line or an extension is used). If
  // this default behaviour changes, please change the description at
  // components/policy/resources/policy_templates.json.
  EXPECT_FALSE(display_manager->unified_desktop_enabled());
  // Now set the policy and check that unified desktop is turned on.
  PolicyMap policies;
  policies.Set(key::kUnifiedDesktopEnabledByDefault, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::WrapUnique(new base::Value(true)), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(display_manager->unified_desktop_enabled());
  policies.Set(key::kUnifiedDesktopEnabledByDefault, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::WrapUnique(new base::Value(false)), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(display_manager->unified_desktop_enabled());
}

class ArcPolicyTest : public PolicyTest {
 public:
  ArcPolicyTest() {}
  ~ArcPolicyTest() override {}

 protected:
  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    arc::ArcSessionManager::DisableUIForTesting();
    arc::ArcSessionManager::Get()->SetArcSessionRunnerForTesting(
        base::MakeUnique<arc::ArcSessionRunner>(
            base::Bind(arc::FakeArcSession::Create)));

    browser()->profile()->GetPrefs()->SetBoolean(prefs::kArcSignedIn, true);
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted,
                                                 true);
  }

  void TearDownOnMainThread() override {
    arc::ArcSessionManager::Get()->Shutdown();
    PolicyTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyTest::SetUpCommandLine(command_line);
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetArcEnabledByPolicy(bool enabled) {
    PolicyMap policies;
    policies.Set(key::kArcEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD,
                 base::WrapUnique(new base::Value(enabled)), nullptr);
    UpdateProviderPolicy(policies);
    if (browser()) {
      const PrefService* const prefs = browser()->profile()->GetPrefs();
      EXPECT_EQ(prefs->GetBoolean(prefs::kArcEnabled), enabled);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcPolicyTest);
};

// Test ArcEnabled policy.
IN_PROC_BROWSER_TEST_F(ArcPolicyTest, ArcEnabled) {
  const PrefService* const pref = browser()->profile()->GetPrefs();
  const auto* const arc_session_manager = arc::ArcSessionManager::Get();

  // ARC is switched off by default.
  EXPECT_TRUE(arc_session_manager->IsSessionStopped());
  EXPECT_FALSE(pref->GetBoolean(prefs::kArcEnabled));

  // Enable ARC.
  SetArcEnabledByPolicy(true);
  EXPECT_TRUE(arc_session_manager->IsSessionRunning());

  // Disable ARC.
  SetArcEnabledByPolicy(false);
  EXPECT_TRUE(arc_session_manager->IsSessionStopped());
}

// Test ArcBackupRestoreEnabled policy.
IN_PROC_BROWSER_TEST_F(ArcPolicyTest, ArcBackupRestoreEnabled) {
  PrefService* const pref = browser()->profile()->GetPrefs();

  // ARC Backup & Restore is switched off by default.
  EXPECT_FALSE(pref->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_FALSE(pref->IsManagedPreference(prefs::kArcBackupRestoreEnabled));

  // Switch on ARC Backup & Restore in the user prefs.
  pref->SetBoolean(prefs::kArcBackupRestoreEnabled, true);
  EXPECT_TRUE(pref->GetBoolean(prefs::kArcBackupRestoreEnabled));

  // Disable ARC Backup & Restore through the policy.
  PolicyMap policies;
  policies.Set(key::kArcBackupRestoreEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(pref->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_TRUE(pref->IsManagedPreference(prefs::kArcBackupRestoreEnabled));

  // Enable ARC Backup & Restore through the policy.
  policies.Set(key::kArcBackupRestoreEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(true), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(pref->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_TRUE(pref->IsManagedPreference(prefs::kArcBackupRestoreEnabled));
}

// Test ArcLocationServiceEnabled policy and its interplay with the
// DefaultGeolocationSetting policy.
IN_PROC_BROWSER_TEST_F(ArcPolicyTest, ArcLocationServiceEnabled) {
  PrefService* const pref = browser()->profile()->GetPrefs();

  // Values of the ArcLocationServiceEnabled policy to be tested.
  const std::vector<base::Value> test_policy_values = {
      base::Value(),       // unset
      base::Value(false),  // disabled
      base::Value(true),   // enabled
  };
  // Values of the DefaultGeolocationSetting policy to be tested.
  const std::vector<base::Value> test_default_geo_policy_values = {
      base::Value(),   // unset
      base::Value(1),  // 'AllowGeolocation'
      base::Value(2),  // 'BlockGeolocation'
      base::Value(3),  // 'AskGeolocation'
  };

  // The pref is switched off by default.
  EXPECT_FALSE(pref->GetBoolean(prefs::kArcLocationServiceEnabled));
  EXPECT_FALSE(pref->IsManagedPreference(prefs::kArcLocationServiceEnabled));

  // Switch on the pref in the user prefs.
  pref->SetBoolean(prefs::kArcLocationServiceEnabled, true);
  EXPECT_TRUE(pref->GetBoolean(prefs::kArcLocationServiceEnabled));

  for (const auto& test_policy_value : test_policy_values) {
    for (const auto& test_default_geo_policy_value :
         test_default_geo_policy_values) {
      PolicyMap policies;
      if (test_policy_value.is_bool()) {
        policies.Set(key::kArcLocationServiceEnabled, POLICY_LEVEL_MANDATORY,
                     POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                     test_policy_value.CreateDeepCopy(), nullptr);
      }
      if (test_default_geo_policy_value.is_int()) {
        policies.Set(key::kDefaultGeolocationSetting, POLICY_LEVEL_MANDATORY,
                     POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                     test_default_geo_policy_value.CreateDeepCopy(), nullptr);
      }
      UpdateProviderPolicy(policies);

      const bool should_be_disabled_by_policy =
          test_policy_value.is_bool() && !test_policy_value.GetBool();
      const bool should_be_disabled_by_default_geo_policy =
          test_default_geo_policy_value.is_int() &&
          test_default_geo_policy_value.GetInt() == 2;
      const bool expected_pref_value =
          !(should_be_disabled_by_policy ||
            should_be_disabled_by_default_geo_policy);
      EXPECT_EQ(expected_pref_value,
                pref->GetBoolean(prefs::kArcLocationServiceEnabled))
          << "ArcLocationServiceEnabled policy is set to " << test_policy_value
          << "DefaultGeolocationSetting policy is set to "
          << test_default_geo_policy_value;

      const bool expected_pref_managed =
          test_policy_value.is_bool() ||
          (test_default_geo_policy_value.is_int() &&
           test_default_geo_policy_value.GetInt() == 2);
      EXPECT_EQ(expected_pref_managed,
                pref->IsManagedPreference(prefs::kArcLocationServiceEnabled))
          << "ArcLocationServiceEnabled policy is set to " << test_policy_value
          << "DefaultGeolocationSetting policy is set to "
          << test_default_geo_policy_value;
    }
  }
}

namespace {
const char kTestUser1[] = "test1@domain.com";
}  // anonymous namespace

class ChromeOSPolicyTest : public PolicyTest {
 public:
  ChromeOSPolicyTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                    cryptohome_id1_.id());
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "hash");
    command_line->AppendSwitch(
        chromeos::switches::kAllowFailedPolicyFetchForTest);
  }

 protected:
  const AccountId test_account_id1_ = AccountId::FromUserEmail(kTestUser1);
  const cryptohome::Identification cryptohome_id1_ =
      cryptohome::Identification(test_account_id1_);

  // Logs in |account_id|.
  void LogIn(const AccountId& account_id, const std::string& user_id_hash) {
    user_manager::UserManager::Get()->UserLoggedIn(account_id, user_id_hash,
                                                   false);
    base::RunLoop().RunUntilIdle();
  }

  void NavigateToUrl(const GURL& url) {
    ui_test_utils::NavigateToURL(browser(), url);
    base::RunLoop().RunUntilIdle();
  }

  void CheckSystemTimezoneAutomaticDetectionPolicyUnset() {
    PrefService* local_state = g_browser_process->local_state();
    EXPECT_FALSE(local_state->IsManagedPreference(
        prefs::kSystemTimezoneAutomaticDetectionPolicy));
    EXPECT_EQ(0, local_state->GetInteger(
                     prefs::kSystemTimezoneAutomaticDetectionPolicy));
  }

  void SetAndTestSystemTimezoneAutomaticDetectionPolicy(int policy_value) {
    PolicyMap policies;
    policies.Set(key::kSystemTimezoneAutomaticDetection, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                 base::MakeUnique<base::Value>(policy_value), nullptr);
    UpdateProviderPolicy(policies);

    PrefService* local_state = g_browser_process->local_state();

    EXPECT_TRUE(local_state->IsManagedPreference(
        prefs::kSystemTimezoneAutomaticDetectionPolicy));
    EXPECT_EQ(policy_value,
              local_state->GetInteger(
                  prefs::kSystemTimezoneAutomaticDetectionPolicy));
  }

  void SetEmptyPolicy() { UpdateProviderPolicy(PolicyMap()); }

  bool CheckResolveTimezoneByGeolocation(bool checked, bool disabled) {
    checker.set_web_contents(
        browser()->tab_strip_model()->GetActiveWebContents());
    const std::string expression = base::StringPrintf(
        "(function () {\n"
        "  var checkbox = "
        "document.getElementById('resolve-timezone-by-geolocation');\n"
        "  if (!checkbox) {\n"
        "    console.log('resolve-timezone-by-geolocation not found.');\n"
        "    return false;\n"
        "  }\n"
        "  var expected_checked = %s;\n"
        "  var expected_disabled = %s;\n"
        "  var checked = checkbox.checked;\n"
        "  var disabled = checkbox.disabled;\n"
        "  if (checked != expected_checked)\n"
        "    console.log('ERROR: expected_checked=' + expected_checked + ' != "
        "checked=' + checked);\n"
        "\n"
        "  if (disabled != expected_disabled)\n"
        "    console.log('ERROR: expected_disabled=' + expected_disabled + ' "
        "!= disabled=' + disabled);\n"
        "\n"
        "  return (checked == expected_checked && disabled == "
        "expected_disabled);\n"
        "})()",
        checked ? "true" : "false", disabled ? "true" : "false");
    return checker.GetBool(expression);
  }

 private:
  chromeos::test::JSChecker checker;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSPolicyTest);
};

IN_PROC_BROWSER_TEST_F(ChromeOSPolicyTest, SystemTimezoneAutomaticDetection) {
  base::test::ScopedFeatureList disable_md_settings;
  disable_md_settings.InitAndDisableFeature(features::kMaterialDesignSettings);

  ui_test_utils::NavigateToURL(browser(), GURL("chrome://settings"));
  chromeos::system::TimeZoneResolverManager* manager =
      g_browser_process->platform_part()->GetTimezoneResolverManager();

  // Policy not set.
  CheckSystemTimezoneAutomaticDetectionPolicyUnset();
  EXPECT_TRUE(CheckResolveTimezoneByGeolocation(true, false));
  EXPECT_TRUE(manager->TimeZoneResolverShouldBeRunningForTests());

  int policy_value = 0 /* USERS_DECIDE */;
  SetAndTestSystemTimezoneAutomaticDetectionPolicy(policy_value);
  EXPECT_TRUE(CheckResolveTimezoneByGeolocation(true, false));
  EXPECT_TRUE(manager->TimeZoneResolverShouldBeRunningForTests());

  policy_value = 1 /* DISABLED */;
  SetAndTestSystemTimezoneAutomaticDetectionPolicy(policy_value);
  EXPECT_TRUE(CheckResolveTimezoneByGeolocation(false, true));
  EXPECT_FALSE(manager->TimeZoneResolverShouldBeRunningForTests());

  policy_value = 2 /* IP_ONLY */;
  SetAndTestSystemTimezoneAutomaticDetectionPolicy(policy_value);
  EXPECT_TRUE(CheckResolveTimezoneByGeolocation(true, true));
  EXPECT_TRUE(manager->TimeZoneResolverShouldBeRunningForTests());

  policy_value = 3 /* SEND_WIFI_ACCESS_POINTS */;
  SetAndTestSystemTimezoneAutomaticDetectionPolicy(policy_value);
  EXPECT_TRUE(CheckResolveTimezoneByGeolocation(true, true));
  EXPECT_TRUE(manager->TimeZoneResolverShouldBeRunningForTests());

  policy_value = 4 /* SEND_ALL_LOCATION_INFO */;
  SetAndTestSystemTimezoneAutomaticDetectionPolicy(policy_value);
  EXPECT_TRUE(CheckResolveTimezoneByGeolocation(true, true));
  EXPECT_TRUE(manager->TimeZoneResolverShouldBeRunningForTests());

  policy_value = 1 /* DISABLED */;
  SetAndTestSystemTimezoneAutomaticDetectionPolicy(policy_value);
  EXPECT_TRUE(CheckResolveTimezoneByGeolocation(false, true));
  EXPECT_FALSE(manager->TimeZoneResolverShouldBeRunningForTests());

  SetEmptyPolicy();
  // Policy not set.
  CheckSystemTimezoneAutomaticDetectionPolicyUnset();
  EXPECT_TRUE(CheckResolveTimezoneByGeolocation(true, false));
  EXPECT_TRUE(manager->TimeZoneResolverShouldBeRunningForTests());
}
#endif  // defined(OS_CHROMEOS)

class NetworkTimePolicyTest : public PolicyTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kEnableFeatures,
        std::string(network_time::kNetworkTimeServiceQuerying.name) +
            "<SSLNetworkTimeBrowserTestFieldTrial");
    command_line->AppendSwitchASCII(
        switches::kForceFieldTrials,
        "SSLNetworkTimeBrowserTestFieldTrial/Enabled/");
    command_line->AppendSwitchASCII(
        variations::switches::kForceFieldTrialParams,
        "SSLNetworkTimeBrowserTestFieldTrial.Enabled:FetchBehavior/"
        "on-demand-only");
  }

  // A request handler that returns a dummy response and counts the number of
  // times it is called.
  std::unique_ptr<net::test_server::HttpResponse> CountingRequestHandler(
      const net::test_server::HttpRequest& request) {
    net::test_server::BasicHttpResponse* response =
        new net::test_server::BasicHttpResponse();
    num_requests_++;
    response->set_code(net::HTTP_OK);
    response->set_content(
        ")]}'\n"
        "{\"current_time_millis\":1461621971825,\"server_nonce\":-6."
        "006853099049523E85}");
    response->AddCustomHeader("x-cup-server-proof", "dead:beef");
    return std::unique_ptr<net::test_server::HttpResponse>(response);
  }

  uint32_t num_requests() { return num_requests_; }

 private:
  uint32_t num_requests_ = 0;
};

IN_PROC_BROWSER_TEST_F(NetworkTimePolicyTest, NetworkTimeQueriesDisabled) {
  // Set a policy to disable network time queries.
  PolicyMap policies;
  policies.Set(key::kNetworkTimeQueriesEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);

  embedded_test_server()->RegisterRequestHandler(base::Bind(
      &NetworkTimePolicyTest::CountingRequestHandler, base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());
  g_browser_process->network_time_tracker()->SetTimeServerURLForTesting(
      embedded_test_server()->GetURL("/"));

  net::EmbeddedTestServer https_server_expired_(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_expired_.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(https_server_expired_.Start());

  // Navigate to a page with a certificate date error and then check that a
  // network time query was not sent.
  ui_test_utils::NavigateToURL(browser(), https_server_expired_.GetURL("/"));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_TRUE(interstitial_page);
  EXPECT_EQ(0u, num_requests());

  // Now enable the policy and check that a network time query is sent.
  policies.Set(key::kNetworkTimeQueriesEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::MakeUnique<base::Value>(true), nullptr);
  UpdateProviderPolicy(policies);
  ui_test_utils::NavigateToURL(browser(), https_server_expired_.GetURL("/"));
  interstitial_page = tab->GetInterstitialPage();
  ASSERT_TRUE(interstitial_page);
  EXPECT_EQ(1u, num_requests());
}

}  // namespace policy
