// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is in maintenance mode, please do NOT add new tests into this file.
//
// policy_browsertests.cc contains lots of tests for multiple policies. However,
// it became huge with hundreds of policies. Instead of adding even more tests
// here, please put new ones with the policy implementation. For example, a
// network policy test can be moved to chrome/browser/net.
//
// Policy component dependency is not necessary for policy test. Most of
// policy values are copied into local state or Profile prefs. They can be used
// to enable policy during test.
//
// Simple policy to prefs mapping can be tested with policy_test_cases.json. If
// the conversion is complicated and requires custom policy handler, we
// recommend to test the handler separately.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/atomic_ref_count.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_current.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker_test_support.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/webui/welcome/helpers.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/net/safe_search_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/download/public/common/download_item.h"
#include "components/google/core/common/google_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/network_session_configurator/common/network_switches.h"
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
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "components/unified_consent/pref_names.h"
#include "components/update_client/update_client_errors.h"
#include "components/user_prefs/user_prefs.h"
#include "components/variations/service/variations_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/mock_notification_observer.h"
#include "content/public/test/network_service_test_helper.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/signed_exchange_browser_test_helper.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/api/messaging/messaging_delegate.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/switches.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/hash_value.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/cert/x509_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/manager/display_manager.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/shell.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/note_taking_helper.h"
#include "chrome/browser/chromeos/policy/login_policy_test_base.h"
#include "chrome/browser/chromeos/policy/user_policy_test_helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/system/timezone_resolver_manager.h"
#include "chrome/browser/ui/ash/chrome_screenshot_grabber.h"
#include "chrome/browser/ui/ash/chrome_screenshot_grabber_test_observer.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/snapshot/screenshot_grabber.h"
#endif

#if !defined(OS_MACOSX)
#include "base/compiler_specific.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "ui/base/window_open_disposition.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
#include "media/webrtc/webrtc_switches.h"
#include "services/service_manager/sandbox/features.h"
#endif

using content::BrowserThread;
using testing::_;
using testing::AtLeast;
using testing::Mock;
using testing::Return;
using webrtc_event_logging::WebRtcEventLogManager;

namespace policy {

namespace {

#if defined(OS_CHROMEOS)
const int kOneHourInMs = 60 * 60 * 1000;
const int kThreeHoursInMs = 180 * 60 * 1000;
#endif

#if !defined(OS_MACOSX)
const base::FilePath::CharType kUnpackedFullscreenAppName[] =
    FILE_PATH_LITERAL("fullscreen_app");
#endif  // !defined(OS_MACOSX)

// Arbitrary port range for testing the WebRTC UDP port policy.
const char kTestWebRtcUdpPortRange[] = "10000-10100";

constexpr size_t kWebAppId = 42;

content::RenderFrameHost* GetMostVisitedIframe(content::WebContents* tab) {
  for (content::RenderFrameHost* frame : tab->GetAllFrames()) {
    if (frame->GetFrameName() == "mv-single")
      return frame;
  }
  return nullptr;
}

// Verifies that the given url |spec| can be opened. This assumes that |spec|
// points at empty.html in the test data dir.
void CheckCanOpenURL(Browser* browser, const std::string& spec) {
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
void CheckURLIsBlockedInWebContents(content::WebContents* web_contents,
                                    const GURL& url) {
  EXPECT_EQ(url, web_contents->GetURL());

  base::string16 blocked_page_title;
  if (url.has_host()) {
    blocked_page_title = base::UTF8ToUTF16(url.host());
  } else {
    // Local file paths show the full URL.
    blocked_page_title = base::UTF8ToUTF16(url.spec());
  }
  EXPECT_EQ(blocked_page_title, web_contents->GetTitle());

  // Verify that the expected error page is being displayed.
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "var textContent = document.body.textContent;"
      "var hasError = textContent.indexOf('ERR_BLOCKED_BY_ADMINISTRATOR') >= 0;"
      "domAutomationController.send(hasError);",
      &result));
  EXPECT_TRUE(result);
}

// Verifies that access to the given url |spec| is blocked.
void CheckURLIsBlocked(Browser* browser, const std::string& spec) {
  GURL url(spec);
  ui_test_utils::NavigateToURL(browser, url);
  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  CheckURLIsBlockedInWebContents(contents, url);
}

// Downloads a file named |file| and expects it to be saved to |dir|, which
// must be empty.
void DownloadAndVerifyFile(Browser* browser,
                           const base::FilePath& dir,
                           const base::FilePath& file) {
  net::EmbeddedTestServer embedded_test_server;
  base::FilePath test_data_directory;
  GetTestDataDirectory(&test_data_directory);
  embedded_test_server.ServeFilesFromDirectory(test_data_directory);
  ASSERT_TRUE(embedded_test_server.Start());
  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(browser->profile());
  content::DownloadTestObserverTerminal observer(
      download_manager, 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  GURL url(embedded_test_server.GetURL("/" + file.MaybeAsASCII()));
  base::FilePath downloaded = dir.Append(file);
  EXPECT_FALSE(base::PathExists(downloaded));
  ui_test_utils::NavigateToURL(browser, url);
  observer.WaitForFinished();
  EXPECT_EQ(1u,
            observer.NumDownloadsSeenInState(download::DownloadItem::COMPLETE));
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

bool IsNetworkPredictionEnabled(PrefService* prefs) {
  return chrome_browser_net::CanPrefetchAndPrerenderUI(prefs) ==
      chrome_browser_net::NetworkPredictionStatus::ENABLED;
}

void FlushBlacklistPolicy() {
  // Updates of the URLBlacklist are done on IO, after building the blacklist
  // on the blocking pool, which is initiated from IO.
  content::RunAllPendingInMessageLoop(BrowserThread::IO);
  content::RunAllTasksUntilIdle();
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

bool ContainsWebstoreTile(content::RenderFrameHost* iframe) {
  int num_webstore_tiles = 0;
  EXPECT_TRUE(instant_test_utils::GetIntFromJS(
      iframe,
      "document.querySelectorAll(\".md-tile[href='" +
          l10n_util::GetStringUTF8(IDS_WEBSTORE_URL) + "']\").length",
      &num_webstore_tiles));
  return num_webstore_tiles == 1;
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
  void OnOutputMuteChanged(bool /* mute_on */) override {
    ++output_mute_changed_count_;
  }

 private:
  int output_mute_changed_count_;

  DISALLOW_COPY_AND_ASSIGN(TestAudioObserver);
};
#endif

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

#if !defined(OS_CHROMEOS)
extensions::MessagingDelegate::PolicyPermission IsNativeMessagingHostAllowed(
    content::BrowserContext* browser_context,
    const std::string& native_host_name) {
  extensions::MessagingDelegate* messaging_delegate =
      extensions::ExtensionsAPIClient::Get()->GetMessagingDelegate();
  EXPECT_NE(messaging_delegate, nullptr);
  return messaging_delegate->IsNativeMessagingHostAllowed(browser_context,
                                                          native_host_name);
}
#endif

}  // namespace

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
                 std::make_unique<base::Value>("fr"), nullptr);
    provider_.UpdateChromePolicy(policies);
    // The "en-US" ResourceBundle is always loaded before this step for tests,
    // but in this test we want the browser to load the bundle as it
    // normally would.
    ui::ResourceBundle::CleanupSharedInstance();
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
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string loaded =
      ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources("en-US");
  EXPECT_EQ("en-US", loaded);
  base::string16 english_title = l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);
  EXPECT_NE(french_title, english_title);
}
#endif

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(LoginPolicyTestBase, PRE_AllowedLanguages) {
  SkipToLoginScreen();
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  Profile* const profile = GetProfileForActiveUser();
  PrefService* prefs = profile->GetPrefs();

  // Set locale and preferred languages to "en-US".
  prefs->SetString(language::prefs::kApplicationLocale, "en-US");
  prefs->SetString(language::prefs::kPreferredLanguages, "en-US");

  // Set policy to only allow "fr" as locale.
  std::unique_ptr<base::DictionaryValue> policy =
      std::make_unique<base::DictionaryValue>();
  base::ListValue allowed_languages;
  allowed_languages.AppendString("fr");
  policy->SetKey(key::kAllowedLanguages, std::move(allowed_languages));
  user_policy_helper()->SetPolicyAndWait(*policy, base::DictionaryValue(),
                                         profile);
}

IN_PROC_BROWSER_TEST_F(LoginPolicyTestBase, AllowedLanguages) {
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  Profile* const profile = GetProfileForActiveUser();
  const PrefService* prefs = profile->GetPrefs();

  // Verifies that the default locale has been overridden by policy
  // (see |GetMandatoryPoliciesValue|)
  Browser* browser = CreateBrowser(profile);
  EXPECT_EQ("fr", prefs->GetString(language::prefs::kApplicationLocale));
  ui_test_utils::NavigateToURL(browser, GURL(chrome::kChromeUINewTabURL));
  base::string16 french_title = l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);
  base::string16 title;
  EXPECT_TRUE(ui_test_utils::GetCurrentTabTitle(browser, &title));
  EXPECT_EQ(french_title, title);

  // Make sure this is really French and differs from the English title.
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string loaded =
      ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources("en-US");
  EXPECT_EQ("en-US", loaded);
  base::string16 english_title = l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);
  EXPECT_NE(french_title, english_title);

  // Verifiy that the enforced locale is added into the list of
  // preferred languages.
  EXPECT_EQ("fr", prefs->GetString(language::prefs::kPreferredLanguages));
}

