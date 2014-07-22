// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_file_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/api/messaging/native_message_process_host.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_cache_fake.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/media_stream_devices_controller.h"
#include "chrome/browser/metrics/variations/variations_service.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/policy/cloud/test_request_interceptor.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/cld_data_harness.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/infobar.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/process_type.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/mock_notification_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/test/net/url_request_failed_job.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "grit/generated_resources.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/url_util.h"
#include "net/http/http_stream_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/magnifier/magnifier_constants.h"
#include "ash/shell.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/screenshot_taker.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "ui/keyboard/keyboard_util.h"
#endif

#if !defined(OS_MACOSX)
#include "apps/app_window.h"
#include "apps/app_window_registry.h"
#include "apps/ui/native_app_window.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "ui/base/window_open_disposition.h"
#endif

using content::BrowserThread;
using content::URLRequestMockHTTPJob;
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
const char kCookieOptions[] = ";expires=Wed Jan 01 3000 00:00:00 GMT";

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
const base::FilePath::CharType kGoodUnpackedExt[] =
    FILE_PATH_LITERAL("good_unpacked");
const base::FilePath::CharType kAppUnpackedExt[] =
    FILE_PATH_LITERAL("app");

#if !defined(OS_MACOSX)
const base::FilePath::CharType kUnpackedFullscreenAppName[] =
    FILE_PATH_LITERAL("fullscreen_app");
#endif  // !defined(OS_MACOSX)

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
    filter->AddUrlInterceptor(
        url, URLRequestMockHTTPJob::CreateInterceptor(base_path));
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
net::URLRequestJob* FailedJobFactory(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const std::string& scheme) {
  return new content::URLRequestFailedJob(
      request, network_delegate, net::ERR_CONNECTION_RESET);
}

// While |MakeRequestFail| is in scope URLRequests to |host| will fail.
class MakeRequestFail {
 public:
  // Sets up the filter on IO thread such that requests to |host| fail.
  explicit MakeRequestFail(const std::string& host) : host_(host) {
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO, FROM_HERE,
        base::Bind(MakeRequestFailOnIO, host_),
        base::MessageLoop::QuitClosure());
    content::RunMessageLoop();
  }
  ~MakeRequestFail() {
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO, FROM_HERE,
        base::Bind(UndoMakeRequestFailOnIO, host_),
        base::MessageLoop::QuitClosure());
    content::RunMessageLoop();
  }

 private:
  // Filters requests to the |host| such that they fail. Run on IO thread.
  static void MakeRequestFailOnIO(const std::string& host) {
    net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
    filter->AddHostnameHandler("http", host, &FailedJobFactory);
    filter->AddHostnameHandler("https", host, &FailedJobFactory);
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
  base::string16 title = base::UTF8ToUTF16(url.spec() + " was blocked");
  EXPECT_NE(title, contents->GetTitle());
}

// Verifies that access to the given url |spec| is blocked.
void CheckURLIsBlocked(Browser* browser, const char* spec) {
  GURL url(spec);
  ui_test_utils::NavigateToURL(browser, url);
  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(url, contents->GetURL());
  base::string16 title = base::UTF8ToUTF16(url.spec() + " was blocked");
  EXPECT_EQ(title, contents->GetTitle());

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
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
  base::FilePath downloaded = dir.Append(file);
  EXPECT_FALSE(base::PathExists(downloaded));
  ui_test_utils::NavigateToURLWithDisposition(
      browser, url, CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
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
  scoped_ptr<base::Value> value = content::ExecuteScriptAndGetValue(
      contents->GetMainFrame(), "123");
  int result = 0;
  if (!value->GetAsInteger(&result))
    EXPECT_EQ(base::Value::TYPE_NULL, value->GetType());
  return result == 123;
}

void CopyPluginListAndQuit(std::vector<content::WebPluginInfo>* out,
                           const std::vector<content::WebPluginInfo>& in) {
  *out = in;
  base::MessageLoop::current()->QuitWhenIdle();
}

template<typename T>
void CopyValueAndQuit(T* out, T in) {
  *out = in;
  base::MessageLoop::current()->QuitWhenIdle();
}

void GetPluginList(std::vector<content::WebPluginInfo>* plugins) {
  content::PluginService* service = content::PluginService::GetInstance();
  service->GetPlugins(base::Bind(CopyPluginListAndQuit, plugins));
  content::RunMessageLoop();
}

const content::WebPluginInfo* GetFlashPlugin(
    const std::vector<content::WebPluginInfo>& plugins) {
  const content::WebPluginInfo* flash = NULL;
  for (size_t i = 0; i < plugins.size(); ++i) {
    if (plugins[i].name == base::ASCIIToUTF16(content::kFlashPluginName)) {
      flash = &plugins[i];
      break;
    }
  }
#if defined(OFFICIAL_BUILD)
  // Official builds bundle Flash.
  EXPECT_TRUE(flash);
#else
  if (!flash)
    LOG(INFO) << "Test skipped because the Flash plugin couldn't be found.";
#endif
  return flash;
}

bool SetPluginEnabled(PluginPrefs* plugin_prefs,
                      const content::WebPluginInfo* plugin,
                      bool enabled) {
  bool ok = false;
  plugin_prefs->EnablePlugin(enabled, plugin->path,
                             base::Bind(CopyValueAndQuit<bool>, &ok));
  content::RunMessageLoop();
  return ok;
}

int CountPluginsOnIOThread() {
  int count = 0;
  for (content::BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    if (iter.GetData().process_type == content::PROCESS_TYPE_PLUGIN ||
        iter.GetData().process_type == content::PROCESS_TYPE_PPAPI_PLUGIN) {
      count++;
    }
  }
  return count;
}

int CountPlugins() {
  int count = -1;
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::Bind(CountPluginsOnIOThread),
      base::Bind(CopyValueAndQuit<int>, &count));
  content::RunMessageLoop();
  EXPECT_GE(count, 0);
  return count;
}

void FlushBlacklistPolicy() {
  // Updates of the URLBlacklist are done on IO, after building the blacklist
  // on FILE, which is initiated from IO.
  content::RunAllPendingInMessageLoop(BrowserThread::IO);
  content::RunAllPendingInMessageLoop(BrowserThread::FILE);
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

  virtual ~TestAudioObserver() {}

 protected:
  // chromeos::CrasAudioHandler::AudioObserver overrides.
  virtual void OnOutputMuteChanged() OVERRIDE {
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
  virtual ~WebContentsLoadedOrDestroyedWatcher();

  // Waits until the WebContents's load is done or until it is destroyed.
  void Wait();

  // Overridden WebContentsObserver methods.
  virtual void WebContentsDestroyed() OVERRIDE;
  virtual void DidStopLoading(
      content::RenderViewHost* render_view_host) OVERRIDE;

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

void WebContentsLoadedOrDestroyedWatcher::DidStopLoading(
    content::RenderViewHost* render_view_host) {
  message_loop_runner_->Quit();
}

#if !defined(OS_MACOSX)

// Observer used to wait for the creation of a new app window.
class TestAddAppWindowObserver : public apps::AppWindowRegistry::Observer {
 public:
  explicit TestAddAppWindowObserver(apps::AppWindowRegistry* registry);
  virtual ~TestAddAppWindowObserver();

  // apps::AppWindowRegistry::Observer:
  virtual void OnAppWindowAdded(apps::AppWindow* app_window) OVERRIDE;

  apps::AppWindow* WaitForAppWindow();

 private:
  apps::AppWindowRegistry* registry_;  // Not owned.
  apps::AppWindow* window_;            // Not owned.
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestAddAppWindowObserver);
};

TestAddAppWindowObserver::TestAddAppWindowObserver(
    apps::AppWindowRegistry* registry)
    : registry_(registry), window_(NULL) {
  registry_->AddObserver(this);
}

TestAddAppWindowObserver::~TestAddAppWindowObserver() {
  registry_->RemoveObserver(this);
}

void TestAddAppWindowObserver::OnAppWindowAdded(apps::AppWindow* app_window) {
  window_ = app_window;
  run_loop_.Quit();
}

apps::AppWindow* TestAddAppWindowObserver::WaitForAppWindow() {
  run_loop_.Run();
  return window_;
}

#endif

}  // namespace

class PolicyTest : public InProcessBrowserTest {
 protected:
  PolicyTest() {}
  virtual ~PolicyTest() {}

  virtual void SetUp() OVERRIDE {
    test_extension_cache_.reset(new extensions::ExtensionCacheFake());
    InProcessBrowserTest::SetUp();
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch("noerrdialogs");
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(chrome_browser_net::SetUrlRequestMocksEnabled, true));
  }

  // Makes URLRequestMockHTTPJobs serve data from content::DIR_TEST_DATA
  // instead of chrome::DIR_TEST_DATA.
  void ServeContentTestData() {
    base::FilePath root_http;
    PathService::Get(content::DIR_TEST_DATA, &root_http);
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO, FROM_HERE,
        base::Bind(URLRequestMockHTTPJob::AddUrlHandler, root_http),
        base::MessageLoop::current()->QuitWhenIdleClosure());
    content::RunMessageLoop();
  }

  void SetScreenshotPolicy(bool enabled) {
    PolicyMap policies;
    policies.Set(key::kDisableScreenshots,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 new base::FundamentalValue(!enabled),
                 NULL);
    UpdateProviderPolicy(policies);
  }