IN_PROC_BROWSER_TEST_F(LoginPolicyTestBase, AllowedInputMethods) {
  SkipToLoginScreen();
  LogIn(kAccountId, kAccountPassword, kEmptyServices);

  Profile* const profile = GetProfileForActiveUser();

  chromeos::input_method::InputMethodManager* imm =
      chromeos::input_method::InputMethodManager::Get();
  ASSERT_TRUE(imm);
  scoped_refptr<chromeos::input_method::InputMethodManager::State> ime_state =
      imm->GetActiveIMEState();
  ASSERT_TRUE(ime_state.get());

  std::vector<std::string> input_methods;
  input_methods.emplace_back("xkb:us::eng");
  input_methods.emplace_back("xkb:fr::fra");
  input_methods.emplace_back("xkb:de::ger");
  EXPECT_TRUE(imm->MigrateInputMethods(&input_methods));

  // No restrictions and current input method should be "xkb:us::eng" (default).
  EXPECT_EQ(0U, ime_state->GetAllowedInputMethods().size());
  EXPECT_EQ(input_methods[0], ime_state->GetCurrentInputMethod().id());
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[1]));
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[2]));

  // Set policy to only allow "xkb:fr::fra", "xkb:de::ger" an an invalid value
  // as input method.
  std::unique_ptr<base::DictionaryValue> policy =
      std::make_unique<base::DictionaryValue>();
  base::ListValue allowed_input_methods;
  allowed_input_methods.AppendString("xkb:fr::fra");
  allowed_input_methods.AppendString("xkb:de::ger");
  allowed_input_methods.AppendString("invalid_value_will_be_ignored");
  policy->SetKey(key::kAllowedInputMethods, std::move(allowed_input_methods));
  user_policy_helper()->SetPolicyAndWait(*policy, base::DictionaryValue(),
                                         profile);

  // Only "xkb:fr::fra", "xkb:de::ger" should be allowed, current input method
  // should be "xkb:fr::fra", enabling "xkb:us::eng" should not be possible,
  // enabling "xkb:de::ger" should be possible.
  EXPECT_EQ(2U, ime_state->GetAllowedInputMethods().size());
  EXPECT_EQ(2U, ime_state->GetActiveInputMethods()->size());
  EXPECT_EQ(input_methods[1], ime_state->GetCurrentInputMethod().id());
  EXPECT_FALSE(ime_state->EnableInputMethod(input_methods[0]));
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[2]));

  // Set policy to only allow an invalid value as input method.
  std::unique_ptr<base::DictionaryValue> policy_invalid =
      std::make_unique<base::DictionaryValue>();
  base::ListValue invalid_input_methods;
  invalid_input_methods.AppendString("invalid_value_will_be_ignored");
  policy_invalid->SetKey(key::kAllowedInputMethods,
                         std::move(invalid_input_methods));
  user_policy_helper()->SetPolicyAndWait(*policy_invalid,
                                         base::DictionaryValue(), profile);

  // No restrictions and current input method should still be "xkb:fr::fra".
  EXPECT_EQ(0U, ime_state->GetAllowedInputMethods().size());
  EXPECT_EQ(input_methods[1], ime_state->GetCurrentInputMethod().id());
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[0]));
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[2]));

  // Allow all input methods again.
  user_policy_helper()->SetPolicyAndWait(base::DictionaryValue(),
                                         base::DictionaryValue(), profile);

  // No restrictions and current input method should still be "xkb:fr::fra".
  EXPECT_EQ(0U, ime_state->GetAllowedInputMethods().size());
  EXPECT_EQ(input_methods[1], ime_state->GetCurrentInputMethod().id());
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[0]));
  EXPECT_TRUE(ime_state->EnableInputMethod(input_methods[2]));
}

class StartupBrowserWindowLaunchSuppressedTest : public LoginPolicyTestBase {
 public:
  StartupBrowserWindowLaunchSuppressedTest() = default;

  void SetUpPolicy(bool enabled) {
    std::unique_ptr<base::DictionaryValue> policy =
        std::make_unique<base::DictionaryValue>();

    policy->SetKey(key::kStartupBrowserWindowLaunchSuppressed,
                   base::Value(enabled));

    user_policy_helper()->SetPolicy(*policy, base::DictionaryValue());
  }

  void CheckLaunchedBrowserCount(unsigned int count) {
    SkipToLoginScreen();
    LogIn(kAccountId, kAccountPassword, kEmptyServices);

    Profile* const profile = GetProfileForActiveUser();

    ASSERT_EQ(count, chrome::GetBrowserCount(profile));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StartupBrowserWindowLaunchSuppressedTest);
};

// Test that the browser window is not launched when
// StartupBrowserWindowLaunchSuppressed is set to true.
IN_PROC_BROWSER_TEST_F(StartupBrowserWindowLaunchSuppressedTest,
                       TrueDoesNotAllowBrowserWindowLaunch) {
  SetUpPolicy(true);
  CheckLaunchedBrowserCount(0u);
}

// Test that the browser window is launched when
// StartupBrowserWindowLaunchSuppressed is set to false.
IN_PROC_BROWSER_TEST_F(StartupBrowserWindowLaunchSuppressedTest,
                       FalseAllowsBrowserWindowLaunch) {
  SetUpPolicy(false);
  CheckLaunchedBrowserCount(1u);
}

class PrimaryUserPoliciesProxiedTest : public LoginPolicyTestBase {
 public:
  PrimaryUserPoliciesProxiedTest() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrimaryUserPoliciesProxiedTest);
};

IN_PROC_BROWSER_TEST_F(PrimaryUserPoliciesProxiedTest,
                       AvailableInLocalStateEarly) {
  PolicyService* const device_wide_policy_service =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetPolicyService();

  // Sanity check default state without a policy active.
  EXPECT_FALSE(device_wide_policy_service
                   ->GetPolicies(PolicyNamespace(
                       POLICY_DOMAIN_CHROME, std::string() /* component_id */))
                   .GetValue(key::kAudioOutputAllowed));
  const PrefService::Preference* pref =
      g_browser_process->local_state()->FindPreference(
          chromeos::prefs::kAudioOutputAllowed);
  EXPECT_FALSE(pref->IsManaged());
  EXPECT_TRUE(pref->GetValue()->GetBool());

  base::DictionaryValue policy;
  policy.SetKey(key::kAudioOutputAllowed, base::Value(false));
  user_policy_helper()->SetPolicy(policy, base::DictionaryValue());

  SkipToLoginScreen();

  content::WindowedNotificationObserver profile_created_observer(
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::NotificationService::AllSources());
  TriggerLogIn(kAccountId, kAccountPassword, kEmptyServices);
  profile_created_observer.Wait();

  const base::Value* policy_value =
      device_wide_policy_service
          ->GetPolicies(PolicyNamespace(POLICY_DOMAIN_CHROME,
                                        std::string() /* component_id */))
          .GetValue(key::kAudioOutputAllowed);
  ASSERT_TRUE(policy_value);
  EXPECT_FALSE(policy_value->GetBool());

  EXPECT_TRUE(pref->IsManaged());
  EXPECT_FALSE(pref->GetValue()->GetBool());

  // Make sure that session startup finishes before letting chrome exit.
  // Rationale: We've seen CHECK-failures when exiting chrome right after
  // NOTIFICATION_PROFILE_CREATED, see e.g. https://crbug.com/1002066 .
  chromeos::test::WaitForPrimaryUserSessionStart();
}

#endif  // defined(OS_CHROMEOS)

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
               std::make_unique<base::Value>(true), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(bookmarks::prefs::kShowBookmarkBar));
  EXPECT_TRUE(prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar));
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DefaultSearchProvider) {
  // Verifies that a default search is made using the provider configured via
  // policy. Also checks that default search can be completely disabled.
  const base::string16 kKeyword(base::ASCIIToUTF16("testsearch"));
  const std::string kSearchURL("http://search.example/search?q={searchTerms}");
  const std::string kAlternateURL0(
      "http://search.example/search#q={searchTerms}");
  const std::string kAlternateURL1("http://search.example/#q={searchTerms}");
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
  EXPECT_FALSE(
    default_search->alternate_urls().size() == 2 &&
    default_search->alternate_urls()[0] == kAlternateURL0 &&
    default_search->alternate_urls()[1] == kAlternateURL1 &&
    default_search->image_url() == kImageURL &&
    default_search->image_url_post_params() == kImageURLPostParams &&
    default_search->new_tab_url() == kNewTabURL);

  // Override the default search provider using policies.
  PolicyMap policies;
  policies.Set(key::kDefaultSearchProviderEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(true), nullptr);
  policies.Set(key::kDefaultSearchProviderKeyword, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(kKeyword), nullptr);
  policies.Set(key::kDefaultSearchProviderSearchURL, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(kSearchURL), nullptr);
  std::unique_ptr<base::ListValue> alternate_urls(new base::ListValue);
  alternate_urls->AppendString(kAlternateURL0);
  alternate_urls->AppendString(kAlternateURL1);
  policies.Set(key::kDefaultSearchProviderAlternateURLs, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::move(alternate_urls), nullptr);
  policies.Set(key::kDefaultSearchProviderImageURL, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(kImageURL), nullptr);
  policies.Set(key::kDefaultSearchProviderImageURLPostParams,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(kImageURLPostParams), nullptr);
  policies.Set(key::kDefaultSearchProviderNewTabURL, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(kNewTabURL), nullptr);
  UpdateProviderPolicy(policies);
  default_search = service->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);
  EXPECT_EQ(kKeyword, default_search->keyword());
  EXPECT_EQ(kSearchURL, default_search->url());
  EXPECT_EQ(2U, default_search->alternate_urls().size());
  EXPECT_EQ(kAlternateURL0, default_search->alternate_urls()[0]);
  EXPECT_EQ(kAlternateURL1, default_search->alternate_urls()[1]);
  EXPECT_EQ(kImageURL, default_search->image_url());
  EXPECT_EQ(kImageURLPostParams, default_search->image_url_post_params());
  EXPECT_EQ(kNewTabURL, default_search->new_tab_url());

  // Verify that searching from the omnibox uses kSearchURL.
  chrome::FocusLocationBar(browser());
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "stuff to search for");
  OmniboxEditModel* model =
      browser()->window()->GetLocationBar()->GetOmniboxView()->model();
  EXPECT_TRUE(model->CurrentMatch(NULL).destination_url.is_valid());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL expected("http://search.example/search?q=stuff+to+search+for");
  EXPECT_EQ(expected, web_contents->GetURL());

  // Verify that searching from the omnibox can be disabled.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  policies.Set(key::kDefaultSearchProviderEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(false), nullptr);
  EXPECT_TRUE(service->GetDefaultSearchProvider());
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(service->GetDefaultSearchProvider());
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "should not work");
  // This means that submitting won't trigger any action.
  EXPECT_FALSE(model->CurrentMatch(NULL).destination_url.is_valid());
  EXPECT_EQ(GURL(url::kAboutBlankURL), web_contents->GetURL());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, SeparateProxyPoliciesMerging) {
  // Add an individual proxy policy value.
  PolicyMap policies;
  policies.Set(key::kProxyServerMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(3), nullptr);
  UpdateProviderPolicy(policies);

  // It should be removed and replaced with a dictionary.
  PolicyMap expected;
  std::unique_ptr<base::DictionaryValue> expected_value(
      new base::DictionaryValue);
  expected_value->SetInteger(key::kProxyServerMode, 3);
  expected.Set(key::kProxySettings, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, std::move(expected_value), nullptr);
#if defined(OS_CHROMEOS)
  SetEnterpriseUsersDefaults(&expected);
#endif

  // Check both the browser and the profile.
  const PolicyMap& actual_from_browser =
      g_browser_process->browser_policy_connector()
          ->GetPolicyService()
          ->GetPolicies(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  EXPECT_TRUE(expected.Equals(actual_from_browser));
  const PolicyMap& actual_from_profile =
      browser()
          ->profile()
          ->GetProfilePolicyConnector()
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
            : std::make_unique<base::Value>(legacy_safe_search == 1),
        google_safe_search == 0
            ? nullptr
            : std::make_unique<base::Value>(google_safe_search == 1),
        legacy_youtube == 0
            ? nullptr
            : std::make_unique<base::Value>(legacy_youtube == 1),
        youtube_restrict == 0
            ? nullptr  // subtracting 1 gives 0,1,2, see above
            : std::make_unique<base::Value>(youtube_restrict - 1));

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
  base::Lock lock;
  std::set<GURL> google_urls_requested;
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) -> bool {
        if (params->url_request.url.host() != "google.com")
          return false;
        base::AutoLock auto_lock(lock);
        google_urls_requested.insert(params->url_request.url);
        std::string relative_path("chrome/test/data/simple.html");
        content::URLLoaderInterceptor::WriteResponse(relative_path,
                                                     params->client.get());
        return true;
      }));

  // Verifies that requests to Google Search engine with the SafeSearch
  // enabled set the safe=active&ssui=on parameters at the end of the query.
  // First check that nothing happens.
  CheckSafeSearch(browser(), false);

  // Go over all combinations of (undefined, true, false) for the
  // ForceGoogleSafeSearch policy.
  for (int safe_search = 0; safe_search < 3; safe_search++) {
    // Override the Google safe search policy.
    ApplySafeSearchPolicy(nullptr,          // ForceSafeSearch
                          safe_search == 0  // ForceGoogleSafeSearch
                              ? nullptr
                              : std::make_unique<base::Value>(safe_search == 1),
                          nullptr,   // ForceYouTubeSafetyMode
                          nullptr);  // ForceYouTubeRestrict
    // Verify that the safe search pref behaves the way we expect.
    PrefService* prefs = browser()->profile()->GetPrefs();
    EXPECT_EQ(safe_search != 0,
              prefs->IsManagedPreference(prefs::kForceGoogleSafeSearch));
    EXPECT_EQ(safe_search == 1,
              prefs->GetBoolean(prefs::kForceGoogleSafeSearch));

    // Verify that safe search actually works.
    CheckSafeSearch(browser(), safe_search == 1);

    GURL google_url(GetExpectedSearchURL(safe_search == 1));

    {
      // Verify that the network request is what we expect.
      base::AutoLock auto_lock(lock);
      ASSERT_TRUE(google_urls_requested.find(google_url) !=
                  google_urls_requested.end());
      google_urls_requested.clear();
    }

    {
      // Now check subresource loads.
      FetchSubresource(browser()->tab_strip_model()->GetActiveWebContents(),
                       GURL("http://google.com/"));

      base::AutoLock auto_lock(lock);
      ASSERT_TRUE(google_urls_requested.find(google_url) !=
                  google_urls_requested.end());
    }
  }
}