#if defined(OS_CHROMEOS)
  class QuitMessageLoopAfterScreenshot : public ScreenshotTakerObserver {
   public:
    virtual void OnScreenshotCompleted(
        ScreenshotTakerObserver::Result screenshot_result,
        const base::FilePath& screenshot_path) OVERRIDE {
      BrowserThread::PostTaskAndReply(BrowserThread::IO,
                                      FROM_HERE,
                                      base::Bind(base::DoNothing),
                                      base::MessageLoop::QuitClosure());
    }

    virtual ~QuitMessageLoopAfterScreenshot() {}
  };

  void TestScreenshotFile(bool enabled) {
    // AddObserver is an ash-specific method, so just replace the screenshot
    // taker with one we've created here.
    scoped_ptr<ScreenshotTaker> screenshot_taker(new ScreenshotTaker);
    // ScreenshotTaker doesn't own this observer, so the observer's lifetime
    // is tied to the test instead.
    screenshot_taker->AddObserver(&observer_);
    ash::Shell::GetInstance()->accelerator_controller()->SetScreenshotDelegate(
        screenshot_taker.PassAs<ash::ScreenshotDelegate>());

    SetScreenshotPolicy(enabled);
    ash::Shell::GetInstance()->accelerator_controller()->PerformAction(
        ash::TAKE_SCREENSHOT, ui::Accelerator());

    content::RunMessageLoop();
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
        chrome::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    installer->InstallCrx(extension_path);
    observer.Wait();
    content::Details<const extensions::Extension> details = observer.details();
    return details.ptr();
  }

  const extensions::Extension* LoadUnpackedExtension(
      const base::FilePath::StringType& name, bool expect_success) {
    base::FilePath extension_path(ui_test_utils::GetTestFilePath(
        base::FilePath(kTestExtensionsDir), base::FilePath(name)));
    scoped_refptr<extensions::UnpackedInstaller> installer =
        extensions::UnpackedInstaller::Create(extension_service());
    content::WindowedNotificationObserver observer(
        expect_success ? chrome::NOTIFICATION_EXTENSION_LOADED_DEPRECATED
                       : chrome::NOTIFICATION_EXTENSION_LOAD_ERROR,
        content::NotificationService::AllSources());
    installer->Load(extension_path);
    observer.Wait();

    const extensions::ExtensionSet* extensions =
        extension_service()->extensions();
    for (extensions::ExtensionSet::const_iterator it = extensions->begin();
         it != extensions->end(); ++it) {
      if ((*it)->path() == extension_path)
        return it->get();
    }
    return NULL;
  }

  void UninstallExtension(const std::string& id, bool expect_success) {
    content::WindowedNotificationObserver observer(
        expect_success ? chrome::NOTIFICATION_EXTENSION_UNINSTALLED_DEPRECATED
                       : chrome::NOTIFICATION_EXTENSION_UNINSTALL_NOT_ALLOWED,
        content::NotificationService::AllSources());
    extension_service()->UninstallExtension(
        id, extensions::UNINSTALL_REASON_FOR_TESTING, NULL);
    observer.Wait();
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
    blink::WebMouseEvent click_event;
    click_event.type = blink::WebInputEvent::MouseDown;
    click_event.button = blink::WebMouseEvent::ButtonLeft;
    click_event.clickCount = 1;
    click_event.x = x;
    click_event.y = y;
    contents->GetRenderViewHost()->ForwardMouseEvent(click_event);
    click_event.type = blink::WebInputEvent::MouseUp;
    contents->GetRenderViewHost()->ForwardMouseEvent(click_event);
  }

  MockConfigurationPolicyProvider provider_;
  scoped_ptr<extensions::ExtensionCacheFake> test_extension_cache_;
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
  virtual ~LocalePolicyTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(key::kApplicationLocaleValue,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 new base::StringValue("fr"),
                 NULL);
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
  std::string loaded =
      ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources("en-US");
  EXPECT_EQ("en-US", loaded);
  base::string16 english_title = l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);
  EXPECT_NE(french_title, english_title);
}
#endif