class PolicyTestSafeSearchRedirect : public PolicyTest {
 public:
  PolicyTestSafeSearchRedirect() = default;

 private:
  void SetUpOnMainThread() override {
    // The test makes requests to google.com which we want to redirect to the
    // test server.
    host_resolver()->AddRule("*", "127.0.0.1");

    // The production code only allows known ports (80 for http and 443 for
    // https), but the test server runs on a random port.
    google_util::IgnorePortNumbersForGoogleURLChecksForTesting();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load pages from "www.google.com" without an interstitial.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  DISALLOW_COPY_AND_ASSIGN(PolicyTestSafeSearchRedirect);
};

IN_PROC_BROWSER_TEST_F(PolicyTestSafeSearchRedirect, ForceGoogleSafeSearch) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  ApplySafeSearchPolicy(nullptr,  // ForceSafeSearch
                        std::make_unique<base::Value>(true),
                        nullptr,   // ForceYouTubeSafetyMode
                        nullptr);  // ForceYouTubeRestrict

  GURL url = https_server.GetURL("www.google.com",
                                 "/server-redirect?http://google.com/");
  CheckSafeSearch(browser(), true, url.spec());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, ForceYouTubeRestrict) {
  base::Lock lock;
  std::map<GURL, net::HttpRequestHeaders> urls_requested;
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) -> bool {
        if (params->url_request.url.host() != "youtube.com")
          return false;

        base::AutoLock auto_lock(lock);
        urls_requested[params->url_request.url] = params->url_request.headers;

        std::string relative_path("chrome/test/data/simple.html");
        content::URLLoaderInterceptor::WriteResponse(relative_path,
                                                     params->client.get());
        return true;
      }));

  for (int youtube_restrict_mode = safe_search_util::YOUTUBE_RESTRICT_OFF;
       youtube_restrict_mode < safe_search_util::YOUTUBE_RESTRICT_COUNT;
       ++youtube_restrict_mode) {
    ApplySafeSearchPolicy(nullptr,  // ForceSafeSearch
                          nullptr,  // ForceGoogleSafeSearch
                          nullptr,  // ForceYouTubeSafetyMode
                          std::make_unique<base::Value>(youtube_restrict_mode));
    {
      // First check frame requests.
      GURL youtube_url("http://youtube.com");
      ui_test_utils::NavigateToURL(browser(), youtube_url);

      base::AutoLock auto_lock(lock);
      CheckYouTubeRestricted(youtube_restrict_mode, urls_requested,
                             youtube_url);
    }

    {
      // Now check subresource loads.
      GURL youtube_script("http://youtube.com/foo.js");
      FetchSubresource(browser()->tab_strip_model()->GetActiveWebContents(),
                       youtube_script);

      base::AutoLock auto_lock(lock);
      CheckYouTubeRestricted(youtube_restrict_mode, urls_requested,
                             youtube_script);
    }
  }
}

IN_PROC_BROWSER_TEST_F(PolicyTest, AllowedDomainsForApps) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::Lock lock;
  std::map<GURL, net::HttpRequestHeaders> urls_requested;
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) -> bool {
        base::AutoLock auto_lock(lock);
        urls_requested[params->url_request.url] =
            params->url_request.cors_exempt_headers;
        return false;
      }));

  for (int allowed_domains = 0; allowed_domains < 2; ++allowed_domains) {
    std::string allowed_domain;
    if (allowed_domains) {
      PolicyMap policies;
      allowed_domain = "foo.com";
      SetPolicy(&policies, key::kAllowedDomainsForApps,
                std::make_unique<base::Value>(allowed_domain));
      UpdateProviderPolicy(policies);
    }

    {
      // First check frame requests.
      GURL google_url =
          embedded_test_server()->GetURL("google.com", "/empty.html");
      ui_test_utils::NavigateToURL(browser(), google_url);

      base::AutoLock auto_lock(lock);
      CheckAllowedDomainsHeader(allowed_domain, urls_requested, google_url);
    }

    {
      // Now check subresource loads.
      GURL google_script =
          embedded_test_server()->GetURL("google.com", "/result_queue.js");

      FetchSubresource(browser()->tab_strip_model()->GetActiveWebContents(),
                       google_script);

      base::AutoLock auto_lock(lock);
      CheckAllowedDomainsHeader(allowed_domain, urls_requested, google_script);
    }

    {
      // Double check that a frame to a non-Google url doesn't have the header.
      GURL non_google_url = embedded_test_server()->GetURL("/empty.html");
      ui_test_utils::NavigateToURL(browser(), non_google_url);

      base::AutoLock auto_lock(lock);
      CheckAllowedDomainsHeader(std::string(), urls_requested, non_google_url);
    }
  }
}

// This test is flaky on Windows 10: https://crbug.com/1069558
#if defined(OS_WIN)
#define MAYBE_Disable3DAPIs DISABLED_Disable3DAPIs
#else
#define MAYBE_Disable3DAPIs Disable3DAPIs
#endif
IN_PROC_BROWSER_TEST_F(PolicyTest, MAYBE_Disable3DAPIs) {
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
               POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(true),
               nullptr);
  UpdateProviderPolicy(policies);
  // Crash and reload the tab to get a new renderer.
  content::CrashTab(contents);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_RELOAD));
  EXPECT_FALSE(IsWebGLEnabled(contents));
  // Enable with a policy.
  policies.Set(key::kDisable3DAPIs, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(false),
               nullptr);
  UpdateProviderPolicy(policies);
  content::CrashTab(contents);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_RELOAD));
  EXPECT_TRUE(IsWebGLEnabled(contents));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DeveloperToolsDisabledByLegacyPolicy) {
  // Verifies that access to the developer tools can be disabled by setting the
  // legacy DeveloperToolsDisabled policy.

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
               std::make_unique<base::Value>(true), nullptr);
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

IN_PROC_BROWSER_TEST_F(PolicyTest,
                       DeveloperToolsDisabledByDeveloperToolsAvailability) {
  // Verifies that access to the developer tools can be disabled by setting the
  // DeveloperToolsAvailability policy.

  // Open devtools.
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_DEV_TOOLS));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DevToolsWindow* devtools_window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);
  EXPECT_TRUE(devtools_window);

  // Disable devtools via policy.
  PolicyMap policies;
  policies.Set(key::kDeveloperToolsAvailability, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(2 /* DeveloperToolsDisallowed */),
               nullptr);
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
  // in chrome://extensions is actively turned off and the checkbox
  // is disabled.
  // Note: We don't test the indicator as it is tested in the policy pref test
  // for kDeveloperToolsDisabled and kDeveloperToolsAvailability.

  // This test depends on the following helper methods to locate the DOM elemens
  // to be tested.
  const char define_helpers_js[] =
      R"(function getToolbar() {
           const manager = document.querySelector('extensions-manager');
           return manager.$$('extensions-toolbar');
         }

         function getToggle() {
           return getToolbar().$.devMode;
         }

         function getControls() {
           return getToolbar().$.devDrawer;
         }
        )";

  const char toggle_dev_mode_accessor_js[] = "getToggle()";
  const char dev_controls_accessor_js[] = "getControls()";
  const char dev_controls_visibility_check_js[] =
      "getControls().hasAttribute('expanded')";

  // Navigate to the extensions frame and enabled "Developer mode"
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIExtensionsURL));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScript(contents, std::string(define_helpers_js)));

  EXPECT_TRUE(content::ExecuteScript(
      contents, base::StringPrintf("domAutomationController.send(%s.click());",
                                   toggle_dev_mode_accessor_js)));

  WaitForExtensionsDevModeControlsVisibility(contents, dev_controls_accessor_js,
                                             dev_controls_visibility_check_js,
                                             true);

  // Disable devtools via policy.
  PolicyMap policies;
  policies.Set(key::kDeveloperToolsAvailability, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(2 /*DeveloperToolsDisallowed*/),
               nullptr);
  UpdateProviderPolicy(policies);

  // Expect devcontrols to be hidden now...
  WaitForExtensionsDevModeControlsVisibility(contents, dev_controls_accessor_js,
                                             dev_controls_visibility_check_js,
                                             false);

  // ... and checkbox is disabled
  bool is_toggle_dev_mode_checkbox_disabled = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      contents,
      base::StringPrintf(
          "domAutomationController.send(%s.hasAttribute('disabled'))",
          toggle_dev_mode_accessor_js),
      &is_toggle_dev_mode_checkbox_disabled));
  EXPECT_TRUE(is_toggle_dev_mode_checkbox_disabled);
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DownloadDirectory) {
  // Verifies that the download directory can be forced by policy.

  // Don't prompt for the download location during this test.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPromptForDownload, false);

  base::FilePath initial_dir =
      DownloadPrefs(browser()->profile()).DownloadPath();

  // Verify that downloads end up on the default directory.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  DownloadAndVerifyFile(browser(), initial_dir, file);
  base::DieFileDie(initial_dir.Append(file), false);

  // Override the download directory with the policy and verify a download.
  base::FilePath forced_dir = initial_dir.AppendASCII("forced");

  PolicyMap policies;
  policies.Set(key::kDownloadDirectory, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(forced_dir.value()), nullptr);
  UpdateProviderPolicy(policies);
  DownloadAndVerifyFile(browser(), forced_dir, file);
  // Verify that the first download location wasn't affected.
  EXPECT_FALSE(base::PathExists(initial_dir.Append(file)));
}