IN_PROC_BROWSER_TEST_F(PolicyTest, BookmarkBarEnabled) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  // Verifies that the bookmarks bar can be forced to always or never show up.

  // Test starts in about:blank.
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kShowBookmarkBar));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kShowBookmarkBar));
  EXPECT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());

  PolicyMap policies;
  policies.Set(key::kBookmarkBarEnabled,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(true),
               NULL);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kShowBookmarkBar));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kShowBookmarkBar));
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());

  // The NTP has special handling of the bookmark bar.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());

  policies.Set(key::kBookmarkBarEnabled,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(false),
               NULL);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kShowBookmarkBar));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kShowBookmarkBar));
  // The bookmark bar is hidden in the NTP when disabled by policy.
  EXPECT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());

  policies.Clear();
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kShowBookmarkBar));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kShowBookmarkBar));
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
  policies.Set(key::kDefaultCookiesSetting,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(4),
               NULL);
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
  ui_test_utils::WaitForTemplateURLServiceToLoad(service);
  TemplateURL* default_search = service->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);
  EXPECT_NE(kKeyword, default_search->keyword());
  EXPECT_NE(kSearchURL, default_search->url());
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
  policies.Set(key::kDefaultSearchProviderEnabled,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(true),
               NULL);
  policies.Set(key::kDefaultSearchProviderKeyword,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::StringValue(kKeyword),
               NULL);
  policies.Set(key::kDefaultSearchProviderSearchURL,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::StringValue(kSearchURL),
               NULL);
  base::ListValue* alternate_urls = new base::ListValue();
  alternate_urls->AppendString(kAlternateURL0);
  alternate_urls->AppendString(kAlternateURL1);
  policies.Set(key::kDefaultSearchProviderAlternateURLs, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, alternate_urls, NULL);
  policies.Set(key::kDefaultSearchProviderSearchTermsReplacementKey,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::StringValue(kSearchTermsReplacementKey),
               NULL);
  policies.Set(key::kDefaultSearchProviderImageURL,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::StringValue(kImageURL),
               NULL);
  policies.Set(key::kDefaultSearchProviderImageURLPostParams,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::StringValue(kImageURLPostParams),
               NULL);
  policies.Set(key::kDefaultSearchProviderNewTabURL,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::StringValue(kNewTabURL),
               NULL);
  UpdateProviderPolicy(policies);
  default_search = service->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);
  EXPECT_EQ(kKeyword, default_search->keyword());
  EXPECT_EQ(kSearchURL, default_search->url());
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
  policies.Set(key::kDefaultSearchProviderEnabled,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(false),
               NULL);
  EXPECT_TRUE(service->GetDefaultSearchProvider());
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(service->GetDefaultSearchProvider());
  ui_test_utils::SendToOmniboxAndSubmit(location_bar, "should not work");
  // This means that submitting won't trigger any action.
  EXPECT_FALSE(model->CurrentMatch(NULL).destination_url.is_valid());
  EXPECT_EQ(GURL(url::kAboutBlankURL), web_contents->GetURL());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, PolicyPreprocessing) {
  // Add an individual proxy policy value.
  PolicyMap policies;
  policies.Set(key::kProxyServerMode,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(3),
               NULL);
  UpdateProviderPolicy(policies);

  // It should be removed and replaced with a dictionary.
  PolicyMap expected;
  scoped_ptr<base::DictionaryValue> expected_value(new base::DictionaryValue);
  expected_value->SetInteger(key::kProxyServerMode, 3);
  expected.Set(key::kProxySettings,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               expected_value.release(),
               NULL);

  // Check both the browser and the profile.
  const PolicyMap& actual_from_browser =
      g_browser_process->browser_policy_connector()
          ->GetPolicyService()
          ->GetPolicies(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  EXPECT_TRUE(expected.Equals(actual_from_browser));
  const PolicyMap& actual_from_profile =
      ProfilePolicyConnectorFactory::GetForProfile(browser()->profile())
          ->policy_service()
          ->GetPolicies(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  EXPECT_TRUE(expected.Equals(actual_from_profile));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, ForceSafeSearch) {
  // Makes the requests fail since all we want to check is that the redirection
  // is done properly.
  MakeRequestFail make_request_fail("google.com");

  // Verifies that requests to Google Search engine with the SafeSearch
  // enabled set the safe=active&ssui=on parameters at the end of the query.
  TemplateURLService* service = TemplateURLServiceFactory::GetForProfile(
      browser()->profile());
  ui_test_utils::WaitForTemplateURLServiceToLoad(service);

  // First check that nothing happens.
  content::TestNavigationObserver no_safesearch_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  chrome::FocusLocationBar(browser());
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  ui_test_utils::SendToOmniboxAndSubmit(location_bar, "http://google.com/");
  OmniboxEditModel* model = location_bar->GetOmniboxView()->model();
  no_safesearch_observer.Wait();
  EXPECT_TRUE(model->CurrentMatch(NULL).destination_url.is_valid());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL expected_without("http://google.com/");
  EXPECT_EQ(expected_without, web_contents->GetURL());

  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kForceSafeSearch));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kForceSafeSearch));

  // Override the default SafeSearch setting using policies.
  PolicyMap policies;
  policies.Set(key::kForceSafeSearch,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(true),
               NULL);
  UpdateProviderPolicy(policies);

  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kForceSafeSearch));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kForceSafeSearch));

  content::TestNavigationObserver safesearch_observer(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Verify that searching from google.com works.
  chrome::FocusLocationBar(browser());
  ui_test_utils::SendToOmniboxAndSubmit(location_bar, "http://google.com/");
  safesearch_observer.Wait();
  EXPECT_TRUE(model->CurrentMatch(NULL).destination_url.is_valid());
  web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  std::string expected_url("http://google.com/?");
  expected_url += std::string(chrome::kSafeSearchSafeParameter) + "&" +
                  chrome::kSafeSearchSsuiParameter;
  GURL expected_with_parameters(expected_url);
  EXPECT_EQ(expected_with_parameters, web_contents->GetURL());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, ReplaceSearchTerms) {
  MakeRequestFail make_request_fail("search.example");

  chrome::EnableQueryExtractionForTesting();

  // Verifies that a default search is made using the provider configured via
  // policy. Also checks that default search can be completely disabled.
  const base::string16 kKeyword(base::ASCIIToUTF16("testsearch"));
  const std::string kSearchURL("https://www.google.com/search?q={searchTerms}");
  const std::string kInstantURL("http://does/not/exist");
  const std::string kAlternateURL0(
      "https://www.google.com/search#q={searchTerms}");
  const std::string kAlternateURL1("https://www.google.com/#q={searchTerms}");
  const std::string kSearchTermsReplacementKey(
      "{google:instantExtendedEnabledKey}");

  TemplateURLService* service = TemplateURLServiceFactory::GetForProfile(
      browser()->profile());
  ui_test_utils::WaitForTemplateURLServiceToLoad(service);
  TemplateURL* default_search = service->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);
  EXPECT_NE(kKeyword, default_search->keyword());
  EXPECT_NE(kSearchURL, default_search->url());
  EXPECT_NE(kInstantURL, default_search->instant_url());
  EXPECT_FALSE(
    default_search->alternate_urls().size() == 2 &&
    default_search->alternate_urls()[0] == kAlternateURL0 &&
    default_search->alternate_urls()[1] == kAlternateURL1);

  // Override the default search provider using policies.
  PolicyMap policies;
  policies.Set(key::kDefaultSearchProviderEnabled,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(true),
               NULL);
  policies.Set(key::kDefaultSearchProviderKeyword,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::StringValue(kKeyword),
               NULL);
  policies.Set(key::kDefaultSearchProviderSearchURL,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::StringValue(kSearchURL),
               NULL);
  policies.Set(key::kDefaultSearchProviderInstantURL,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::StringValue(kInstantURL),
               NULL);
  base::ListValue* alternate_urls = new base::ListValue();
  alternate_urls->AppendString(kAlternateURL0);
  alternate_urls->AppendString(kAlternateURL1);
  policies.Set(key::kDefaultSearchProviderAlternateURLs, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, alternate_urls, NULL);
  policies.Set(key::kDefaultSearchProviderSearchTermsReplacementKey,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::StringValue(kSearchTermsReplacementKey),
               NULL);
  UpdateProviderPolicy(policies);
  default_search = service->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);
  EXPECT_EQ(kKeyword, default_search->keyword());
  EXPECT_EQ(kSearchURL, default_search->url());
  EXPECT_EQ(kInstantURL, default_search->instant_url());
  EXPECT_EQ(2U, default_search->alternate_urls().size());
  EXPECT_EQ(kAlternateURL0, default_search->alternate_urls()[0]);
  EXPECT_EQ(kAlternateURL1, default_search->alternate_urls()[1]);

  // Query terms replacement requires that the renderer process be a recognized
  // Instant renderer. Fake it.
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  instant_service->AddInstantProcess(browser()->tab_strip_model()->
      GetActiveWebContents()->GetRenderProcessHost()->GetID());

  // Verify that searching from the omnibox does search term replacement with
  // first URL pattern.
  chrome::FocusLocationBar(browser());
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox_view = location_bar->GetOmniboxView();
  ui_test_utils::SendToOmniboxAndSubmit(location_bar,
      "https://www.google.com/?espv=1#q=foobar");
  EXPECT_TRUE(
      browser()->toolbar_model()->WouldPerformSearchTermReplacement(false));
  EXPECT_EQ(base::ASCIIToUTF16("foobar"), omnibox_view->GetText());

  // Verify that not using espv=1 does not do search term replacement.
  chrome::FocusLocationBar(browser());
  ui_test_utils::SendToOmniboxAndSubmit(location_bar,
      "https://www.google.com/?q=foobar");
  EXPECT_FALSE(
      browser()->toolbar_model()->WouldPerformSearchTermReplacement(false));
  EXPECT_EQ(base::ASCIIToUTF16("https://www.google.com/?q=foobar"),
            omnibox_view->GetText());

  // Verify that searching from the omnibox does search term replacement with
  // second URL pattern.
  chrome::FocusLocationBar(browser());
  ui_test_utils::SendToOmniboxAndSubmit(location_bar,
      "https://www.google.com/search?espv=1#q=banana");
  EXPECT_TRUE(
      browser()->toolbar_model()->WouldPerformSearchTermReplacement(false));
  EXPECT_EQ(base::ASCIIToUTF16("banana"), omnibox_view->GetText());

  // Verify that searching from the omnibox does search term replacement with
  // standard search URL pattern.
  chrome::FocusLocationBar(browser());
  ui_test_utils::SendToOmniboxAndSubmit(location_bar,
      "https://www.google.com/search?q=tractor+parts&espv=1");
  EXPECT_TRUE(
      browser()->toolbar_model()->WouldPerformSearchTermReplacement(false));
  EXPECT_EQ(base::ASCIIToUTF16("tractor parts"), omnibox_view->GetText());

  // Verify that searching from the omnibox prioritizes hash over query.
  chrome::FocusLocationBar(browser());
  ui_test_utils::SendToOmniboxAndSubmit(location_bar,
      "https://www.google.com/search?q=tractor+parts&espv=1#q=foobar");
  EXPECT_TRUE(
      browser()->toolbar_model()->WouldPerformSearchTermReplacement(false));
  EXPECT_EQ(base::ASCIIToUTF16("foobar"), omnibox_view->GetText());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, Disable3DAPIs) {
  // This test assumes Gpu access.
  if (!content::GpuDataManager::GetInstance()->GpuAccessAllowed(NULL))
    return;

  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  // WebGL is enabled by default.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsWebGLEnabled(contents));
  // Disable with a policy.
  PolicyMap policies;
  policies.Set(key::kDisable3DAPIs,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(true),
               NULL);
  UpdateProviderPolicy(policies);
  // Crash and reload the tab to get a new renderer.
  content::CrashTab(contents);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_RELOAD));
  EXPECT_FALSE(IsWebGLEnabled(contents));
  // Enable with a policy.
  policies.Set(key::kDisable3DAPIs,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(false),
               NULL);
  UpdateProviderPolicy(policies);
  content::CrashTab(contents);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_RELOAD));
  EXPECT_TRUE(IsWebGLEnabled(contents));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DisableSpdy) {
  // Verifies that SPDY can be disable by policy.
  EXPECT_TRUE(net::HttpStreamFactory::spdy_enabled());
  PolicyMap policies;
  policies.Set(key::kDisableSpdy,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(true),
               NULL);
  UpdateProviderPolicy(policies);
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(net::HttpStreamFactory::spdy_enabled());
  // Verify that it can be force-enabled too.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kDisableSpdy, true);
  policies.Set(key::kDisableSpdy,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(false),
               NULL);
  UpdateProviderPolicy(policies);
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(net::HttpStreamFactory::spdy_enabled());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DisabledPlugins) {
  // Verifies that plugins can be forced to be disabled by policy.

  // Verify that the Flash plugin exists and that it can be enabled and disabled
  // by the user.
  std::vector<content::WebPluginInfo> plugins;
  GetPluginList(&plugins);
  const content::WebPluginInfo* flash = GetFlashPlugin(plugins);
  if (!flash)
    return;
  PluginPrefs* plugin_prefs =
      PluginPrefs::GetForProfile(browser()->profile()).get();
  EXPECT_TRUE(plugin_prefs->IsPluginEnabled(*flash));
  EXPECT_TRUE(SetPluginEnabled(plugin_prefs, flash, false));
  EXPECT_FALSE(plugin_prefs->IsPluginEnabled(*flash));
  EXPECT_TRUE(SetPluginEnabled(plugin_prefs, flash, true));
  EXPECT_TRUE(plugin_prefs->IsPluginEnabled(*flash));

  // Now disable it with a policy.
  base::ListValue disabled_plugins;
  disabled_plugins.Append(new base::StringValue("*Flash*"));
  PolicyMap policies;
  policies.Set(key::kDisabledPlugins, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, disabled_plugins.DeepCopy(), NULL);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(plugin_prefs->IsPluginEnabled(*flash));
  // The user shouldn't be able to enable it.
  EXPECT_FALSE(SetPluginEnabled(plugin_prefs, flash, true));
  EXPECT_FALSE(plugin_prefs->IsPluginEnabled(*flash));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DisabledPluginsExceptions) {
  // Verifies that plugins with an exception in the blacklist can be enabled.

  // Verify that the Flash plugin exists and that it can be enabled and disabled
  // by the user.
  std::vector<content::WebPluginInfo> plugins;
  GetPluginList(&plugins);
  const content::WebPluginInfo* flash = GetFlashPlugin(plugins);
  if (!flash)
    return;
  PluginPrefs* plugin_prefs =
      PluginPrefs::GetForProfile(browser()->profile()).get();
  EXPECT_TRUE(plugin_prefs->IsPluginEnabled(*flash));

  // Disable all plugins.
  base::ListValue disabled_plugins;
  disabled_plugins.Append(new base::StringValue("*"));
  PolicyMap policies;
  policies.Set(key::kDisabledPlugins, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, disabled_plugins.DeepCopy(), NULL);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(plugin_prefs->IsPluginEnabled(*flash));
  // The user shouldn't be able to enable it.
  EXPECT_FALSE(SetPluginEnabled(plugin_prefs, flash, true));
  EXPECT_FALSE(plugin_prefs->IsPluginEnabled(*flash));

  // Now open an exception for flash.
  base::ListValue disabled_plugins_exceptions;
  disabled_plugins_exceptions.Append(new base::StringValue("*Flash*"));
  policies.Set(key::kDisabledPluginsExceptions, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, disabled_plugins_exceptions.DeepCopy(), NULL);
  UpdateProviderPolicy(policies);
  // It should revert to the user's preference automatically.
  EXPECT_TRUE(plugin_prefs->IsPluginEnabled(*flash));
  // And the user should be able to disable and enable again.
  EXPECT_TRUE(SetPluginEnabled(plugin_prefs, flash, false));
  EXPECT_FALSE(plugin_prefs->IsPluginEnabled(*flash));
  EXPECT_TRUE(SetPluginEnabled(plugin_prefs, flash, true));
  EXPECT_TRUE(plugin_prefs->IsPluginEnabled(*flash));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, EnabledPlugins) {
  // Verifies that a plugin can be force-installed with a policy.
  std::vector<content::WebPluginInfo> plugins;
  GetPluginList(&plugins);
  const content::WebPluginInfo* flash = GetFlashPlugin(plugins);
  if (!flash)
    return;
  PluginPrefs* plugin_prefs =
      PluginPrefs::GetForProfile(browser()->profile()).get();
  EXPECT_TRUE(plugin_prefs->IsPluginEnabled(*flash));

  // The user disables it and then a policy forces it to be enabled.
  EXPECT_TRUE(SetPluginEnabled(plugin_prefs, flash, false));
  EXPECT_FALSE(plugin_prefs->IsPluginEnabled(*flash));
  base::ListValue plugin_list;
  plugin_list.Append(new base::StringValue(content::kFlashPluginName));
  PolicyMap policies;
  policies.Set(key::kEnabledPlugins, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, plugin_list.DeepCopy(), NULL);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(plugin_prefs->IsPluginEnabled(*flash));
  // The user can't disable it anymore.
  EXPECT_FALSE(SetPluginEnabled(plugin_prefs, flash, false));
  EXPECT_TRUE(plugin_prefs->IsPluginEnabled(*flash));

  // When a plugin is both enabled and disabled, the whitelist takes precedence.
  policies.Set(key::kDisabledPlugins, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, plugin_list.DeepCopy(), NULL);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(plugin_prefs->IsPluginEnabled(*flash));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, AlwaysAuthorizePlugins) {
  // Verifies that dangerous plugins can be always authorized to run with
  // a policy.

  // Verify that the test page exists. It is only present in checkouts with
  // src-internal.
  if (!base::PathExists(ui_test_utils::GetTestFilePath(
      base::FilePath(FILE_PATH_LITERAL("plugin")),
      base::FilePath(FILE_PATH_LITERAL("quicktime.html"))))) {
    LOG(INFO) <<
        "Test skipped because plugin/quicktime.html test file wasn't found.";
    return;
  }

  ServeContentTestData();
  // No plugins at startup.
  EXPECT_EQ(0, CountPlugins());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  InfoBarService* infobar_service = InfoBarService::FromWebContents(contents);
  ASSERT_TRUE(infobar_service);
  EXPECT_EQ(0u, infobar_service->infobar_count());

  base::FilePath path(FILE_PATH_LITERAL("plugin/quicktime.html"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(path));
  ui_test_utils::NavigateToURL(browser(), url);
  // This should have triggered the dangerous plugin infobar.
  ASSERT_EQ(1u, infobar_service->infobar_count());
  EXPECT_TRUE(
      infobar_service->infobar_at(0)->delegate()->AsConfirmInfoBarDelegate());
  // And the plugin isn't running.
  EXPECT_EQ(0, CountPlugins());

  // Now set a policy to always authorize this.
  PolicyMap policies;
  policies.Set(key::kAlwaysAuthorizePlugins,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(true),
               NULL);
  UpdateProviderPolicy(policies);
  // Reloading the page shouldn't trigger the infobar this time.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(0u, infobar_service->infobar_count());
  // And the plugin started automatically.
  EXPECT_EQ(1, CountPlugins());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DeveloperToolsDisabled) {
  // Verifies that access to the developer tools can be disabled.

  // Open devtools.
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_DEV_TOOLS));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DevToolsWindow *devtools_window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);
  EXPECT_TRUE(devtools_window);

  // Disable devtools via policy.
  PolicyMap policies;
  policies.Set(key::kDeveloperToolsDisabled,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(true),
               NULL);
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

// TODO(samarth): remove along with rest of NTP4 code.
IN_PROC_BROWSER_TEST_F(PolicyTest, DISABLED_WebStoreIconHidden) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

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
  policies.Set(key::kHideWebStoreIcon,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(true),
               NULL);
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
  base::ScopedTempDir initial_dir;
  ASSERT_TRUE(initial_dir.CreateUniqueTempDir());
  browser()->profile()->GetPrefs()->SetFilePath(
      prefs::kDownloadDefaultDirectory, initial_dir.path());
  // Don't prompt for the download location during this test.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPromptForDownload, false);

  // Verify that downloads end up on the default directory.
  base::FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  DownloadAndVerifyFile(browser(), initial_dir.path(), file);
  base::DieFileDie(initial_dir.path().Append(file), false);

  // Override the download directory with the policy and verify a download.
  base::ScopedTempDir forced_dir;
  ASSERT_TRUE(forced_dir.CreateUniqueTempDir());
  PolicyMap policies;
  policies.Set(key::kDownloadDirectory,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::StringValue(forced_dir.path().value()),
               NULL);
  UpdateProviderPolicy(policies);
  DownloadAndVerifyFile(browser(), forced_dir.path(), file);
  // Verify that the first download location wasn't affected.
  EXPECT_FALSE(base::PathExists(initial_dir.path().Append(file)));
}