#if defined(OS_CHROMEOS)
// Verifies that the download directory can be forced to Google Drive by policy.
IN_PROC_BROWSER_TEST_F(PolicyTest, DownloadDirectory_Drive) {
  // Override the download directory with the policy.
  {
    PolicyMap policies;
    policies.Set(key::kDownloadDirectory, POLICY_LEVEL_RECOMMENDED,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>("${google_drive}/"), nullptr);
    UpdateProviderPolicy(policies);

    EXPECT_EQ(drive::DriveIntegrationServiceFactory::FindForProfile(
                  browser()->profile())
                  ->GetMountPointPath()
                  .AppendASCII("root"),
              DownloadPrefs(browser()->profile())
                  .DownloadPath()
                  .StripTrailingSeparators());
  }

  PolicyMap policies;
  policies.Set(key::kDownloadDirectory, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>("${google_drive}/Downloads"),
               nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_EQ(drive::DriveIntegrationServiceFactory::FindForProfile(
                browser()->profile())
                ->GetMountPointPath()
                .AppendASCII("root/Downloads"),
            DownloadPrefs(browser()->profile())
                .DownloadPath()
                .StripTrailingSeparators());
}
#endif  // !defined(OS_CHROMEOS)

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
               std::make_unique<base::Value>(chrome::kChromeUICreditsURL),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_HOME));
  content::WaitForLoadStop(contents);
  EXPECT_EQ(GURL(chrome::kChromeUICreditsURL), contents->GetURL());

  policies.Set(key::kHomepageIsNewTabPage, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(true), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_HOME));
  content::WaitForLoadStop(contents);
  EXPECT_TRUE(search::IsInstantNTP(contents));
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
               std::make_unique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(chrome::ExecuteCommand(browser(), IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_EQ(1u, active_browser_list->size());
  EXPECT_FALSE(BrowserList::IsIncognitoSessionActive());

  // Enable via policy and verify that incognito windows can be opened.
  policies.Set(key::kIncognitoEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(true), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_EQ(2u, active_browser_list->size());
  EXPECT_TRUE(BrowserList::IsIncognitoSessionActive());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, NetworkPrediction) {
  PrefService* prefs = browser()->profile()->GetPrefs();

  // Enabled by default.
  EXPECT_TRUE(IsNetworkPredictionEnabled(prefs));

  // Disable by old, deprecated policy.
  PolicyMap policies;
  policies.Set(key::kDnsPrefetchingEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_FALSE(IsNetworkPredictionEnabled(prefs));

  // Enabled by new policy, this should override old one.
  policies.Set(key::kNetworkPredictionOptions, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(
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
               std::make_unique<base::Value>(true), nullptr);
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
               std::make_unique<base::Value>(false), nullptr);
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
               std::make_unique<base::Value>(true), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kAllowDeletingBrowserHistory));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory));

  EXPECT_TRUE(prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistory));
  EXPECT_TRUE(prefs->GetBoolean(browsing_data::prefs::kDeleteDownloadHistory));
  EXPECT_TRUE(
      prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistoryBasic));

  policies.Set(key::kAllowDeletingBrowserHistory, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(false), nullptr);
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

IN_PROC_BROWSER_TEST_F(PolicyTest, URLBlacklist) {
  // Checks that URLs can be blacklisted, and that exceptions can be made to
  // the blacklist.

  ASSERT_TRUE(embedded_test_server()->Start());

  const std::string kURLS[] = {
      embedded_test_server()->GetURL("aaa.com", "/empty.html").spec(),
      embedded_test_server()->GetURL("bbb.com", "/empty.html").spec(),
      embedded_test_server()->GetURL("sub.bbb.com", "/empty.html").spec(),
      embedded_test_server()->GetURL("bbb.com", "/policy/blank.html").spec(),
      embedded_test_server()->GetURL("bbb.com.", "/policy/blank.html").spec(),
  };

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
  for (size_t i = 1; i < base::size(kURLS); ++i)
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
}

IN_PROC_BROWSER_TEST_F(PolicyTest, URLBlacklistIncognito) {
  // Checks that URLs can be blacklisted, and that exceptions can be made to
  // the blacklist.

  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  ASSERT_TRUE(embedded_test_server()->Start());

  const std::string kURLS[] = {
      embedded_test_server()->GetURL("aaa.com", "/empty.html").spec(),
      embedded_test_server()->GetURL("bbb.com", "/empty.html").spec(),
      embedded_test_server()->GetURL("sub.bbb.com", "/empty.html").spec(),
      embedded_test_server()->GetURL("bbb.com", "/policy/blank.html").spec(),
      embedded_test_server()->GetURL("bbb.com.", "/policy/blank.html").spec(),
  };

  // Verify that "bbb.com" opens before applying the blacklist.
  CheckCanOpenURL(incognito_browser, kURLS[1]);

  // Set a blacklist.
  base::ListValue blacklist;
  blacklist.AppendString("bbb.com");
  PolicyMap policies;
  policies.Set(key::kURLBlacklist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, blacklist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();
  // All bbb.com URLs are blocked, and "aaa.com" is still unblocked.
  CheckCanOpenURL(incognito_browser, kURLS[0]);
  for (size_t i = 1; i < base::size(kURLS); ++i)
    CheckURLIsBlocked(incognito_browser, kURLS[i]);

  // Whitelist some sites of bbb.com.
  base::ListValue whitelist;
  whitelist.AppendString("sub.bbb.com");
  whitelist.AppendString("bbb.com/policy");
  policies.Set(key::kURLWhitelist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, whitelist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();
  CheckURLIsBlocked(incognito_browser, kURLS[1]);
  CheckCanOpenURL(incognito_browser, kURLS[2]);
  CheckCanOpenURL(incognito_browser, kURLS[3]);
  CheckCanOpenURL(incognito_browser, kURLS[4]);
}

IN_PROC_BROWSER_TEST_F(PolicyTest, URLBlacklistAndWhitelist) {
  // Regression test for http://crbug.com/755256. Blacklisting * and
  // whitelisting an origin should work.

  ASSERT_TRUE(embedded_test_server()->Start());

  base::ListValue blacklist;
  blacklist.AppendString("*");
  PolicyMap policies;
  policies.Set(key::kURLBlacklist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, blacklist.CreateDeepCopy(), nullptr);

  base::ListValue whitelist;
  whitelist.AppendString("aaa.com");
  policies.Set(key::kURLWhitelist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, whitelist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();
  CheckCanOpenURL(
      browser(),
      embedded_test_server()->GetURL("aaa.com", "/empty.html").spec());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, URLBlacklistSubresources) {
  // Checks that an image with a blacklisted URL is loaded, but an iframe with a
  // blacklisted URL is not.

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL main_url =
      embedded_test_server()->GetURL("/policy/blacklist-subresources.html");
  GURL image_url = embedded_test_server()->GetURL("/policy/pixel.png");
  GURL subframe_url = embedded_test_server()->GetURL("/policy/blank.html");

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

IN_PROC_BROWSER_TEST_F(PolicyTest, URLBlacklistClientRedirect) {
  // Checks that a client side redirect to a blacklisted URL is blocked.
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL redirected_url =
      embedded_test_server()->GetURL("/policy/blacklist-redirect.html");
  GURL first_url = embedded_test_server()->GetURL("/client-redirect?" +
                                                  redirected_url.spec());

  // There are two navigations: one when loading client-redirect.html and
  // another when the document redirects using http-equiv="refresh".
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                            first_url, 2);
  EXPECT_EQ(base::ASCIIToUTF16("Redirected!"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

  base::ListValue blacklist;
  blacklist.AppendString(redirected_url.spec().c_str());
  PolicyMap policies;
  policies.Set(key::kURLBlacklist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, blacklist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();

  ui_test_utils::NavigateToURL(browser(), first_url);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_NE(base::ASCIIToUTF16("Redirected!"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, URLBlacklistServerRedirect) {
  // Checks that a server side redirect to a blacklisted URL is blocked.
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL redirected_url =
      embedded_test_server()->GetURL("/policy/blacklist-redirect.html");
  GURL first_url = embedded_test_server()->GetURL("/server-redirect?" +
                                                  redirected_url.spec());

  ui_test_utils::NavigateToURL(browser(), first_url);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(base::ASCIIToUTF16("Redirected!"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

  base::ListValue blacklist;
  blacklist.AppendString(redirected_url.spec().c_str());
  PolicyMap policies;
  policies.Set(key::kURLBlacklist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, blacklist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();

  ui_test_utils::NavigateToURL(browser(), first_url);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_NE(base::ASCIIToUTF16("Redirected!"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
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
  GetTestDataDirectory(&test_path);
  const std::string base_path = "file://" + test_path.AsUTF8Unsafe() +"/";
  const std::string folder_path = base_path + "apptest/";
  const std::string file_path1 = base_path + "title1.html";
  const std::string file_path2 = folder_path + "basic.html";

  CheckCanOpenURL(browser(), file_path1);
  CheckCanOpenURL(browser(), file_path2);

  // Set a blacklist for all the files.
  base::ListValue blacklist;
  blacklist.AppendString("file://*");
  PolicyMap policies;
  policies.Set(key::kURLBlacklist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, blacklist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();

  CheckURLIsBlocked(browser(), file_path1);
  CheckURLIsBlocked(browser(), file_path2);

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

  CheckCanOpenURL(browser(), file_path1);
  CheckURLIsBlocked(browser(), file_path2);
}

IN_PROC_BROWSER_TEST_F(PolicyTest, UrlKeyedAnonymizedDataCollection) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  EXPECT_TRUE(prefs->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));

  // Disable by policy.
  PolicyMap policies;
  policies.Set(key::kUrlKeyedAnonymizedDataCollectionEnabled,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_FALSE(prefs->GetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
}

#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(PolicyTest, FullscreenAllowedBrowser) {
  PolicyMap policies;
  policies.Set(key::kFullscreenAllowed, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(false), nullptr);
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
               std::make_unique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);

  scoped_refptr<const extensions::Extension> extension =
      LoadUnpackedExtension(kUnpackedFullscreenAppName);
  ASSERT_TRUE(extension);

  // Launch an app that tries to open a fullscreen window.
  TestAddAppWindowObserver add_window_observer(
      extensions::AppWindowRegistry::Get(browser()->profile()));
  apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
      ->BrowserAppLauncher()
      .LaunchAppWithParams(apps::AppLaunchParams(
          extension->id(), apps::mojom::LaunchContainer::kLaunchContainerNone,
          WindowOpenDisposition::NEW_WINDOW,
          apps::mojom::AppLaunchSource::kSourceTest));
  extensions::AppWindow* window = add_window_observer.WaitForAppWindow();
  ASSERT_TRUE(window);

  // Verify that the window is not in fullscreen mode.
  EXPECT_FALSE(window->GetBaseWindow()->IsFullscreen());

  // We have to wait for the navigation to commit since the JS object
  // registration is delayed (see AppWindowCreateFunction::RunAsync).
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
               std::make_unique<base::Value>(false), nullptr);
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
               std::make_unique<base::Value>(true), nullptr);
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
               std::make_unique<base::Value>(kThreeHoursInMs), nullptr);
  UpdateProviderPolicy(policies);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Decrease the session length limit to 1 hour. Verify that the session is
  // terminated immediately.
  EXPECT_CALL(observer, Observe(chrome::NOTIFICATION_APP_TERMINATING, _, _));
  policies.Set(key::kSessionLengthLimit, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(kOneHourInMs), nullptr);
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
               std::make_unique<base::Value>(true), nullptr);
  policies.Set(key::kSessionLengthLimit, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(kThreeHoursInMs), nullptr);
  UpdateProviderPolicy(policies);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Decrease the session length limit to 1 hour. Verify that the session is
  // terminated immediately.
  EXPECT_CALL(observer, Observe(chrome::NOTIFICATION_APP_TERMINATING, _, _));
  policies.Set(key::kSessionLengthLimit, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(kOneHourInMs), nullptr);
  UpdateProviderPolicy(policies);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);
}

#endif  // defined(OS_CHROMEOS)

namespace {

constexpr const char* kRestoredURLs[] = {
    "http://aaa.com/empty.html", "http://bbb.com/empty.html",
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
  RestoreOnStartupPolicyTest() = default;
  virtual ~RestoreOnStartupPolicyTest() = default;

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
    base::EraseIf(argv, IsNonSwitchArgument);
    command_line->InitFromArgv(argv);
    ASSERT_TRUE(std::equal(argv.begin(), argv.end(),
                           command_line->argv().begin()));
  }

  void ListOfURLs() {
    // Verifies that policy can set the startup pages to a list of URLs.
    base::ListValue urls;
    for (size_t i = 0; i < base::size(kRestoredURLs); ++i) {
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
    for (size_t i = 0; i < base::size(kRestoredURLs); ++i)
      expected_urls_.push_back(GURL(kRestoredURLs[i]));
  }

  void Blocked() {
    // Verifies that URLs are blocked during session restore.
    PolicyMap policies;
    policies.Set(
        key::kRestoreOnStartup, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        POLICY_SOURCE_CLOUD,
        std::make_unique<base::Value>(SessionStartupPref::kPrefValueLast),
        nullptr);
    auto urls = std::make_unique<base::Value>(base::Value::Type::LIST);
    for (const auto* url_string : kRestoredURLs)
      urls->Append(url_string);
    policies.Set(key::kURLBlacklist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                 POLICY_SOURCE_CLOUD, std::move(urls), nullptr);
    provider_.UpdateChromePolicy(policies);
    // This should restore the tabs opened at PRE_RunTest below, yet all should
    // be blocked.
    blocked_ = true;
    for (size_t i = 0; i < base::size(kRestoredURLs); ++i)
      expected_urls_.emplace_back(kRestoredURLs[i]);
  }

  // URLs that are expected to be loaded.
  std::vector<GURL> expected_urls_;

  // True if the loaded URLs should be blocked by policy.
  bool blocked_ = false;
};

IN_PROC_BROWSER_TEST_P(RestoreOnStartupPolicyTest, PRE_RunTest) {
  // Do not show Welcome Page.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kHasSeenWelcomePage,
                                               true);

  // Open some tabs to verify if they are restored after the browser restarts.
  // Most policy settings override this, except kPrefValueLast which enforces
  // a restore.
  ui_test_utils::NavigateToURL(browser(), GURL(kRestoredURLs[0]));
  for (size_t i = 1; i < base::size(kRestoredURLs); ++i) {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::AddSelectedTabWithURL(browser(), GURL(kRestoredURLs[i]),
                                  ui::PAGE_TRANSITION_LINK);
    observer.Wait();
  }
}

IN_PROC_BROWSER_TEST_P(RestoreOnStartupPolicyTest, RunTest) {
  TabStripModel* model = browser()->tab_strip_model();
  int size = static_cast<int>(expected_urls_.size());
  EXPECT_EQ(size, model->count());
  resource_coordinator::WaitForTransitionToLoaded(model);
  for (int i = 0; i < size && i < model->count(); ++i) {
    content::WebContents* web_contents = model->GetWebContentsAt(i);
    if (blocked_)
      CheckURLIsBlockedInWebContents(web_contents, expected_urls_[i]);
    else if (expected_urls_[i] == GURL(chrome::kChromeUINewTabURL))
      EXPECT_TRUE(search::IsInstantNTP(web_contents));
    else
      EXPECT_EQ(expected_urls_[i], web_contents->GetURL());
  }
}

INSTANTIATE_TEST_SUITE_P(
    RestoreOnStartupPolicyTestInstance,
    RestoreOnStartupPolicyTest,
    testing::Values(&RestoreOnStartupPolicyTest::ListOfURLs,
                    &RestoreOnStartupPolicyTest::NTP,
                    &RestoreOnStartupPolicyTest::Last,
                    &RestoreOnStartupPolicyTest::Blocked));

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
                 std::make_unique<base::Value>(true), nullptr);
    policies.Set(key::kBookmarkBarEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(false), nullptr);
    policies.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>("http://chromium.org"), nullptr);
    provider_.UpdateChromePolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(PolicyStatisticsCollectorTest, Startup) {
  // Verifies that policy usage histograms are collected at startup.

  // BrowserPolicyConnector::Init() has already been called. Make sure the
  // CompleteInitialization() task has executed as well.
  content::RunAllPendingInMessageLoop();

  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram("Enterprise.Policies");
  std::unique_ptr<base::HistogramSamples> samples(histogram->SnapshotSamples());
  // HomepageLocation has policy ID 1.
  EXPECT_GT(samples->GetCount(1), 0);
  // ShowHomeButton has policy ID 35.
  EXPECT_GT(samples->GetCount(35), 0);
  // BookmarkBarEnabled has policy ID 82.
  EXPECT_GT(samples->GetCount(82), 0);
}

// Similar to PolicyTest, but force to enable the new tab material design flag
// before the browser start.
class PolicyWebStoreIconTest : public PolicyTest {
 public:
  PolicyWebStoreIconTest() {}
  ~PolicyWebStoreIconTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyTest::SetUpCommandLine(command_line);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PolicyWebStoreIconTest);
};

IN_PROC_BROWSER_TEST_F(PolicyWebStoreIconTest, AppsWebStoreIconHidden) {
  // Verifies that the web store icon can be hidden from the chrome://apps
  // page. A policy change takes immediate effect on the apps page for the
  // current profile. Browser restart is not required.

  // Open new tab page and look for the web store icons.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAppsURL));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

#if !defined(OS_CHROMEOS)
  // Look for web store's app ID in the apps page.
  EXPECT_TRUE(
      ContainsVisibleElement(contents, "ahfgeienlihckogmohjhadlkjgocpleb"));
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
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAppsURL));
  EXPECT_FALSE(
      ContainsVisibleElement(contents, "ahfgeienlihckogmohjhadlkjgocpleb"));
  EXPECT_FALSE(ContainsVisibleElement(contents, "chrome-web-store-link"));
}

IN_PROC_BROWSER_TEST_F(PolicyWebStoreIconTest, NTPWebStoreIconShown) {
  // This test is to verify that the web store icons is shown when no policy
  // applies. See WebStoreIconPolicyTest.NTPWebStoreIconHidden for verification
  // when a policy is in effect.

  // Open new tab page and look for the web store icons.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  content::RenderFrameHost* iframe = GetMostVisitedIframe(active_tab);

  // Look though all the tiles and see whether there is a webstore icon.
  // Make sure that there is one web store icon.
  EXPECT_TRUE(ContainsWebstoreTile(iframe));
}

// Similar to PolicyWebStoreIconShownTest, but applies the HideWebStoreIcon
// policy before the browser is started. This is required because the list that
// includes the WebStoreIcon on the NTP is initialized at browser start.
class PolicyWebStoreIconHiddenTest : public PolicyTest {
 public:
  PolicyWebStoreIconHiddenTest() {}
  ~PolicyWebStoreIconHiddenTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(key::kHideWebStoreIcon, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(true), nullptr);
    provider_.UpdateChromePolicy(policies);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyTest::SetUpCommandLine(command_line);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PolicyWebStoreIconHiddenTest);
};

IN_PROC_BROWSER_TEST_F(PolicyWebStoreIconHiddenTest, NTPWebStoreIconHidden) {
  // Verifies that the web store icon can be hidden from the new tab page. Check
  // to see NTPWebStoreIconShown for behavior when the policy is not applied.

  // Open new tab page and look for the web store icon
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  content::RenderFrameHost* iframe = GetMostVisitedIframe(active_tab);

  // Applying the policy before the browser started, the web store icon should
  // now be hidden.
  EXPECT_FALSE(ContainsWebstoreTile(iframe));
}

class CertificateTransparencyPolicyTest : public PolicyTest {
 public:
  CertificateTransparencyPolicyTest() : PolicyTest() {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        true);
  }

  ~CertificateTransparencyPolicyTest() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        base::nullopt);
  }
};