// Flaky: http://crbug.com/388340
IN_PROC_BROWSER_TEST_F(PolicyTest,
                       DISABLED_ExtensionInstallBlacklistSelective) {
  // Verifies that blacklisted extensions can't be installed.
  ExtensionService* service = extension_service();
  ASSERT_FALSE(service->GetExtensionById(kGoodCrxId, true));
  ASSERT_FALSE(service->GetExtensionById(kAdBlockCrxId, true));
  base::ListValue blacklist;
  blacklist.Append(new base::StringValue(kGoodCrxId));
  PolicyMap policies;
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, blacklist.DeepCopy(), NULL);
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
  blacklist.Append(new base::StringValue("*"));
  PolicyMap policies;
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, blacklist.DeepCopy(), NULL);
  UpdateProviderPolicy(policies);

  // AdBlock was automatically removed.
  ASSERT_FALSE(service->GetExtensionById(kAdBlockCrxId, true));

  // And can't be installed again, nor can good.crx.
  EXPECT_FALSE(InstallExtension(kAdBlockCrxName));
  EXPECT_FALSE(service->GetExtensionById(kAdBlockCrxId, true));
  EXPECT_FALSE(InstallExtension(kGoodCrxName));
  EXPECT_FALSE(service->GetExtensionById(kGoodCrxId, true));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, ExtensionInstallWhitelist) {
  // Verifies that the whitelist can open exceptions to the blacklist.
  ExtensionService* service = extension_service();
  ASSERT_FALSE(service->GetExtensionById(kGoodCrxId, true));
  ASSERT_FALSE(service->GetExtensionById(kAdBlockCrxId, true));
  base::ListValue blacklist;
  blacklist.Append(new base::StringValue("*"));
  base::ListValue whitelist;
  whitelist.Append(new base::StringValue(kGoodCrxId));
  PolicyMap policies;
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, blacklist.DeepCopy(), NULL);
  policies.Set(key::kExtensionInstallWhitelist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, whitelist.DeepCopy(), NULL);
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
  GURL url(URLRequestMockHTTPJob::GetMockUrl(path));

  // Setting the forcelist extension should install "good_v1.crx".
  base::ListValue forcelist;
  forcelist.Append(new base::StringValue(
      base::StringPrintf("%s;%s", kGoodCrxId, url.spec().c_str())));
  PolicyMap policies;
  policies.Set(key::kExtensionInstallForcelist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, forcelist.DeepCopy(), NULL);
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED,
      content::NotificationService::AllSources());
  UpdateProviderPolicy(policies);
  observer.Wait();
  // Note: Cannot check that the notification details match the expected
  // exception, since the details object has already been freed prior to
  // the completion of observer.Wait().

  EXPECT_TRUE(service->GetExtensionById(kGoodCrxId, true));

  // The user is not allowed to uninstall force-installed extensions.
  UninstallExtension(kGoodCrxId, false);

  // The user is not allowed to load an unpacked extension with the
  // same ID as a force-installed extension.
  LoadUnpackedExtension(kGoodUnpackedExt, false);

  // Loading other unpacked extensions are not blocked.
  LoadUnpackedExtension(kAppUnpackedExt, true);

  const std::string old_version_number =
      service->GetExtensionById(kGoodCrxId, true)->version()->GetString();

  base::FilePath test_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_path));

  TestRequestInterceptor interceptor(
      "update.extension",
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
  interceptor.PushJobCallback(
      TestRequestInterceptor::FileJob(
          test_path.Append(kTestExtensionsDir).Append(kGood2CrxManifestName)));

  // Updating the force-installed extension.
  extensions::ExtensionUpdater* updater = service->updater();
  extensions::ExtensionUpdater::CheckParams params;
  params.install_immediately = true;
  content::WindowedNotificationObserver update_observer(
      chrome::NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED,
      content::NotificationService::AllSources());
  updater->CheckNow(params);
  update_observer.Wait();

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
      extensions::ExtensionSystem::Get(browser()->profile())->process_manager();
  extensions::ProcessManager::ViewSet all_views = manager->GetAllViews();
  for (extensions::ProcessManager::ViewSet::const_iterator iter =
           all_views.begin();
       iter != all_views.end();) {
    if (!(*iter)->IsLoading()) {
      ++iter;
    } else {
      content::WebContents* web_contents =
          content::WebContents::FromRenderViewHost(*iter);
      ASSERT_TRUE(web_contents);
      WebContentsLoadedOrDestroyedWatcher(web_contents).Wait();

      // Test activity may have modified the set of extension processes during
      // message processing, so re-start the iteration to catch added/removed
      // processes.
      all_views = manager->GetAllViews();
      iter = all_views.begin();
    }
  }

  // Test policy-installed extensions are reloaded when killed.
  BackgroundContentsService::
      SetRestartDelayForForceInstalledAppsAndExtensionsForTesting(0);
  content::WindowedNotificationObserver extension_crashed_observer(
      chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
      content::NotificationService::AllSources());
  content::WindowedNotificationObserver extension_loaded_observer(
      chrome::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
      content::NotificationService::AllSources());
  extensions::ExtensionHost* extension_host =
      extensions::ExtensionSystem::Get(browser()->profile())->
          process_manager()->GetBackgroundHostForExtension(kGoodCrxId);
  base::KillProcess(extension_host->render_process_host()->GetHandle(),
                    content::RESULT_CODE_KILLED, false);
  extension_crashed_observer.Wait();
  extension_loaded_observer.Wait();
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
               POLICY_SCOPE_USER, allowed_types.DeepCopy(), NULL);
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
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAppsGalleryInstallAutoConfirmForTests, "accept");

  const GURL install_source_url(URLRequestMockHTTPJob::GetMockUrl(
      base::FilePath(FILE_PATH_LITERAL("extensions/*"))));
  const GURL referrer_url(URLRequestMockHTTPJob::GetMockUrl(
      base::FilePath(FILE_PATH_LITERAL("policy/*"))));

  base::ScopedTempDir download_directory;
  ASSERT_TRUE(download_directory.CreateUniqueTempDir());
  DownloadPrefs* download_prefs =
      DownloadPrefs::FromBrowserContext(browser()->profile());
  download_prefs->SetDownloadPath(download_directory.path());

  const GURL download_page_url(URLRequestMockHTTPJob::GetMockUrl(base::FilePath(
      FILE_PATH_LITERAL("policy/extension_install_sources_test.html"))));
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
               POLICY_SCOPE_USER, install_sources.DeepCopy(), NULL);
  UpdateProviderPolicy(policies);

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED,
      content::NotificationService::AllSources());
  PerformClick(1, 0);
  observer.Wait();
  // Note: Cannot check that the notification details match the expected
  // exception, since the details object has already been freed prior to
  // the completion of observer.Wait().

  // The first extension shouldn't be present, the second should be there.
  EXPECT_FALSE(extension_service()->GetExtensionById(kGoodCrxId, true));
  EXPECT_TRUE(extension_service()->GetExtensionById(kAdBlockCrxId, false));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, HomepageLocation) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

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
  policies.Set(key::kHomepageLocation,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::StringValue(chrome::kChromeUICreditsURL),
               NULL);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_HOME));
  content::WaitForLoadStop(contents);
  EXPECT_EQ(GURL(chrome::kChromeUICreditsURL), contents->GetURL());

  policies.Set(key::kHomepageIsNewTabPage,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(true),
               NULL);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_HOME));
  content::WaitForLoadStop(contents);
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), contents->GetURL());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, IncognitoEnabled) {
  // Verifies that incognito windows can't be opened when disabled by policy.

  const BrowserList* active_browser_list =
      BrowserList::GetInstance(chrome::GetActiveDesktop());

  // Disable incognito via policy and verify that incognito windows can't be
  // opened.
  EXPECT_EQ(1u, active_browser_list->size());
  EXPECT_FALSE(BrowserList::IsOffTheRecordSessionActive());
  PolicyMap policies;
  policies.Set(key::kIncognitoEnabled,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(false),
               NULL);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(chrome::ExecuteCommand(browser(), IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_EQ(1u, active_browser_list->size());
  EXPECT_FALSE(BrowserList::IsOffTheRecordSessionActive());

  // Enable via policy and verify that incognito windows can be opened.
  policies.Set(key::kIncognitoEnabled,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(true),
               NULL);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_EQ(2u, active_browser_list->size());
  EXPECT_TRUE(BrowserList::IsOffTheRecordSessionActive());
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
  policies.Set(key::kJavascriptEnabled,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(false),
               NULL);
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
  policies.Set(key::kDefaultJavaScriptSetting,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(CONTENT_SETTING_ALLOW),
               NULL);
  UpdateProviderPolicy(policies);
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  EXPECT_TRUE(IsJavascriptEnabled(contents));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, SavingBrowserHistoryDisabled) {
  // Verifies that browsing history is not saved.
  PolicyMap policies;
  policies.Set(key::kSavingBrowserHistoryDisabled,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(true),
               NULL);
  UpdateProviderPolicy(policies);
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("empty.html")));
  ui_test_utils::NavigateToURL(browser(), url);
  // Verify that the navigation wasn't saved in the history.
  ui_test_utils::HistoryEnumerator enumerator1(browser()->profile());
  EXPECT_EQ(0u, enumerator1.urls().size());

  // Now flip the policy and try again.
  policies.Set(key::kSavingBrowserHistoryDisabled,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(false),
               NULL);
  UpdateProviderPolicy(policies);
  ui_test_utils::NavigateToURL(browser(), url);
  // Verify that the navigation was saved in the history.
  ui_test_utils::HistoryEnumerator enumerator2(browser()->profile());
  ASSERT_EQ(1u, enumerator2.urls().size());
  EXPECT_EQ(url, enumerator2.urls()[0]);
}