IN_PROC_BROWSER_TEST_F(CertificateTransparencyPolicyTest,
                       CertificateTransparencyEnforcementDisabledForUrls) {
  net::EmbeddedTestServer https_server_ok(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_ok.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  https_server_ok.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_ok.Start());

  // Require CT for all hosts (in the absence of policy).
  SetRequireCTForTesting(true);

  ui_test_utils::NavigateToURL(browser(), https_server_ok.GetURL("/"));

  // The page should initially be blocked.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(tab);

  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetMainFrame(), "proceed-link"));
  EXPECT_NE(base::UTF8ToUTF16("OK"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

  // Now exempt the URL from being blocked by setting policy.
  std::unique_ptr<base::ListValue> disabled_urls =
      std::make_unique<base::ListValue>();
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
  EXPECT_FALSE(IsShowingInterstitial(tab));
  EXPECT_EQ(base::UTF8ToUTF16("OK"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

  // Now ensure that this setting still works after a network process crash.
  if (!content::IsOutOfProcessNetworkService())
    return;

  ui_test_utils::NavigateToURL(browser(),
                               https_server_ok.GetURL("/title1.html"));

  SimulateNetworkServiceCrash();
  SetRequireCTForTesting(true);

  ui_test_utils::NavigateToURL(browser(),
                               https_server_ok.GetURL("/simple.html"));

  // There should be no interstitial after the page loads.
  EXPECT_FALSE(IsShowingInterstitial(tab));
  EXPECT_EQ(base::UTF8ToUTF16("OK"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
}

IN_PROC_BROWSER_TEST_F(CertificateTransparencyPolicyTest,
                       CertificateTransparencyEnforcementDisabledForCas) {
  net::EmbeddedTestServer https_server_ok(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_ok.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  https_server_ok.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server_ok.Start());

  // Require CT for all hosts (in the absence of policy).
  SetRequireCTForTesting(true);

  ui_test_utils::NavigateToURL(browser(), https_server_ok.GetURL("/"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // The page should initially be blocked.
  content::RenderFrameHost* main_frame;
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          web_contents);
  ASSERT_TRUE(helper);
  ASSERT_TRUE(
      helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting());
  main_frame = web_contents->GetMainFrame();
  ASSERT_TRUE(content::WaitForRenderFrameReady(main_frame));
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      main_frame, "proceed-link"));

  EXPECT_NE(base::UTF8ToUTF16("OK"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());

  // Now exempt the leaf SPKI from being blocked by setting policy.
  net::HashValue leaf_hash;
  ASSERT_TRUE(net::x509_util::CalculateSha256SpkiHash(
      https_server_ok.GetCertificate()->cert_buffer(), &leaf_hash));
  std::unique_ptr<base::ListValue> disabled_spkis =
      std::make_unique<base::ListValue>();
  disabled_spkis->AppendString(leaf_hash.ToString());

  PolicyMap policies;
  policies.Set(key::kCertificateTransparencyEnforcementDisabledForCas,
               POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::move(disabled_spkis), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();

  ui_test_utils::NavigateToURL(browser(),
                               https_server_ok.GetURL("/simple.html"));

  // Check we are no longer in the interstitial.
  EXPECT_EQ(base::UTF8ToUTF16("OK"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
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
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(tab);

  // The interstitial should display the proceed link.
  EXPECT_TRUE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetMainFrame(), "proceed-link"));
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
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  WaitForInterstitial(tab);

  // The interstitial should not display the proceed link.
  EXPECT_FALSE(chrome_browser_interstitials::IsInterstitialDisplayingText(
      tab->GetMainFrame(), "proceed-link"));

  // The interstitial should not proceed, even if the command is sent in
  // some other way (e.g., via the keyboard shortcut).
  SendInterstitialCommand(tab, security_interstitials::CMD_PROCEED);
  EXPECT_TRUE(IsShowingInterstitial(tab));
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
                   std::make_unique<base::Value>(kTestWebRtcUdpPortRange),
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

// Sets the proper policy before the browser is started.
class WebRtcLocalIpsAllowedUrlsTest : public PolicyTest,
                                      public testing::WithParamInterface<int> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(key::kWebRtcLocalIpsAllowedUrls, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 std::make_unique<base::ListValue>(GenerateUrlList()), nullptr);
    provider_.UpdateChromePolicy(policies);
  }

  base::Value::ListStorage GenerateUrlList() {
    int num_urls = GetParam();
    base::Value::ListStorage ret;
    for (int i = 0; i < num_urls; ++i)
      ret.push_back(base::Value(base::NumberToString(i) + ".example.com"));

    return ret;
  }
};

IN_PROC_BROWSER_TEST_P(WebRtcLocalIpsAllowedUrlsTest, RunTest) {
  const PrefService::Preference* pref =
      user_prefs::UserPrefs::Get(browser()->profile())
          ->FindPreference(prefs::kWebRtcLocalIpsAllowedUrls);
  EXPECT_TRUE(pref->IsManaged());
  base::Value::ConstListView allowed_urls = pref->GetValue()->GetList();
  const auto& expected_urls = GenerateUrlList();
  EXPECT_EQ(expected_urls.size(), allowed_urls.size());
  for (const auto& allowed_url : allowed_urls) {
    auto it =
        std::find(expected_urls.begin(), expected_urls.end(), allowed_url);
    EXPECT_TRUE(it != expected_urls.end());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebRtcLocalIpsAllowedUrlsTest,
                         ::testing::Range(0, 3));

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
                 std::make_unique<base::Value>("restricted"), nullptr);
    provider_.UpdateChromePolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(PolicyVariationsServiceTest, VariationsURLIsValid) {
  const std::string default_variations_url =
      variations::VariationsService::GetDefaultVariationsServerURLForTesting();

  const GURL url =
      g_browser_process->variations_service()->GetVariationsServerURL(
          variations::VariationsService::HttpOptions::USE_HTTPS);
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

  EXPECT_EQ(extensions::MessagingDelegate::PolicyPermission::DISALLOW,
            IsNativeMessagingHostAllowed(browser()->profile(), "host.name"));
  EXPECT_EQ(
      extensions::MessagingDelegate::PolicyPermission::ALLOW_ALL,
      IsNativeMessagingHostAllowed(browser()->profile(), "other.host.name"));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, NativeMessagingBlacklistWildcard) {
  base::ListValue blacklist;
  blacklist.AppendString("*");
  PolicyMap policies;
  policies.Set(key::kNativeMessagingBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               blacklist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_EQ(extensions::MessagingDelegate::PolicyPermission::DISALLOW,
            IsNativeMessagingHostAllowed(browser()->profile(), "host.name"));
  EXPECT_EQ(
      extensions::MessagingDelegate::PolicyPermission::DISALLOW,
      IsNativeMessagingHostAllowed(browser()->profile(), "other.host.name"));
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

  EXPECT_EQ(extensions::MessagingDelegate::PolicyPermission::ALLOW_ALL,
            IsNativeMessagingHostAllowed(browser()->profile(), "host.name"));
  EXPECT_EQ(
      extensions::MessagingDelegate::PolicyPermission::DISALLOW,
      IsNativeMessagingHostAllowed(browser()->profile(), "other.host.name"));
}

#endif  // !defined(CHROME_OS)

#if !defined(OS_CHROMEOS)
// Sets the hardware acceleration mode policy before the browser is started.
class HardwareAccelerationModePolicyTest : public PolicyTest {
 public:
  HardwareAccelerationModePolicyTest() {}

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(key::kHardwareAccelerationModeEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(false), nullptr);
    provider_.UpdateChromePolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(HardwareAccelerationModePolicyTest,
                       HardwareAccelerationDisabled) {
  // Verifies that hardware acceleration can be disabled with policy.
  EXPECT_FALSE(
      content::GpuDataManager::GetInstance()->HardwareAccelerationEnabled());
}
#endif  // !defined(OS_CHROMEOS)

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

#endif  // defined(OS_CHROMEOS)

class NetworkTimePolicyTest : public PolicyTest {
 public:
  NetworkTimePolicyTest() {}
  ~NetworkTimePolicyTest() override {}

  void SetUpOnMainThread() override {
    std::map<std::string, std::string> parameters;
    parameters["FetchBehavior"] = "on-demand-only";
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        network_time::kNetworkTimeServiceQuerying, parameters);
    PolicyTest::SetUpOnMainThread();
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
  base::test::ScopedFeatureList scoped_feature_list_;
  uint32_t num_requests_ = 0;

  DISALLOW_COPY_AND_ASSIGN(NetworkTimePolicyTest);
};

// TODO(https://crbug.com/1012853): This test is using ScopedFeatureList
// incorrectly, and fixing it causes conflicts with PolicyTest's use of the
// deprecated variations API.
IN_PROC_BROWSER_TEST_F(NetworkTimePolicyTest,
                       DISABLED_NetworkTimeQueriesDisabled) {
  // Set a policy to disable network time queries.
  PolicyMap policies;
  policies.Set(key::kBrowserNetworkTimeQueriesEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(false), nullptr);
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
  WaitForInterstitial(tab);
  EXPECT_EQ(0u, num_requests());

  // Now enable the policy and check that a network time query is sent.
  policies.Set(key::kBrowserNetworkTimeQueriesEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(true), nullptr);
  UpdateProviderPolicy(policies);
  ui_test_utils::NavigateToURL(browser(), https_server_expired_.GetURL("/"));
  EXPECT_TRUE(IsShowingInterstitial(tab));
  EXPECT_EQ(1u, num_requests());
}

#if defined(OS_CHROMEOS)

class NoteTakingOnLockScreenPolicyTest : public PolicyTest {
 public:
  NoteTakingOnLockScreenPolicyTest() = default;
  ~NoteTakingOnLockScreenPolicyTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // An app requires lockScreen permission to be enabled as a lock screen app.
    // This permission is protected by a whitelist, so the test app has to be
    // whitelisted as well.
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID, kTestAppId);
    command_line->AppendSwitch(ash::switches::kAshForceEnableStylusTools);
    PolicyTest::SetUpCommandLine(command_line);
  }

  void SetUserLevelPrefValue(const std::string& app_id,
                             bool enabled_on_lock_screen) {
    chromeos::NoteTakingHelper* helper = chromeos::NoteTakingHelper::Get();
    ASSERT_TRUE(helper);

    helper->SetPreferredApp(browser()->profile(), app_id);
    helper->SetPreferredAppEnabledOnLockScreen(browser()->profile(),
                                               enabled_on_lock_screen);
  }

  void SetPolicyValue(std::unique_ptr<base::Value> value) {
    PolicyMap policies;
    if (value) {
      policies.Set(key::kNoteTakingAppsLockScreenWhitelist,
                   POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_CLOUD, std::move(value), nullptr);
    }
    UpdateProviderPolicy(policies);
  }

  chromeos::NoteTakingLockScreenSupport GetAppLockScreenStatus(
      const std::string& app_id) {
    std::unique_ptr<chromeos::NoteTakingAppInfo> info =
        chromeos::NoteTakingHelper::Get()->GetPreferredChromeAppInfo(
            browser()->profile());
    if (!info || info->app_id != app_id)
      return chromeos::NoteTakingLockScreenSupport::kNotSupported;
    return info->lock_screen_support;
  }

  // The test app ID.
  static const char kTestAppId[];

 private:
  DISALLOW_COPY_AND_ASSIGN(NoteTakingOnLockScreenPolicyTest);
};

const char NoteTakingOnLockScreenPolicyTest::kTestAppId[] =
    "cadfeochfldmbdgoccgbeianhamecbae";

IN_PROC_BROWSER_TEST_F(NoteTakingOnLockScreenPolicyTest,
                       DisableLockScreenNoteTakingByPolicy) {
  scoped_refptr<const extensions::Extension> app =
      LoadUnpackedExtension("lock_screen_apps/app_launch");
  ASSERT_TRUE(app);
  ASSERT_EQ(kTestAppId, app->id());

  SetUserLevelPrefValue(app->id(), true);
  EXPECT_EQ(chromeos::NoteTakingLockScreenSupport::kEnabled,
            GetAppLockScreenStatus(app->id()));

  SetPolicyValue(std::make_unique<base::ListValue>());
  EXPECT_EQ(chromeos::NoteTakingLockScreenSupport::kNotAllowedByPolicy,
            GetAppLockScreenStatus(app->id()));

  SetPolicyValue(nullptr);
  EXPECT_EQ(chromeos::NoteTakingLockScreenSupport::kEnabled,
            GetAppLockScreenStatus(app->id()));
}

IN_PROC_BROWSER_TEST_F(NoteTakingOnLockScreenPolicyTest,
                       WhitelistLockScreenNoteTakingAppByPolicy) {
  scoped_refptr<const extensions::Extension> app =
      LoadUnpackedExtension("lock_screen_apps/app_launch");
  ASSERT_TRUE(app);
  ASSERT_EQ(kTestAppId, app->id());

  SetUserLevelPrefValue(app->id(), false);
  EXPECT_EQ(chromeos::NoteTakingLockScreenSupport::kSupported,
            GetAppLockScreenStatus(app->id()));

  auto policy = std::make_unique<base::ListValue>();
  policy->Append(base::Value(kTestAppId));
  SetPolicyValue(std::move(policy));

  EXPECT_EQ(chromeos::NoteTakingLockScreenSupport::kSupported,
            GetAppLockScreenStatus(app->id()));

  SetUserLevelPrefValue(app->id(), true);
  EXPECT_EQ(chromeos::NoteTakingLockScreenSupport::kEnabled,
            GetAppLockScreenStatus(app->id()));
}

#endif  // defined(OS_CHROMEOS)

// Handler for embedded http-server, returns a small page with javascript
// variable and a link to increment it. It's for JavascriptBlacklistable test.
std::unique_ptr<net::test_server::HttpResponse> JSIncrementerPageHandler(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/test.html") {
    return std::unique_ptr<net::test_server::HttpResponse>();
  }

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse());
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(
      "<head><script type=\"text/javascript\">\n"
      "<!--\n"
      "var value = 1;"
      "var increment = function() {"
      "  value = value + 1;"
      "};\n"
      "//-->\n"
      "</script></head><body>"
      "<a id='link' href=\"javascript:increment();\">click</a>"
      "</body>");
  http_response->set_content_type("text/html");
  return http_response;
}

// Fetch value from page generated by JSIncrementerPageHandler.
int JSIncrementerFetch(content::WebContents* contents) {
  int result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      contents, "domAutomationController.send(value);", &result));
  return result;
}

// Tests that javascript-links are handled properly according to blacklist
// settings, bug 913334.
IN_PROC_BROWSER_TEST_F(PolicyTest, JavascriptBlacklistable) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&JSIncrementerPageHandler));
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/test.html"));

  EXPECT_EQ(JSIncrementerFetch(contents), 1);

  // Without blacklist policy value is incremented properly.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("javascript:increment()"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);

  EXPECT_EQ(JSIncrementerFetch(contents), 2);

  // Create and apply a policy.
  base::ListValue blacklist;
  blacklist.AppendString("javascript://*");
  PolicyMap policies;
  policies.Set(key::kURLBlacklist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, blacklist.CreateDeepCopy(), nullptr);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();

  // After applying policy javascript url's don't work any more, value leaves
  // unchanged.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("javascript:increment()"),
      WindowOpenDisposition::CURRENT_TAB, ui_test_utils::BROWSER_TEST_NONE);
  EXPECT_EQ(JSIncrementerFetch(contents), 2);

  // But in-page links still work even if they are javascript-links.
  EXPECT_TRUE(content::ExecuteScript(
      contents, "document.getElementById('link').click();"));
  EXPECT_EQ(JSIncrementerFetch(contents), 3);
}

#if !defined(OS_ANDROID)

// The possibilities for a boolean policy.
enum class BooleanPolicy {
  kNotConfigured,
  kFalse,
  kTrue,
};

#endif  // !defined(OS_ANDROID)

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)

// Tests that the PromotionalTabsEnabled policy properly suppresses the welcome
// page for browser first-runs.
class PromotionalTabsEnabledPolicyTest
    : public PolicyTest,
      public testing::WithParamInterface<BooleanPolicy> {
 protected:
  PromotionalTabsEnabledPolicyTest() {
    scoped_feature_list_.InitWithFeatures({welcome::kForceEnabled}, {});
  }
  ~PromotionalTabsEnabledPolicyTest() = default;

  void SetUp() override {
    // Ordinarily, browser tests include chrome://blank on the command line to
    // suppress any onboarding or promotional tabs. This test, on the other
    // hand, must evaluate startup with nothing on the command line so that a
    // default launch takes place.
    set_open_about_blank_on_browser_launch(false);
    PolicyTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kForceFirstRun);
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    // Set policies before the browser starts up.
    PolicyMap policies;

    // Suppress the first-run dialog by disabling metrics reporting.
    policies.Set(key::kMetricsReportingEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(false), nullptr);

    // Apply the policy setting under test.
    if (GetParam() != BooleanPolicy::kNotConfigured) {
      policies.Set(
          key::kPromotionalTabsEnabled, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
          std::make_unique<base::Value>(GetParam() == BooleanPolicy::kTrue),
          nullptr);
    }

    UpdateProviderPolicy(policies);
    PolicyTest::CreatedBrowserMainParts(browser_main_parts);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(PromotionalTabsEnabledPolicyTest);
};

IN_PROC_BROWSER_TEST_P(PromotionalTabsEnabledPolicyTest, RunTest) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_GE(tab_strip->count(), 1);
  const auto& url = tab_strip->GetWebContentsAt(0)->GetURL();
  switch (GetParam()) {
    case BooleanPolicy::kFalse:
      // Only the NTP should show.
      EXPECT_EQ(tab_strip->count(), 1);
      if (url.possibly_invalid_spec() != chrome::kChromeUINewTabURL)
        EXPECT_TRUE(search::IsNTPOrRelatedURL(url, browser()->profile()))
            << url;
      break;
    case BooleanPolicy::kNotConfigured:
    case BooleanPolicy::kTrue:
      // One or more onboarding tabs should show.
      EXPECT_NE(url.possibly_invalid_spec(), chrome::kChromeUINewTabURL);
      EXPECT_FALSE(search::IsNTPOrRelatedURL(url, browser()->profile())) << url;
      break;
  }
}
#undef MAYBE_RunTest

INSTANTIATE_TEST_SUITE_P(All,
                         PromotionalTabsEnabledPolicyTest,
                         ::testing::Values(BooleanPolicy::kNotConfigured,
                                           BooleanPolicy::kFalse,
                                           BooleanPolicy::kTrue));

#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
class WebRtcEventLogCollectionAllowedPolicyTest
    : public PolicyTest,
      public testing::WithParamInterface<BooleanPolicy> {
 public:
  ~WebRtcEventLogCollectionAllowedPolicyTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;

    const BooleanPolicy policy = GetParam();
    if (policy == BooleanPolicy::kFalse || policy == BooleanPolicy::kTrue) {
      const bool policy_bool = (policy == BooleanPolicy::kTrue);
      policies.Set(policy::key::kWebRtcEventLogCollectionAllowed,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
                   std::make_unique<base::Value>(policy_bool), nullptr);
    }

    provider_.UpdateChromePolicy(policies);
  }

  const PrefService::Preference* GetPreference() const {
    auto* service = user_prefs::UserPrefs::Get(browser()->profile());
    return service->FindPreference(prefs::kWebRtcEventLogCollectionAllowed);
  }

  base::OnceCallback<void(bool)> BlockingBoolExpectingReply(
      base::RunLoop* run_loop,
      bool expected_value) {
    return base::BindOnce(
        [](base::RunLoop* run_loop, bool expected_value, bool value) {
          EXPECT_EQ(expected_value, value);
          run_loop->Quit();
        },
        run_loop, expected_value);
  }

  // The "extras" in question are the ID and error (only one of which may
  // be non-null), which this test ignores (tested elsewhere).
  base::OnceCallback<void(bool, const std::string&, const std::string&)>
  BlockingBoolExpectingReplyWithExtras(base::RunLoop* run_loop,
                                       bool expected_value) {
    return base::BindOnce(
        [](base::RunLoop* run_loop, bool expected_value, bool value,
           const std::string& ignored_log_id,
           const std::string& ignored_error) {
          EXPECT_EQ(expected_value, value);
          run_loop->Quit();
        },
        run_loop, expected_value);
  }
};

IN_PROC_BROWSER_TEST_P(WebRtcEventLogCollectionAllowedPolicyTest, RunTest) {
  const PrefService::Preference* const pref = GetPreference();
  const bool remote_logging_allowed = (GetParam() == BooleanPolicy::kTrue);
  ASSERT_EQ(pref->GetValue()->GetBool(), remote_logging_allowed);

  auto* webrtc_event_log_manager = WebRtcEventLogManager::GetInstance();
  ASSERT_TRUE(webrtc_event_log_manager);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int render_process_id = web_contents->GetMainFrame()->GetProcess()->GetID();

  constexpr int kLid = 123;
  const std::string kSessionId = "id";

  {
    base::RunLoop run_loop;
    webrtc_event_log_manager->PeerConnectionAdded(
        render_process_id, kLid, BlockingBoolExpectingReply(&run_loop, true));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    webrtc_event_log_manager->PeerConnectionSessionIdSet(
        render_process_id, kLid, kSessionId,
        BlockingBoolExpectingReply(&run_loop, true));
    run_loop.Run();
  }

  {
    constexpr size_t kMaxFileSizeBytes = 1000 * 1000;
    constexpr int kOutputPeriodMs = 1000;

    base::RunLoop run_loop;

    // Test focus - remote-bound logging allowed if and only if the policy
    // is configured to allow it.
    webrtc_event_log_manager->StartRemoteLogging(
        render_process_id, kSessionId, kMaxFileSizeBytes, kOutputPeriodMs,
        kWebAppId,
        BlockingBoolExpectingReplyWithExtras(&run_loop,
                                             remote_logging_allowed));
    run_loop.Run();
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebRtcEventLogCollectionAllowedPolicyTest,
                         ::testing::Values(BooleanPolicy::kNotConfigured,
                                           BooleanPolicy::kFalse,
                                           BooleanPolicy::kTrue));
#endif  // !defined(OS_ANDROID)

class SignedExchangePolicyTest : public PolicyTest {
 public:
  SignedExchangePolicyTest() {}
  ~SignedExchangePolicyTest() override {}

  void SetUp() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &SignedExchangePolicyTest::MonitorRequest, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    sxg_test_helper_.SetUp();
    PolicyTest::SetUp();
  }

  void TearDownOnMainThread() override {
    PolicyTest::TearDownOnMainThread();
    sxg_test_helper_.TearDownOnMainThread();
  }

 protected:
  void SetSignedExchangePolicy(bool enabled) {
    PolicyMap policies;
    policies.Set(key::kSignedHTTPExchangeEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(enabled), nullptr);
    UpdateProviderPolicy(policies);
  }

  void InstallUrlInterceptor(const GURL& url, const std::string& data_path) {
    sxg_test_helper_.InstallUrlInterceptor(url, data_path);
  }

  bool HadSignedExchangeInAcceptHeader(const GURL& url) const {
    const auto it = url_accept_header_map_.find(url);
    if (it == url_accept_header_map_.end())
      return false;
    return it->second.find("application/signed-exchange") != std::string::npos;
  }

 private:
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    const auto it = request.headers.find("Accept");
    if (it == request.headers.end())
      return;
    url_accept_header_map_[request.base_url.Resolve(request.relative_url)] =
        it->second;
  }

  content::SignedExchangeBrowserTestHelper sxg_test_helper_;
  std::map<GURL, std::string> url_accept_header_map_;
};