// http://crbug.com/241691 PolicyTest.TranslateEnabled is failing regularly.
IN_PROC_BROWSER_TEST_F(PolicyTest, DISABLED_TranslateEnabled) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  scoped_ptr<test::CldDataHarness> cld_data_scope =
      test::CreateCldDataHarness();
  ASSERT_NO_FATAL_FAILURE(cld_data_scope->Init());

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
  policies.Set(key::kTranslateEnabled,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(true),
               NULL);
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
  policies.Set(key::kTranslateEnabled,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(false),
               NULL);
  UpdateProviderPolicy(policies);
  // Navigating to the same URL now doesn't trigger an infobar.
  content::WindowedNotificationObserver language_observer2(
      chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(browser(), url);
  language_observer2.Wait();
  EXPECT_EQ(0u, infobar_service->infobar_count());
}

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
  };
  {
    base::RunLoop loop;
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO, FROM_HERE,
        base::Bind(RedirectHostsToTestData, kURLS, arraysize(kURLS)),
        loop.QuitClosure());
    loop.Run();
  }

  // Verify that "bbb.com" opens before applying the blacklist.
  CheckCanOpenURL(browser(), kURLS[1]);

  // Set a blacklist.
  base::ListValue blacklist;
  blacklist.Append(new base::StringValue("bbb.com"));
  PolicyMap policies;
  policies.Set(key::kURLBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, blacklist.DeepCopy(), NULL);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();
  // All bbb.com URLs are blocked, and "aaa.com" is still unblocked.
  CheckCanOpenURL(browser(), kURLS[0]);
  for (size_t i = 1; i < arraysize(kURLS); ++i)
    CheckURLIsBlocked(browser(), kURLS[i]);

  // Whitelist some sites of bbb.com.
  base::ListValue whitelist;
  whitelist.Append(new base::StringValue("sub.bbb.com"));
  whitelist.Append(new base::StringValue("bbb.com/policy"));
  policies.Set(key::kURLWhitelist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, whitelist.DeepCopy(), NULL);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();
  CheckURLIsBlocked(browser(), kURLS[1]);
  CheckCanOpenURL(browser(), kURLS[2]);
  CheckCanOpenURL(browser(), kURLS[3]);

  {
    base::RunLoop loop;
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO, FROM_HERE,
        base::Bind(UndoRedirectHostsToTestData, kURLS, arraysize(kURLS)),
        loop.QuitClosure());
    loop.Run();
  }
}

// This test is flaky on all platforms; see http://crbug.com/339240
IN_PROC_BROWSER_TEST_F(PolicyTest, DISABLED_FileURLBlacklist) {
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
  blacklist.Append(new base::StringValue("file://*"));
  PolicyMap policies;
  policies.Set(key::kURLBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, blacklist.DeepCopy(), NULL);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();

  CheckURLIsBlocked(browser(), file_path1.c_str());
  CheckURLIsBlocked(browser(), file_path2.c_str());

  // Replace the URLblacklist with disabling the file scheme.
  blacklist.Remove(base::StringValue("file://*"), NULL);
  policies.Set(key::kURLBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, blacklist.DeepCopy(), NULL);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();

  PrefService* prefs = browser()->profile()->GetPrefs();
  const base::ListValue* list_url = prefs->GetList(policy_prefs::kUrlBlacklist);
  EXPECT_EQ(list_url->Find(base::StringValue("file://*")),
            list_url->end());

  base::ListValue disabledscheme;
  disabledscheme.Append(new base::StringValue("file"));
  policies.Set(key::kDisabledSchemes, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, disabledscheme.DeepCopy(), NULL);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();

  list_url = prefs->GetList(policy_prefs::kUrlBlacklist);
  EXPECT_NE(list_url->Find(base::StringValue("file://*")),
            list_url->end());

  // Whitelist one folder and blacklist an another just inside.
  base::ListValue whitelist;
  whitelist.Append(new base::StringValue(base_path));
  policies.Set(key::kURLWhitelist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, whitelist.DeepCopy(), NULL);
  blacklist.Append(new base::StringValue(folder_path));
  policies.Set(key::kURLBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, blacklist.DeepCopy(), NULL);
  UpdateProviderPolicy(policies);
  FlushBlacklistPolicy();

  CheckCanOpenURL(browser(), file_path1.c_str());
  CheckURLIsBlocked(browser(), file_path2.c_str());
}

#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(PolicyTest, FullscreenAllowedBrowser) {
  PolicyMap policies;
  policies.Set(key::kFullscreenAllowed,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(false),
               NULL);
  UpdateProviderPolicy(policies);

  BrowserWindow* browser_window = browser()->window();
  ASSERT_TRUE(browser_window);

  EXPECT_FALSE(browser_window->IsFullscreen());
  chrome::ToggleFullscreenMode(browser());
  EXPECT_FALSE(browser_window->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, FullscreenAllowedApp) {
  PolicyMap policies;
  policies.Set(key::kFullscreenAllowed,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(false),
               NULL);
  UpdateProviderPolicy(policies);

  const extensions::Extension* extension =
      LoadUnpackedExtension(kUnpackedFullscreenAppName, true);
  ASSERT_TRUE(extension);

  // Launch an app that tries to open a fullscreen window.
  TestAddAppWindowObserver add_window_observer(
      apps::AppWindowRegistry::Get(browser()->profile()));
  OpenApplication(AppLaunchParams(browser()->profile(),
                                  extension,
                                  extensions::LAUNCH_CONTAINER_NONE,
                                  NEW_WINDOW));
  apps::AppWindow* window = add_window_observer.WaitForAppWindow();
  ASSERT_TRUE(window);

  // Verify that the window is not in fullscreen mode.
  EXPECT_FALSE(window->GetBaseWindow()->IsFullscreen());

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
IN_PROC_BROWSER_TEST_F(PolicyTest, DisableScreenshotsFile) {
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
  scoped_ptr<TestAudioObserver> test_observer(new TestAudioObserver);
  audio_handler->AddAudioObserver(test_observer.get());

  bool prior_state = audio_handler->IsOutputMuted();
  // Make sure the audio is not muted and then toggle the policy and observe
  // if the output mute changed event is fired.
  audio_handler->SetOutputMute(false);
  EXPECT_FALSE(audio_handler->IsOutputMuted());
  EXPECT_EQ(1, test_observer->output_mute_changed_count());
  PolicyMap policies;
  policies.Set(key::kAudioOutputAllowed,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(false),
               NULL);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(audio_handler->IsOutputMuted());
  // This should not change the state now and should not trigger output mute
  // changed event.
  audio_handler->SetOutputMute(false);
  EXPECT_TRUE(audio_handler->IsOutputMuted());
  EXPECT_EQ(1, test_observer->output_mute_changed_count());

  // Toggle back and observe if the output mute changed event is fired.
  policies.Set(key::kAudioOutputAllowed,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(true),
               NULL);
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

// Disabled, see http://crbug.com/315308.
IN_PROC_BROWSER_TEST_F(PolicyTest, DISABLED_PRE_SessionLengthLimit) {
  // Indicate that the session started 2 hours ago and no user activity has
  // occurred yet.
  g_browser_process->local_state()->SetInt64(
      prefs::kSessionStartTime,
      (base::TimeTicks::Now() - base::TimeDelta::FromHours(2))
          .ToInternalValue());
}

// Disabled, see http://crbug.com/315308.
IN_PROC_BROWSER_TEST_F(PolicyTest, DISABLED_SessionLengthLimit) {
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
  policies.Set(key::kSessionLengthLimit,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(kThreeHoursInMs),
               NULL);
  UpdateProviderPolicy(policies);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Decrease the session length limit to 1 hour. Verify that the session is
  // terminated immediately.
  EXPECT_CALL(observer, Observe(chrome::NOTIFICATION_APP_TERMINATING, _, _));
  policies.Set(key::kSessionLengthLimit,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(kOneHourInMs),
               NULL);
  UpdateProviderPolicy(policies);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);
}

// Disabled, see http://crbug.com/315308.
IN_PROC_BROWSER_TEST_F(PolicyTest,
                       DISABLED_PRE_WaitForInitialUserActivityUsatisfied) {
  // Indicate that the session started 2 hours ago and no user activity has
  // occurred yet.
  g_browser_process->local_state()->SetInt64(
      prefs::kSessionStartTime,
      (base::TimeTicks::Now() - base::TimeDelta::FromHours(2))
          .ToInternalValue());
}

// Disabled, see http://crbug.com/315308.
IN_PROC_BROWSER_TEST_F(PolicyTest,
                       DISABLED_WaitForInitialUserActivityUsatisfied) {
  content::MockNotificationObserver observer;
  content::NotificationRegistrar registrar;
  registrar.Add(&observer,
                chrome::NOTIFICATION_APP_TERMINATING,
                content::NotificationService::AllSources());

  // Require initial user activity.
  PolicyMap policies;
  policies.Set(key::kWaitForInitialUserActivity, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(true),
               NULL);
  UpdateProviderPolicy(policies);
  base::RunLoop().RunUntilIdle();

  // Set the session length limit to 1 hour. Verify that the session is not
  // terminated.
  EXPECT_CALL(observer, Observe(chrome::NOTIFICATION_APP_TERMINATING, _, _))
      .Times(0);
  policies.Set(key::kSessionLengthLimit,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(kOneHourInMs),
               NULL);
  UpdateProviderPolicy(policies);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);
}

// Disabled, see http://crbug.com/315308.
IN_PROC_BROWSER_TEST_F(PolicyTest,
                       DISABLED_PRE_WaitForInitialUserActivitySatisfied) {
  // Indicate that initial user activity in this session occurred 2 hours ago.
  g_browser_process->local_state()->SetInt64(
      prefs::kSessionStartTime,
      (base::TimeTicks::Now() - base::TimeDelta::FromHours(2))
          .ToInternalValue());
  g_browser_process->local_state()->SetBoolean(
      prefs::kSessionUserActivitySeen,
      true);
}

// Disabled, see http://crbug.com/315308.
IN_PROC_BROWSER_TEST_F(PolicyTest,
                       DISABLED_WaitForInitialUserActivitySatisfied) {
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
               POLICY_SCOPE_USER,
               new base::FundamentalValue(true),
               NULL);
  policies.Set(key::kSessionLengthLimit,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(kThreeHoursInMs),
               NULL);
  UpdateProviderPolicy(policies);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  // Decrease the session length limit to 1 hour. Verify that the session is
  // terminated immediately.
  EXPECT_CALL(observer, Observe(chrome::NOTIFICATION_APP_TERMINATING, _, _));
  policies.Set(key::kSessionLengthLimit,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(kOneHourInMs),
               NULL);
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
  policies.Set(key::kLargeCursorEnabled,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(false),
               NULL);
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
  accessibility_manager->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(accessibility_manager->IsSpokenFeedbackEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kSpokenFeedbackEnabled,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(false),
               NULL);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(accessibility_manager->IsSpokenFeedbackEnabled());

  // Verify that spoken feedback cannot be enabled manually anymore.
  accessibility_manager->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
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
  policies.Set(key::kHighContrastEnabled,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(false),
               NULL);
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
  policies.Set(key::kScreenMagnifierType,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(0),
               NULL);
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
  policies.Set(key::kScreenMagnifierType,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(ash::MAGNIFIER_FULL),
               NULL);
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
  policies.Set(key::kVirtualKeyboardEnabled,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(false),
               NULL);
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
  policies.Set(key::kTouchVirtualKeyboardEnabled,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(true),
               NULL);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(keyboard::IsKeyboardEnabled());
  keyboard::SetTouchKeyboardEnabled(false);
  EXPECT_TRUE(keyboard::IsKeyboardEnabled());

  // Verify that disabling the policy takes effect immediately and that the user
  // cannot enable the keyboard.
  policies.Set(key::kTouchVirtualKeyboardEnabled,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(false),
               NULL);
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

bool IsNonSwitchArgument(const CommandLine::StringType& s) {
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
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // TODO(nkostylev): Investigate if we can remove this switch.
    command_line->AppendSwitch(switches::kCreateBrowserOnStartupForTests);
    PolicyTest::SetUpCommandLine(command_line);
  }
#endif

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    // Set early policies now, before the browser is created.
    (this->*(GetParam()))();

    // Remove the non-switch arguments, so that session restore kicks in for
    // these tests.
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    CommandLine::StringVector argv = command_line->argv();
    argv.erase(std::remove_if(++argv.begin(), argv.end(), IsNonSwitchArgument),
               argv.end());
    command_line->InitFromArgv(argv);
    ASSERT_TRUE(std::equal(argv.begin(), argv.end(),
                           command_line->argv().begin()));

    // Redirect the test URLs to the test data directory.
    RedirectHostsToTestData(kRestoredURLs, arraysize(kRestoredURLs));
  }

  void HomepageIsNotNTP() {
    // Verifies that policy can set the startup pages to the homepage, when
    // the homepage is not the NTP.
    PolicyMap policies;
    policies.Set(
        key::kRestoreOnStartup,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_USER,
        new base::FundamentalValue(SessionStartupPref::kPrefValueHomePage),
        NULL);
    policies.Set(key::kHomepageIsNewTabPage,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 new base::FundamentalValue(false),
                 NULL);
    policies.Set(key::kHomepageLocation,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 new base::StringValue(kRestoredURLs[1]),
                 NULL);
    provider_.UpdateChromePolicy(policies);

    expected_urls_.push_back(GURL(kRestoredURLs[1]));
  }

  void HomepageIsNTP() {
    // Verifies that policy can set the startup pages to the homepage, when
    // the homepage is the NTP.
    PolicyMap policies;
    policies.Set(
        key::kRestoreOnStartup,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_USER,
        new base::FundamentalValue(SessionStartupPref::kPrefValueHomePage),
        NULL);
    policies.Set(key::kHomepageIsNewTabPage,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 new base::FundamentalValue(true),
                 NULL);
    provider_.UpdateChromePolicy(policies);

    expected_urls_.push_back(GURL(chrome::kChromeUINewTabURL));
  }

  void ListOfURLs() {
    // Verifies that policy can set the startup pages to a list of URLs.
    base::ListValue urls;
    for (size_t i = 0; i < arraysize(kRestoredURLs); ++i) {
      urls.Append(new base::StringValue(kRestoredURLs[i]));
      expected_urls_.push_back(GURL(kRestoredURLs[i]));
    }
    PolicyMap policies;
    policies.Set(key::kRestoreOnStartup,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 new base::FundamentalValue(SessionStartupPref::kPrefValueURLs),
                 NULL);
    policies.Set(
        key::kRestoreOnStartupURLs, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        urls.DeepCopy(), NULL);
    provider_.UpdateChromePolicy(policies);
  }

  void NTP() {
    // Verifies that policy can set the startup page to the NTP.
    PolicyMap policies;
    policies.Set(
        key::kRestoreOnStartup,
        POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_USER,
        new base::FundamentalValue(SessionStartupPref::kPrefValueNewTab),
        NULL);
    provider_.UpdateChromePolicy(policies);
    expected_urls_.push_back(GURL(chrome::kChromeUINewTabURL));
  }

  void Last() {
    // Verifies that policy can set the startup pages to the last session.
    PolicyMap policies;
    policies.Set(key::kRestoreOnStartup,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 new base::FundamentalValue(SessionStartupPref::kPrefValueLast),
                 NULL);
    provider_.UpdateChromePolicy(policies);
    // This should restore the tabs opened at PRE_RunTest below.
    for (size_t i = 0; i < arraysize(kRestoredURLs); ++i)
      expected_urls_.push_back(GURL(kRestoredURLs[i]));
  }

  std::vector<GURL> expected_urls_;
};

IN_PROC_BROWSER_TEST_P(RestoreOnStartupPolicyTest, PRE_RunTest) {
  // Open some tabs to verify if they are restored after the browser restarts.
  // Most policy settings override this, except kPrefValueLast which enforces
  // a restore.
  ui_test_utils::NavigateToURL(browser(), GURL(kRestoredURLs[0]));
  for (size_t i = 1; i < arraysize(kRestoredURLs); ++i) {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::AddSelectedTabWithURL(browser(), GURL(kRestoredURLs[i]),
                                  content::PAGE_TRANSITION_LINK);
    observer.Wait();
  }
}

IN_PROC_BROWSER_TEST_P(RestoreOnStartupPolicyTest, RunTest) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

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
    testing::Values(&RestoreOnStartupPolicyTest::HomepageIsNotNTP,
                    &RestoreOnStartupPolicyTest::HomepageIsNTP,
                    &RestoreOnStartupPolicyTest::ListOfURLs,
                    &RestoreOnStartupPolicyTest::NTP,
                    &RestoreOnStartupPolicyTest::Last));

// Similar to PolicyTest but sets a couple of policies before the browser is
// started.
class PolicyStatisticsCollectorTest : public PolicyTest {
 public:
  PolicyStatisticsCollectorTest() {}
  virtual ~PolicyStatisticsCollectorTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(key::kShowHomeButton,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 new base::FundamentalValue(true),
                 NULL);
    policies.Set(key::kBookmarkBarEnabled,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 new base::FundamentalValue(false),
                 NULL);
    policies.Set(key::kHomepageLocation,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 new base::StringValue("http://chromium.org"),
                 NULL);
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
  MediaStreamDevicesControllerBrowserTest()
      : request_url_allowed_via_whitelist_(false) {
    policy_value_ = GetParam();
  }
  virtual ~MediaStreamDevicesControllerBrowserTest() {}