IN_PROC_BROWSER_TEST_F(SignedExchangePolicyTest, SignedExchangeDisabled) {
  SetSignedExchangePolicy(false);

  content::DownloadTestObserverTerminal download_observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()), 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_DENY);

  GURL url = embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");
  ui_test_utils::NavigateToURL(browser(), url);

  download_observer.WaitForFinished();

  // Check that the SXG file was not loaded as a page, but downloaded.
  std::vector<download::DownloadItem*> downloads;
  content::BrowserContext::GetDownloadManager(browser()->profile())
      ->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  EXPECT_EQ(downloads[0]->GetURL(), url);

  ASSERT_FALSE(HadSignedExchangeInAcceptHeader(url));
}

IN_PROC_BROWSER_TEST_F(SignedExchangePolicyTest, SignedExchangeEnabled) {
  SetSignedExchangePolicy(true);

  InstallUrlInterceptor(GURL("https://test.example.org/test/"),
                        "content/test/data/sxg/fallback.html");

  GURL url = embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");
  base::string16 title = base::ASCIIToUTF16("Fallback URL response");
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), title);
  ui_test_utils::NavigateToURL(browser(), url);

  // Check that the SXG file was handled as a Signed Exchange, and the
  // navigation was redirected to the SXG's fallback URL.
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());

  ASSERT_TRUE(HadSignedExchangeInAcceptHeader(url));
}

class HSTSPolicyTest : public PolicyTest {
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    std::vector<base::Value> bypass_list;
    bypass_list.push_back(base::Value("example"));
    SetPolicy(&policies, key::kHSTSPolicyBypassList,
              std::make_unique<base::ListValue>(bypass_list));
    provider_.UpdateChromePolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(HSTSPolicyTest, HSTSPolicyBypassList) {
  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
  content::GetNetworkService()->BindTestInterface(
      network_service_test.BindNewPipeAndPassReceiver());
  mojo::ScopedAllowSyncCallForTesting allow_sync_call;
  // The port number 1234 here doesn't matter - it just needs to be a non-zero
  // value so that we use the unittest_default preload list.
  network_service_test->SetTransportSecurityStateSource(1234);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url("http://example/");
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // If the policy didn't take effect, the request to http://example would be
  // upgraded to https://example. This checks that the HSTS upgrade to https
  // didn't happen.
  EXPECT_EQ(url, contents->GetURL());
}

class PolicyTestSyncXHR : public PolicyTest {
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(policy::key::kAllowSyncXHRInPageDismissal,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(true), nullptr);
    provider_.UpdateChromePolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(PolicyTestSyncXHR, CheckAllowSyncXHRInPageDismissal) {
  ASSERT_TRUE(embedded_test_server()->Start());

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowSyncXHRInPageDismissal);
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kAllowSyncXHRInPageDismissal));

  GURL url(embedded_test_server()->GetURL("/empty.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  constexpr char kScript[] =
      R"({
           window.addEventListener('unload', function() {
             var xhr = new XMLHttpRequest();
             xhr.open('GET', '', false);
             try { xhr.send(); } catch(err) {
               window.domAutomationController.send(false);
             }
             window.domAutomationController.send(xhr.status === 200);
           });
           window.location.href='about:blank';
         })";
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::DOMMessageQueue message_queue;
  content::ExecuteScriptAsync(web_contents, kScript);
  std::string message;
  EXPECT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("true", message);
}

class SharedClipboardPolicyTest : public PolicyTest {
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(policy::key::kSharedClipboardEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(true), nullptr);
    provider_.UpdateChromePolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(SharedClipboardPolicyTest, SharedClipboardEnabled) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kSharedClipboardEnabled));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kSharedClipboardEnabled));
}

#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))

class AudioSandboxEnabledTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<
          /*policy::key::kAllowAudioSandbox=*/base::Optional<bool>> {
 public:
  // InProcessBrowserTest implementation:
  void SetUp() override {
    EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::PolicyMap values;
    if (GetParam().has_value()) {
      values.Set(policy::key::kAudioSandboxEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(*GetParam()), nullptr);
    }
    policy_provider_.UpdateChromePolicy(values);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    InProcessBrowserTest::SetUp();
  }

 private:
  policy::MockConfigurationPolicyProvider policy_provider_;
};

IN_PROC_BROWSER_TEST_P(AudioSandboxEnabledTest, IsRespected) {
  base::Optional<bool> enable_sandbox_via_policy = GetParam();
  bool is_sandbox_enabled_by_default = base::FeatureList::IsEnabled(
      service_manager::features::kAudioServiceSandbox);

  ASSERT_EQ(enable_sandbox_via_policy.value_or(is_sandbox_enabled_by_default),
            service_manager::IsAudioSandboxEnabled());
}

INSTANTIATE_TEST_SUITE_P(
    Enabled,
    AudioSandboxEnabledTest,
    ::testing::Values(/*policy::key::kAudioSandboxEnabled=*/true));

INSTANTIATE_TEST_SUITE_P(
    Disabled,
    AudioSandboxEnabledTest,
    ::testing::Values(/*policy::key::kAudioSandboxEnabled=*/false));

INSTANTIATE_TEST_SUITE_P(
    NotSet,
    AudioSandboxEnabledTest,
    ::testing::Values(/*policy::key::kAudioSandboxEnabled=*/base::nullopt));

#endif  //  defined(OS_WIN) || defined (OS_MACOSX) || (defined(OS_LINUX) &&
        //  !defined(OS_CHROMEOS))

enum class CorsPolicyTestMode {
  kEnabled,
  kDisabled,
};

class CorsPolicyTest
    : public PolicyTest,
      public ::testing::WithParamInterface<CorsPolicyTestMode> {
 public:
  CorsPolicyTest() {
    switch (GetParam()) {
      case CorsPolicyTestMode::kEnabled:
        scoped_feature_list_.InitWithFeatures(
            {}, {features::kHideCorsLegacyModeEnabledPolicySupport,
                 features::kHideCorsMitigationListPolicySupport});
        break;
      case CorsPolicyTestMode::kDisabled:
        scoped_feature_list_.InitWithFeatures(
            {features::kHideCorsLegacyModeEnabledPolicySupport,
             features::kHideCorsMitigationListPolicySupport},
            {});
        break;
    }
  }

 protected:
  bool ShouldForceWebRequestExtraHeaders() {
    switch (GetParam()) {
      case CorsPolicyTestMode::kEnabled:
        return network::features::ShouldEnableOutOfBlinkCorsForTesting();
      case CorsPolicyTestMode::kDisabled:
        return false;
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// See CorsExtraSafelistedHeaderNamesTest for more complex end to end tests.
IN_PROC_BROWSER_TEST_P(CorsPolicyTest, CorsMitigationExtraHeadersTest) {
  // The list should be initialized as an empty list, but should not be managed.
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(prefs->GetList(prefs::kCorsMitigationList));
  EXPECT_TRUE(prefs->GetList(prefs::kCorsMitigationList)->empty());
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kCorsMitigationList));

  EXPECT_FALSE(extensions::ExtensionsBrowserClient::Get()
                   ->ShouldForceWebRequestExtraHeaders(browser()->profile()));

  PolicyMap policies;
  policies.Set(key::kCorsMitigationList, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::ListValue>(), nullptr);
  UpdateProviderPolicy(policies);

  // Now the list is managed, and it enforces the webRequest API to use the
  // extraHeaders option.
  EXPECT_TRUE(prefs->GetList(prefs::kCorsMitigationList));
  EXPECT_TRUE(prefs->GetList(prefs::kCorsMitigationList)->empty());
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kCorsMitigationList));

  EXPECT_EQ(ShouldForceWebRequestExtraHeaders(),
            extensions::ExtensionsBrowserClient::Get()
                ->ShouldForceWebRequestExtraHeaders(browser()->profile()));
}

IN_PROC_BROWSER_TEST_P(CorsPolicyTest, CorsLegacyModeEnabledConsistencyTest) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  bool is_out_of_blink_cors_enabled = profile->ShouldEnableOutOfBlinkCors();

  // Check initial states.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kCorsLegacyModeEnabled));
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kCorsLegacyModeEnabled));

  // Check if updated policies are reflected. However, |profile| should keep
  // returning a consistent value that returned at the first access.
  PolicyMap policies;
  policies.Set(key::kCorsLegacyModeEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(true), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_TRUE(prefs->GetBoolean(prefs::kCorsLegacyModeEnabled));
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kCorsLegacyModeEnabled));
  EXPECT_EQ(is_out_of_blink_cors_enabled,
            profile->ShouldEnableOutOfBlinkCors());

  // Flip the value, and check again.
  policies.Set(key::kCorsLegacyModeEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_FALSE(prefs->GetBoolean(prefs::kCorsLegacyModeEnabled));
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kCorsLegacyModeEnabled));
  EXPECT_EQ(is_out_of_blink_cors_enabled,
            profile->ShouldEnableOutOfBlinkCors());
}

INSTANTIATE_TEST_SUITE_P(EnabledCorsPolicyTest,
                         CorsPolicyTest,
                         testing::Values(CorsPolicyTestMode::kEnabled));

INSTANTIATE_TEST_SUITE_P(DisabledCorsPolicyTest,
                         CorsPolicyTest,
                         testing::Values(CorsPolicyTestMode::kDisabled));

}  // namespace policy