  // Configure a given policy map.
  // The |policy_name| is the name of either the audio or video capture allow
  // policy and must never be NULL.
  // |whitelist_policy| and |allow_rule| are optional.  If NULL, no whitelist
  // policy is set.  If non-NULL, the request_url_ will be set to be non empty
  // and the whitelist policy is set to contain either the |allow_rule| (if
  // non-NULL) or an "allow all" wildcard.
  void ConfigurePolicyMap(PolicyMap* policies, const char* policy_name,
                          const char* whitelist_policy,
                          const char* allow_rule) {
    policies->Set(policy_name,
                  POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER,
                  new base::FundamentalValue(policy_value_),
                  NULL);

    if (whitelist_policy) {
      // TODO(tommi): Remove the kiosk mode flag when the whitelist is visible
      // in the media exceptions UI.
      // See discussion here: https://codereview.chromium.org/15738004/
      CommandLine::ForCurrentProcess()->AppendSwitch(switches::kKioskMode);

      // Add an entry to the whitelist that allows the specified URL regardless
      // of the setting of kAudioCapturedAllowed.
      request_url_ = GURL("http://www.example.com/foo");
      base::ListValue* list = new base::ListValue();
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
      policies->Set(whitelist_policy, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_USER, list, NULL);
    }
  }

  void Accept(const content::MediaStreamDevices& devices,
              content::MediaStreamRequestResult result,
              scoped_ptr<content::MediaStreamUI> ui) {
    if (policy_value_ || request_url_allowed_via_whitelist_) {
      ASSERT_EQ(1U, devices.size());
      ASSERT_EQ("fake_dev", devices[0].id);
    } else {
      ASSERT_EQ(0U, devices.size());
    }
  }

  void FinishAudioTest() {
    content::MediaStreamRequest request(0, 0, 0,
                                        request_url_.GetOrigin(), false,
                                        content::MEDIA_DEVICE_ACCESS,
                                        std::string(), std::string(),
                                        content::MEDIA_DEVICE_AUDIO_CAPTURE,
                                        content::MEDIA_NO_SERVICE);
    // TODO(raymes): Test MEDIA_DEVICE_OPEN (Pepper) which grants both webcam
    // and microphone permissions at the same time.
    MediaStreamDevicesController controller(
        browser()->tab_strip_model()->GetActiveWebContents(), request,
        base::Bind(&MediaStreamDevicesControllerBrowserTest::Accept, this));
    controller.Accept(false);

    base::MessageLoop::current()->QuitWhenIdle();
  }

  void FinishVideoTest() {
    // TODO(raymes): Test MEDIA_DEVICE_OPEN (Pepper) which grants both webcam
    // and microphone permissions at the same time.
    content::MediaStreamRequest request(0, 0, 0,
                                        request_url_.GetOrigin(), false,
                                        content::MEDIA_DEVICE_ACCESS,
                                        std::string(),
                                        std::string(),
                                        content::MEDIA_NO_SERVICE,
                                        content::MEDIA_DEVICE_VIDEO_CAPTURE);
    MediaStreamDevicesController controller(
        browser()->tab_strip_model()->GetActiveWebContents(), request,
        base::Bind(&MediaStreamDevicesControllerBrowserTest::Accept, this));
    controller.Accept(false);

    base::MessageLoop::current()->QuitWhenIdle();
  }

  bool policy_value_;
  bool request_url_allowed_via_whitelist_;
  GURL request_url_;
  static const char kExampleRequestPattern[];
};

// static
const char MediaStreamDevicesControllerBrowserTest::kExampleRequestPattern[] =
    "http://[*.]example.com/";

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
      base::Bind(&MediaCaptureDevicesDispatcher::SetTestAudioCaptureDevices,
                 base::Unretained(MediaCaptureDevicesDispatcher::GetInstance()),
                 audio_devices),
      base::Bind(&MediaStreamDevicesControllerBrowserTest::FinishAudioTest,
                 this));

  base::MessageLoop::current()->Run();
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
        base::Bind(
            &MediaCaptureDevicesDispatcher::SetTestAudioCaptureDevices,
            base::Unretained(MediaCaptureDevicesDispatcher::GetInstance()),
            audio_devices),
        base::Bind(
            &MediaStreamDevicesControllerBrowserTest::FinishAudioTest,
            this));

    base::MessageLoop::current()->Run();
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
      base::Bind(&MediaCaptureDevicesDispatcher::SetTestVideoCaptureDevices,
                 base::Unretained(MediaCaptureDevicesDispatcher::GetInstance()),
                 video_devices),
      base::Bind(&MediaStreamDevicesControllerBrowserTest::FinishVideoTest,
                 this));

  base::MessageLoop::current()->Run();
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
        base::Bind(&MediaCaptureDevicesDispatcher::SetTestVideoCaptureDevices,
            base::Unretained(MediaCaptureDevicesDispatcher::GetInstance()),
            video_devices),
        base::Bind(
            &MediaStreamDevicesControllerBrowserTest::FinishVideoTest,
            this));

    base::MessageLoop::current()->Run();
  }
}

INSTANTIATE_TEST_CASE_P(MediaStreamDevicesControllerBrowserTestInstance,
                        MediaStreamDevicesControllerBrowserTest,
                        testing::Bool());

#if !defined(OS_CHROMEOS)
// Similar to PolicyTest but sets the proper policy before the browser is
// started.
class PolicyVariationsServiceTest : public PolicyTest {
 public:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(key::kVariationsRestrictParameter,
                 POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER,
                 new base::StringValue("restricted"),
                 NULL);
    provider_.UpdateChromePolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(PolicyVariationsServiceTest, VariationsURLIsValid) {
  const std::string default_variations_url =
      chrome_variations::VariationsService::
          GetDefaultVariationsServerURLForTesting();

  const GURL url =
      chrome_variations::VariationsService::GetVariationsServerURL(
          g_browser_process->local_state());
  EXPECT_TRUE(StartsWithASCII(url.spec(), default_variations_url, true));
  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "restrict", &value));
  EXPECT_EQ("restricted", value);
}

IN_PROC_BROWSER_TEST_F(PolicyTest, NativeMessagingBlacklistSelective) {
  base::ListValue blacklist;
  blacklist.Append(new base::StringValue("host.name"));
  PolicyMap policies;
  policies.Set(key::kNativeMessagingBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, blacklist.DeepCopy(), NULL);
  UpdateProviderPolicy(policies);

  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_FALSE(extensions::NativeMessageProcessHost::IsHostAllowed(
      prefs, "host.name"));
  EXPECT_TRUE(extensions::NativeMessageProcessHost::IsHostAllowed(
      prefs, "other.host.name"));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, NativeMessagingBlacklistWildcard) {
  base::ListValue blacklist;
  blacklist.Append(new base::StringValue("*"));
  PolicyMap policies;
  policies.Set(key::kNativeMessagingBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, blacklist.DeepCopy(), NULL);
  UpdateProviderPolicy(policies);

  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_FALSE(extensions::NativeMessageProcessHost::IsHostAllowed(
      prefs, "host.name"));
  EXPECT_FALSE(extensions::NativeMessageProcessHost::IsHostAllowed(
      prefs, "other.host.name"));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, NativeMessagingWhitelist) {
  base::ListValue blacklist;
  blacklist.Append(new base::StringValue("*"));
  base::ListValue whitelist;
  whitelist.Append(new base::StringValue("host.name"));
  PolicyMap policies;
  policies.Set(key::kNativeMessagingBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, blacklist.DeepCopy(), NULL);
  policies.Set(key::kNativeMessagingWhitelist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, whitelist.DeepCopy(), NULL);
  UpdateProviderPolicy(policies);

  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(extensions::NativeMessageProcessHost::IsHostAllowed(
      prefs, "host.name"));
  EXPECT_FALSE(extensions::NativeMessageProcessHost::IsHostAllowed(
      prefs, "other.host.name"));
}

IN_PROC_BROWSER_TEST_F(PolicyTest,
                       EnableDeprecatedWebPlatformFeatures_ShowModalDialog) {
  base::ListValue enabled_features;
  enabled_features.Append(new base::StringValue(
      "ShowModalDialog_EffectiveUntil20150430"));
  PolicyMap policies;
  policies.Set(key::kEnableDeprecatedWebPlatformFeatures,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               enabled_features.DeepCopy(),
               NULL);
  UpdateProviderPolicy(policies);

  // Policy only takes effect on new browsers, not existing browsers, so create
  // a new browser.
  Browser* browser2 = CreateBrowser(browser()->profile());
  ui_test_utils::NavigateToURL(browser2, GURL(url::kAboutBlankURL));
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser2->tab_strip_model()->GetActiveWebContents(),
      "domAutomationController.send(window.showModalDialog !== undefined);",
      &result));
  EXPECT_TRUE(result);
}

#endif  // !defined(CHROME_OS)

}  // namespace policy
