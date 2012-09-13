// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/test/test_file_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/process_type.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_utils.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "net/http/http_stream_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/plugins/webplugininfo.h"

#if defined(OS_CHROMEOS)
#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#endif

using content::BrowserThread;
using testing::Return;

namespace policy {

namespace {

const char kURL[] = "http://example.com";
const char kCookieValue[] = "converted=true";
// Assigned to Philip J. Fry to fix eventually.
const char kCookieOptions[] = ";expires=Wed Jan 01 3000 00:00:00 GMT";

const FilePath::CharType kTestExtensionsDir[] = FILE_PATH_LITERAL("extensions");
const FilePath::CharType kGoodCrxName[] = FILE_PATH_LITERAL("good.crx");
const FilePath::CharType kAdBlockCrxName[] = FILE_PATH_LITERAL("adblock.crx");

const char kGoodCrxId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char kAdBlockCrxId[] = "dojnnbeimaimaojcialkkgajdnefpgcn";

const FilePath::CharType kGoodCrxManifestName[] =
    FILE_PATH_LITERAL("good_update_manifest.xml");

const char* kURLs[] = {
  chrome::kChromeUINewTabURL,
  chrome::kChromeUIAboutURL,
  chrome::kChromeUICreditsURL,
  chrome::kChromeUIPolicyURL,
  chrome::kChromeUIVersionURL,
};

// Filters requests to the hosts in |urls| and redirects them to the test data
// dir through URLRequestMockHTTPJobs.
void RedirectHostsToTestDataOnIOThread(const GURL* const urls[], size_t size) {
  // Map the given hosts to the test data dir.
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  for (size_t i = 0; i < size; ++i) {
    const GURL* url = urls[i];
    EXPECT_TRUE(url->is_valid());
    filter->AddHostnameHandler(url->scheme(), url->host(),
                               URLRequestMockHTTPJob::Factory);
  }
}

// Verifies that the given |url| can be opened. This assumes that |url| points
// at empty.html in the test data dir.
void CheckCanOpenURL(Browser* browser, const GURL& url) {
  ui_test_utils::NavigateToURL(browser, url);
  content::WebContents* contents = chrome::GetActiveWebContents(browser);
  EXPECT_EQ(url, contents->GetURL());
  EXPECT_EQ(net::FormatUrl(url, std::string()), contents->GetTitle());
}

// Verifies that access to the given |url| is blocked.
void CheckURLIsBlocked(Browser* browser, const GURL& url) {
  ui_test_utils::NavigateToURL(browser, url);
  content::WebContents* contents = chrome::GetActiveWebContents(browser);
  EXPECT_EQ(url, contents->GetURL());
  string16 title = UTF8ToUTF16(url.spec() + " is not available");
  EXPECT_EQ(title, contents->GetTitle());

  // Verify that the expected error page is being displayed.
  // (error 138 == NETWORK_ACCESS_DENIED)
  bool result = false;
  EXPECT_TRUE(content::ExecuteJavaScriptAndExtractBool(
      contents->GetRenderViewHost(),
      std::wstring(),
      ASCIIToWide(
          "var hasError = false;"
          "var error = document.getElementById('errorDetails');"
          "if (error)"
          "  hasError = error.textContent.indexOf('Error 138') == 0;"
          "domAutomationController.send(hasError);"),
      &result));
  EXPECT_TRUE(result);
}

// Downloads a file named |file| and expects it to be saved to |dir|, which
// must be empty.
void DownloadAndVerifyFile(
    Browser* browser, const FilePath& dir, const FilePath& file) {
  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(browser->profile());
  content::DownloadTestObserverTerminal observer(
      download_manager, 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
  FilePath downloaded = dir.Append(file);
  EXPECT_FALSE(file_util::PathExists(downloaded));
  ui_test_utils::NavigateToURLWithDisposition(
      browser, url, CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  observer.WaitForFinished();
  EXPECT_EQ(
      1u, observer.NumDownloadsSeenInState(content::DownloadItem::COMPLETE));
  EXPECT_TRUE(file_util::PathExists(downloaded));
  file_util::FileEnumerator enumerator(
      dir, false, file_util::FileEnumerator::FILES);
  EXPECT_EQ(file, enumerator.Next().BaseName());
  EXPECT_EQ(FilePath(), enumerator.Next());
}

#if defined(OS_CHROMEOS)
int CountScreenshots() {
  DownloadPrefs* download_prefs = DownloadPrefs::FromBrowserContext(
      ash::Shell::GetInstance()->delegate()->GetCurrentBrowserContext());
  file_util::FileEnumerator enumerator(download_prefs->DownloadPath(),
                                       false, file_util::FileEnumerator::FILES,
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
  EXPECT_TRUE(content::ExecuteJavaScriptAndExtractBool(
      contents->GetRenderViewHost(),
      std::wstring(),
      L"var canvas = document.createElement('canvas');"
      L"var context = canvas.getContext('experimental-webgl');"
      L"domAutomationController.send(context != null);",
      &result));
  return result;
}

bool IsJavascriptEnabled(content::WebContents* contents) {
  content::RenderViewHost* rvh = contents->GetRenderViewHost();
  base::Value* value = rvh->ExecuteJavascriptAndGetValue(
      string16(),
      ASCIIToUTF16("123"));
  int result = 0;
  if (!value->GetAsInteger(&result))
    EXPECT_EQ(base::Value::TYPE_NULL, value->GetType());
  return result == 123;
}

void CopyPluginListAndQuit(std::vector<webkit::WebPluginInfo>* out,
                           const std::vector<webkit::WebPluginInfo>& in) {
  *out = in;
  MessageLoop::current()->QuitWhenIdle();
}

template<typename T>
void CopyValueAndQuit(T* out, T in) {
  *out = in;
  MessageLoop::current()->QuitWhenIdle();
}

void GetPluginList(std::vector<webkit::WebPluginInfo>* plugins) {
  content::PluginService* service = content::PluginService::GetInstance();
  service->GetPlugins(base::Bind(CopyPluginListAndQuit, plugins));
  content::RunMessageLoop();
}

const webkit::WebPluginInfo* GetFlashPlugin(
    const std::vector<webkit::WebPluginInfo>& plugins) {
  const webkit::WebPluginInfo* flash = NULL;
  for (size_t i = 0; i < plugins.size(); ++i) {
    if (plugins[i].name == ASCIIToUTF16("Shockwave Flash")) {
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
                      const webkit::WebPluginInfo* plugin,
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
    if (iter.GetData().type == content::PROCESS_TYPE_PLUGIN ||
        iter.GetData().type == content::PROCESS_TYPE_PPAPI_PLUGIN) {
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

}  // namespace

class PolicyTest : public InProcessBrowserTest {
 protected:
  PolicyTest() {}
  virtual ~PolicyTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    EXPECT_CALL(provider_, IsInitializationComplete())
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
    FilePath root_http;
    PathService::Get(content::DIR_TEST_DATA, &root_http);
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO, FROM_HERE,
        base::Bind(URLRequestMockHTTPJob::AddUrlHandler, root_http),
        MessageLoop::current()->QuitWhenIdleClosure());
    content::RunMessageLoop();
  }

  void SetScreenshotPolicy(bool enabled) {
    PolicyMap policies;
    policies.Set(key::kDisableScreenshots, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, base::Value::CreateBooleanValue(!enabled));
    provider_.UpdateChromePolicy(policies);
  }

  void TestScreenshotFeedback(bool enabled) {
    SetScreenshotPolicy(enabled);

    // Wait for feedback page to load.
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_FEEDBACK));
    observer.Wait();
    content::WebContents* web_contents =
        static_cast<content::Source<content::NavigationController> >(
            observer.source())->GetWebContents();

    // Wait for feedback page to fully initialize.
    // setupCurrentScreenshot is called when feedback page loads and (among
    // other things) adds current-screenshots-thumbnailDiv-0-image element.
    // The code below executes either before setupCurrentScreenshot was called
    // (setupCurrentScreenshot is replaced with our hook) or after it has
    // completed (in that case send result immediately).
    bool result = false;
    EXPECT_TRUE(content::ExecuteJavaScriptAndExtractBool(
        web_contents->GetRenderViewHost(),
        std::wstring(),
        L"function btest_initCompleted(url) {"
        L"  var img = new Image();"
        L"  img.src = url;"
        L"  img.onload = function() {"
        L"    domAutomationController.send(img.width * img.height > 0);"
        L"  };"
        L"  img.onerror = function() {"
        L"    domAutomationController.send(false);"
        L"  };"
        L"}"
        L"function setupCurrentScreenshot(url) {"
        L"  btest_initCompleted(url);"
        L"}"
        L"var img = document.getElementById("
        L"    'current-screenshots-thumbnailDiv-0-image');"
        L"if (img)"
        L"  btest_initCompleted(img.src);",
        &result));
    EXPECT_EQ(enabled, result);

    // Feedback page is a singleton page, so close so future calls to this
    // function work as expected.
    web_contents->Close();
  }

#if defined(OS_CHROMEOS)
  void TestScreenshotFile(bool enabled) {
    SetScreenshotPolicy(enabled);
    ash::Shell::GetInstance()->accelerator_controller()->PerformAction(
        ash::TAKE_SCREENSHOT, ui::Accelerator());

    // TAKE_SCREENSHOT handler posts write file task on success, wait for it.
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(base::DoNothing),
        MessageLoop::QuitClosure());
    content::RunMessageLoop();
  }
#endif

  ExtensionService* extension_service() {
    extensions::ExtensionSystem* system =
        extensions::ExtensionSystem::Get(browser()->profile());
    return system->extension_service();
  }

  const extensions::Extension* InstallExtension(
      const FilePath::StringType& name) {
    FilePath extension_path(ui_test_utils::GetTestFilePath(
        FilePath(kTestExtensionsDir), FilePath(name)));
    scoped_refptr<extensions::CrxInstaller> installer =
        extensions::CrxInstaller::Create(extension_service(), NULL);
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

  void UninstallExtension(const std::string& id, bool expect_success) {
    content::WindowedNotificationObserver observer(
        expect_success ? chrome::NOTIFICATION_EXTENSION_UNINSTALLED
                       : chrome::NOTIFICATION_EXTENSION_UNINSTALL_NOT_ALLOWED,
        content::NotificationService::AllSources());
    extension_service()->UninstallExtension(id, false, NULL);
    observer.Wait();
  }

  MockConfigurationPolicyProvider provider_;
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
    policies.Set(
        key::kApplicationLocaleValue, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        base::Value::CreateStringValue("fr"));
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
  string16 french_title = l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);
  string16 title;
  EXPECT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &title));
  EXPECT_EQ(french_title, title);

  // Make sure this is really French and differs from the English title.
  std::string loaded =
      ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources("en-US");
  EXPECT_EQ("en-US", loaded);
  string16 english_title = l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);
  EXPECT_NE(french_title, english_title);
}
#endif

IN_PROC_BROWSER_TEST_F(PolicyTest, BookmarkBarEnabled) {
  // Verifies that the bookmarks bar can be forced to always or never show up.

  // Test starts in about:blank.
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kShowBookmarkBar));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kShowBookmarkBar));
  EXPECT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());

  PolicyMap policies;
  policies.Set(key::kBookmarkBarEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  provider_.UpdateChromePolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kShowBookmarkBar));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kShowBookmarkBar));
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());

  // The NTP has special handling of the bookmark bar.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());

  policies.Set(key::kBookmarkBarEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false));
  provider_.UpdateChromePolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kShowBookmarkBar));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kShowBookmarkBar));
  // The bookmark bar is hidden in the NTP when disabled by policy.
  EXPECT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());

  policies.Clear();
  provider_.UpdateChromePolicy(policies);
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kShowBookmarkBar));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kShowBookmarkBar));
  // The bookmark bar is shown detached in the NTP, when disabled by prefs only.
  EXPECT_EQ(BookmarkBar::DETACHED, browser()->bookmark_bar_state());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, PRE_PRE_ClearSiteDataOnExit) {
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

IN_PROC_BROWSER_TEST_F(PolicyTest, PRE_ClearSiteDataOnExit) {
  // Verify that the cookie persists across restarts.
  EXPECT_EQ(kCookieValue, GetCookies(browser()->profile(), GURL(kURL)));
  // Now set the policy and the cookie should be gone after another restart.
  PolicyMap policies;
  policies.Set(key::kClearSiteDataOnExit, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  provider_.UpdateChromePolicy(policies);
}

IN_PROC_BROWSER_TEST_F(PolicyTest, ClearSiteDataOnExit) {
  // Verify that the cookie is gone.
  EXPECT_TRUE(GetCookies(browser()->profile(), GURL(kURL)).empty());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DefaultSearchProvider) {
  // Verifies that a default search is made using the provider configured via
  // policy. Also checks that default search can be completely disabled.
  const string16 kKeyword(ASCIIToUTF16("testsearch"));
  const std::string kSearchURL("http://search.example/search?q={searchTerms}");

  TemplateURLService* service = TemplateURLServiceFactory::GetForProfile(
      browser()->profile());
  ui_test_utils::WaitForTemplateURLServiceToLoad(service);
  TemplateURL* default_search = service->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);
  EXPECT_NE(kKeyword, default_search->keyword());
  EXPECT_NE(kSearchURL, default_search->url());

  // Override the default search provider using policies.
  PolicyMap policies;
  policies.Set(key::kDefaultSearchProviderEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  policies.Set(key::kDefaultSearchProviderKeyword, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateStringValue(kKeyword));
  policies.Set(key::kDefaultSearchProviderSearchURL, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateStringValue(kSearchURL));
  provider_.UpdateChromePolicy(policies);
  default_search = service->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);
  EXPECT_EQ(kKeyword, default_search->keyword());
  EXPECT_EQ(kSearchURL, default_search->url());

  // Verify that searching from the omnibox uses kSearchURL.
  chrome::FocusLocationBar(browser());
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  ui_test_utils::SendToOmniboxAndSubmit(location_bar, "stuff to search for");
  OmniboxEditModel* model = location_bar->GetLocationEntry()->model();
  EXPECT_TRUE(model->CurrentMatch().destination_url.is_valid());
  content::WebContents* web_contents = chrome::GetActiveWebContents(browser());
  GURL expected("http://search.example/search?q=stuff+to+search+for");
  EXPECT_EQ(expected, web_contents->GetURL());

  // Verify that searching from the omnibox can be disabled.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kAboutBlankURL));
  policies.Set(key::kDefaultSearchProviderEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false));
  EXPECT_TRUE(service->GetDefaultSearchProvider());
  provider_.UpdateChromePolicy(policies);
  EXPECT_FALSE(service->GetDefaultSearchProvider());
  ui_test_utils::SendToOmniboxAndSubmit(location_bar, "should not work");
  // This means that submitting won't trigger any action.
  EXPECT_FALSE(model->CurrentMatch().destination_url.is_valid());
  EXPECT_EQ(GURL(chrome::kAboutBlankURL), web_contents->GetURL());
}

// The linux and win  bots can't create a GL context. http://crbug.com/103379
#if defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(PolicyTest, Disable3DAPIs) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kAboutBlankURL));
  // WebGL is enabled by default.
  content::WebContents* contents = chrome::GetActiveWebContents(browser());
  EXPECT_TRUE(IsWebGLEnabled(contents));
  // Disable with a policy.
  PolicyMap policies;
  policies.Set(key::kDisable3DAPIs, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  provider_.UpdateChromePolicy(policies);
  // Crash and reload the tab to get a new renderer.
  content::CrashTab(contents);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_RELOAD));
  EXPECT_FALSE(IsWebGLEnabled(contents));
  // Enable with a policy.
  policies.Set(key::kDisable3DAPIs, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false));
  provider_.UpdateChromePolicy(policies);
  content::CrashTab(contents);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_RELOAD));
  EXPECT_TRUE(IsWebGLEnabled(contents));
}
#endif

IN_PROC_BROWSER_TEST_F(PolicyTest, DisableSpdy) {
  // Verifies that SPDY can be disable by policy.
  EXPECT_TRUE(net::HttpStreamFactory::spdy_enabled());
  PolicyMap policies;
  policies.Set(key::kDisableSpdy, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  provider_.UpdateChromePolicy(policies);
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(net::HttpStreamFactory::spdy_enabled());
  // Verify that it can be force-enabled too.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kDisableSpdy, true);
  policies.Set(key::kDisableSpdy, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false));
  provider_.UpdateChromePolicy(policies);
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(net::HttpStreamFactory::spdy_enabled());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DisabledPlugins) {
  // Verifies that plugins can be forced to be disabled by policy.

  // Verify that the Flash plugin exists and that it can be enabled and disabled
  // by the user.
  std::vector<webkit::WebPluginInfo> plugins;
  GetPluginList(&plugins);
  const webkit::WebPluginInfo* flash = GetFlashPlugin(plugins);
  if (!flash)
    return;
  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(browser()->profile());
  EXPECT_TRUE(plugin_prefs->IsPluginEnabled(*flash));
  EXPECT_TRUE(SetPluginEnabled(plugin_prefs, flash, false));
  EXPECT_FALSE(plugin_prefs->IsPluginEnabled(*flash));
  EXPECT_TRUE(SetPluginEnabled(plugin_prefs, flash, true));
  EXPECT_TRUE(plugin_prefs->IsPluginEnabled(*flash));

  // Now disable it with a policy.
  base::ListValue disabled_plugins;
  disabled_plugins.Append(base::Value::CreateStringValue("*Flash*"));
  PolicyMap policies;
  policies.Set(key::kDisabledPlugins, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, disabled_plugins.DeepCopy());
  provider_.UpdateChromePolicy(policies);
  EXPECT_FALSE(plugin_prefs->IsPluginEnabled(*flash));
  // The user shouldn't be able to enable it.
  EXPECT_FALSE(SetPluginEnabled(plugin_prefs, flash, true));
  EXPECT_FALSE(plugin_prefs->IsPluginEnabled(*flash));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DisabledPluginsExceptions) {
  // Verifies that plugins with an exception in the blacklist can be enabled.

  // Verify that the Flash plugin exists and that it can be enabled and disabled
  // by the user.
  std::vector<webkit::WebPluginInfo> plugins;
  GetPluginList(&plugins);
  const webkit::WebPluginInfo* flash = GetFlashPlugin(plugins);
  if (!flash)
    return;
  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(browser()->profile());
  EXPECT_TRUE(plugin_prefs->IsPluginEnabled(*flash));

  // Disable all plugins.
  base::ListValue disabled_plugins;
  disabled_plugins.Append(base::Value::CreateStringValue("*"));
  PolicyMap policies;
  policies.Set(key::kDisabledPlugins, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, disabled_plugins.DeepCopy());
  provider_.UpdateChromePolicy(policies);
  EXPECT_FALSE(plugin_prefs->IsPluginEnabled(*flash));
  // The user shouldn't be able to enable it.
  EXPECT_FALSE(SetPluginEnabled(plugin_prefs, flash, true));
  EXPECT_FALSE(plugin_prefs->IsPluginEnabled(*flash));

  // Now open an exception for flash.
  base::ListValue disabled_plugins_exceptions;
  disabled_plugins_exceptions.Append(
      base::Value::CreateStringValue("*Flash*"));
  policies.Set(key::kDisabledPluginsExceptions, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, disabled_plugins_exceptions.DeepCopy());
  provider_.UpdateChromePolicy(policies);
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
  std::vector<webkit::WebPluginInfo> plugins;
  GetPluginList(&plugins);
  const webkit::WebPluginInfo* flash = GetFlashPlugin(plugins);
  if (!flash)
    return;
  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(browser()->profile());
  EXPECT_TRUE(plugin_prefs->IsPluginEnabled(*flash));

  // The user disables it and then a policy forces it to be enabled.
  EXPECT_TRUE(SetPluginEnabled(plugin_prefs, flash, false));
  EXPECT_FALSE(plugin_prefs->IsPluginEnabled(*flash));
  base::ListValue plugin_list;
  plugin_list.Append(base::Value::CreateStringValue("Shockwave Flash"));
  PolicyMap policies;
  policies.Set(key::kEnabledPlugins, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, plugin_list.DeepCopy());
  provider_.UpdateChromePolicy(policies);
  EXPECT_TRUE(plugin_prefs->IsPluginEnabled(*flash));
  // The user can't disable it anymore.
  EXPECT_FALSE(SetPluginEnabled(plugin_prefs, flash, false));
  EXPECT_TRUE(plugin_prefs->IsPluginEnabled(*flash));

  // When a plugin is both enabled and disabled, the whitelist takes precedence.
  policies.Set(key::kDisabledPlugins, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, plugin_list.DeepCopy());
  provider_.UpdateChromePolicy(policies);
  EXPECT_TRUE(plugin_prefs->IsPluginEnabled(*flash));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, AlwaysAuthorizePlugins) {
  // Verifies that dangerous plugins can be always authorized to run with
  // a policy.

  // Verify that the test page exists. It is only present in checkouts with
  // src-internal.
  if (!file_util::PathExists(ui_test_utils::GetTestFilePath(
      FilePath(FILE_PATH_LITERAL("plugin")),
      FilePath(FILE_PATH_LITERAL("quicktime.html"))))) {
    LOG(INFO) <<
        "Test skipped because plugin/quicktime.html test file wasn't found.";
    return;
  }

  ServeContentTestData();
  // No plugins at startup.
  EXPECT_EQ(0, CountPlugins());

  content::WebContents* contents = chrome::GetActiveWebContents(browser());
  ASSERT_TRUE(contents);
  TabContents* tab_contents = TabContents::FromWebContents(contents);
  ASSERT_TRUE(tab_contents);
  InfoBarTabHelper* infobar_helper = tab_contents->infobar_tab_helper();
  ASSERT_TRUE(infobar_helper);
  EXPECT_EQ(0u, infobar_helper->GetInfoBarCount());

  FilePath path(FILE_PATH_LITERAL("plugin/quicktime.html"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(path));
  ui_test_utils::NavigateToURL(browser(), url);
  // This should have triggered the dangerous plugin infobar.
  ASSERT_EQ(1u, infobar_helper->GetInfoBarCount());
  InfoBarDelegate* infobar_delegate = infobar_helper->GetInfoBarDelegateAt(0);
  EXPECT_TRUE(infobar_delegate->AsConfirmInfoBarDelegate());
  // And the plugin isn't running.
  EXPECT_EQ(0, CountPlugins());

  // Now set a policy to always authorize this.
  PolicyMap policies;
  policies.Set(key::kAlwaysAuthorizePlugins, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  provider_.UpdateChromePolicy(policies);
  // Reloading the page shouldn't trigger the infobar this time.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(0u, infobar_helper->GetInfoBarCount());
  // And the plugin started automatically.
  EXPECT_EQ(1, CountPlugins());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DeveloperToolsDisabled) {
  // Verifies that access to the developer tools can be disabled.

  // Open devtools.
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_DEV_TOOLS));
  content::WebContents* contents = chrome::GetActiveWebContents(browser());
  EXPECT_TRUE(DevToolsWindow::GetDevToolsContents(contents));

  // Disable devtools via policy.
  PolicyMap policies;
  policies.Set(key::kDeveloperToolsDisabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  provider_.UpdateChromePolicy(policies);
  // The existing devtools window should have closed.
  EXPECT_FALSE(DevToolsWindow::GetDevToolsContents(contents));
  // And it's not possible to open it again.
  EXPECT_FALSE(chrome::ExecuteCommand(browser(), IDC_DEV_TOOLS));
  EXPECT_FALSE(DevToolsWindow::GetDevToolsContents(contents));
}

// This policy isn't available on Chrome OS.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PolicyTest, DownloadDirectory) {
  // Verifies that the download directory can be forced by policy.

  // Set the initial download directory.
  ScopedTempDir initial_dir;
  ASSERT_TRUE(initial_dir.CreateUniqueTempDir());
  browser()->profile()->GetPrefs()->SetFilePath(
      prefs::kDownloadDefaultDirectory, initial_dir.path());
  // Don't prompt for the download location during this test.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPromptForDownload, false);

  // Verify that downloads end up on the default directory.
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  DownloadAndVerifyFile(browser(), initial_dir.path(), file);
  file_util::DieFileDie(initial_dir.path().Append(file), false);

  // Override the download directory with the policy and verify a download.
  ScopedTempDir forced_dir;
  ASSERT_TRUE(forced_dir.CreateUniqueTempDir());
  PolicyMap policies;
  policies.Set(key::kDownloadDirectory, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               base::Value::CreateStringValue(forced_dir.path().value()));
  provider_.UpdateChromePolicy(policies);
  DownloadAndVerifyFile(browser(), forced_dir.path(), file);
  // Verify that the first download location wasn't affected.
  EXPECT_FALSE(file_util::PathExists(initial_dir.path().Append(file)));
}
#endif

IN_PROC_BROWSER_TEST_F(PolicyTest, ExtensionInstallBlacklist) {
  // Verifies that blacklisted extensions can't be installed.
  ExtensionService* service = extension_service();
  ASSERT_FALSE(service->GetExtensionById(kGoodCrxId, true));
  ASSERT_FALSE(service->GetExtensionById(kAdBlockCrxId, true));
  base::ListValue blacklist;
  blacklist.Append(base::Value::CreateStringValue(kGoodCrxId));
  PolicyMap policies;
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, blacklist.DeepCopy());
  provider_.UpdateChromePolicy(policies);

  // "good.crx" is blacklisted.
  EXPECT_FALSE(InstallExtension(kGoodCrxName));
  EXPECT_FALSE(service->GetExtensionById(kGoodCrxId, true));

  // "adblock.crx" is not.
  const extensions::Extension* adblock = InstallExtension(kAdBlockCrxName);
  ASSERT_TRUE(adblock);
  EXPECT_EQ(kAdBlockCrxId, adblock->id());
  EXPECT_EQ(adblock,
            service->GetExtensionById(kAdBlockCrxId, true));

  // Now blacklist all extensions.
  blacklist.Clear();
  blacklist.Append(base::Value::CreateStringValue("*"));
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, blacklist.DeepCopy());
  provider_.UpdateChromePolicy(policies);
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
  blacklist.Append(base::Value::CreateStringValue("*"));
  base::ListValue whitelist;
  whitelist.Append(base::Value::CreateStringValue(kGoodCrxId));
  PolicyMap policies;
  policies.Set(key::kExtensionInstallBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, blacklist.DeepCopy());
  policies.Set(key::kExtensionInstallWhitelist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, whitelist.DeepCopy());
  provider_.UpdateChromePolicy(policies);
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
  // that includes "good.crx".
  FilePath path = FilePath(kTestExtensionsDir).Append(kGoodCrxManifestName);
  GURL url(URLRequestMockHTTPJob::GetMockUrl(path));

  // Setting the forcelist extension should install "good.crx".
  base::ListValue forcelist;
  forcelist.Append(base::Value::CreateStringValue(StringPrintf(
      "%s;%s", kGoodCrxId, url.spec().c_str())));
  PolicyMap policies;
  policies.Set(key::kExtensionInstallForcelist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, forcelist.DeepCopy());
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_EXTENSION_INSTALLED,
      content::NotificationService::AllSources());
  provider_.UpdateChromePolicy(policies);
  observer.Wait();
  content::Details<const extensions::Extension> details = observer.details();
  EXPECT_EQ(kGoodCrxId, details->id());
  EXPECT_EQ(details.ptr(), service->GetExtensionById(kGoodCrxId, true));
  // The user is not allowed to uninstall force-installed extensions.
  UninstallExtension(kGoodCrxId, false);
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
  content::WebContents* contents = chrome::GetActiveWebContents(browser());
  EXPECT_EQ(GURL(chrome::kAboutBlankURL), contents->GetURL());
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_HOME));
  EXPECT_EQ(GURL(chrome::kChromeUIPolicyURL), contents->GetURL());

  // Now override with policy.
  PolicyMap policies;
  policies.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               base::Value::CreateStringValue(chrome::kChromeUIBookmarksURL));
  provider_.UpdateChromePolicy(policies);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_HOME));
  content::WaitForLoadStop(contents);
  EXPECT_EQ(GURL(chrome::kChromeUIBookmarksURL), contents->GetURL());

  policies.Set(key::kHomepageIsNewTabPage, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  provider_.UpdateChromePolicy(policies);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_HOME));
  content::WaitForLoadStop(contents);
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), contents->GetURL());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, IncognitoEnabled) {
  // Verifies that incognito windows can't be opened when disabled by policy.

  // Disable incognito via policy and verify that incognito windows can't be
  // opened.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_FALSE(BrowserList::IsOffTheRecordSessionActive());
  PolicyMap policies;
  policies.Set(key::kIncognitoEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false));
  provider_.UpdateChromePolicy(policies);
  EXPECT_FALSE(chrome::ExecuteCommand(browser(), IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_FALSE(BrowserList::IsOffTheRecordSessionActive());

  // Enable via policy and verify that incognito windows can be opened.
  policies.Set(key::kIncognitoEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  provider_.UpdateChromePolicy(policies);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_TRUE(BrowserList::IsOffTheRecordSessionActive());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, Javascript) {
  // Verifies that Javascript can be disabled.
  content::WebContents* contents = chrome::GetActiveWebContents(browser());
  EXPECT_TRUE(IsJavascriptEnabled(contents));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS_CONSOLE));

  // Disable Javascript via policy.
  PolicyMap policies;
  policies.Set(key::kJavascriptEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false));
  provider_.UpdateChromePolicy(policies);
  // Reload the page.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  EXPECT_FALSE(IsJavascriptEnabled(contents));
  // Developer tools still work when javascript is disabled.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS_CONSOLE));
  // Javascript is always enabled for the internal pages.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://settings"));
  EXPECT_TRUE(IsJavascriptEnabled(contents));

  // The javascript content setting policy overrides the javascript policy.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  EXPECT_FALSE(IsJavascriptEnabled(contents));
  policies.Set(key::kDefaultJavaScriptSetting, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               base::Value::CreateIntegerValue(CONTENT_SETTING_ALLOW));
  provider_.UpdateChromePolicy(policies);
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  EXPECT_TRUE(IsJavascriptEnabled(contents));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, SavingBrowserHistoryDisabled) {
  // Verifies that browsing history is not saved.
  PolicyMap policies;
  policies.Set(key::kSavingBrowserHistoryDisabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  provider_.UpdateChromePolicy(policies);
  GURL url = ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("empty.html")));
  ui_test_utils::NavigateToURL(browser(), url);
  // Verify that the navigation wasn't saved in the history.
  ui_test_utils::HistoryEnumerator enumerator1(browser()->profile());
  EXPECT_EQ(0u, enumerator1.urls().size());

  // Now flip the policy and try again.
  policies.Set(key::kSavingBrowserHistoryDisabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false));
  provider_.UpdateChromePolicy(policies);
  ui_test_utils::NavigateToURL(browser(), url);
  // Verify that the navigation was saved in the history.
  ui_test_utils::HistoryEnumerator enumerator2(browser()->profile());
  ASSERT_EQ(1u, enumerator2.urls().size());
  EXPECT_EQ(url, enumerator2.urls()[0]);
}

IN_PROC_BROWSER_TEST_F(PolicyTest, TranslateEnabled) {
  // Verifies that translate can be forced enabled or disabled by policy.

  // Get the |infobar_helper|, and verify that there are no infobars on startup.
  content::WebContents* contents = chrome::GetActiveWebContents(browser());
  ASSERT_TRUE(contents);
  TabContents* tab_contents = TabContents::FromWebContents(contents);
  ASSERT_TRUE(tab_contents);
  InfoBarTabHelper* infobar_helper = tab_contents->infobar_tab_helper();
  ASSERT_TRUE(infobar_helper);
  EXPECT_EQ(0u, infobar_helper->GetInfoBarCount());

  // Force enable the translate feature.
  PolicyMap policies;
  policies.Set(key::kTranslateEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  provider_.UpdateChromePolicy(policies);
  // Instead of waiting for NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED, this test
  // waits for NOTIFICATION_TAB_LANGUAGE_DETERMINED because that's what the
  // TranslateManager observes. This allows checking that an infobar is NOT
  // shown below, without polling for infobars for some indeterminate amount
  // of time.
  GURL url = ui_test_utils::GetTestUrl(
      FilePath(), FilePath(FILE_PATH_LITERAL("french_page.html")));
  content::WindowedNotificationObserver language_observer1(
      chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(browser(), url);
  language_observer1.Wait();
  // Verify that the translate infobar showed up.
  ASSERT_EQ(1u, infobar_helper->GetInfoBarCount());
  InfoBarDelegate* infobar_delegate = infobar_helper->GetInfoBarDelegateAt(0);
  TranslateInfoBarDelegate* delegate =
      infobar_delegate->AsTranslateInfoBarDelegate();
  ASSERT_TRUE(delegate);
  EXPECT_EQ(TranslateInfoBarDelegate::BEFORE_TRANSLATE, delegate->type());
  EXPECT_EQ("fr", delegate->GetOriginalLanguageCode());

  // Now force disable translate.
  ui_test_utils::CloseAllInfoBars(tab_contents);
  EXPECT_EQ(0u, infobar_helper->GetInfoBarCount());
  policies.Set(key::kTranslateEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false));
  provider_.UpdateChromePolicy(policies);
  // Navigating to the same URL now doesn't trigger an infobar.
  content::WindowedNotificationObserver language_observer2(
      chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(browser(), url);
  language_observer2.Wait();
  EXPECT_EQ(0u, infobar_helper->GetInfoBarCount());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, URLBlacklist) {
  // Checks that URLs can be blacklisted, and that exceptions can be made to
  // the blacklist.
  const GURL kAAA("http://aaa.com/empty.html");
  const GURL kBBB("http://bbb.com/empty.html");
  const GURL kSUB_BBB("http://sub.bbb.com/empty.html");
  const GURL kBBB_PATH("http://bbb.com/policy/device_management");
  // Filter |kURLS| on IO thread, so that requests to those hosts end up
  // as URLRequestMockHTTPJobs.
  const GURL* kURLS[] = {
    &kAAA,
    &kBBB,
    &kSUB_BBB,
    &kBBB_PATH,
  };
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::Bind(RedirectHostsToTestDataOnIOThread, kURLS, arraysize(kURLS)),
      MessageLoop::QuitClosure());
  content::RunMessageLoop();

  // Verify that all the URLs can be opened without a blacklist.
  for (size_t i = 0; i < arraysize(kURLS); ++i)
    CheckCanOpenURL(browser(), *kURLS[i]);

  // Set a blacklist.
  base::ListValue blacklist;
  blacklist.Append(base::Value::CreateStringValue("bbb.com"));
  PolicyMap policies;
  policies.Set(key::kURLBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, blacklist.DeepCopy());
  provider_.UpdateChromePolicy(policies);
  // All bbb.com URLs are blocked.
  CheckCanOpenURL(browser(), kAAA);
  for (size_t i = 1; i < arraysize(kURLS); ++i)
    CheckURLIsBlocked(browser(), *kURLS[i]);

  // Whitelist some sites of bbb.com.
  base::ListValue whitelist;
  whitelist.Append(base::Value::CreateStringValue("sub.bbb.com"));
  whitelist.Append(base::Value::CreateStringValue("bbb.com/policy"));
  policies.Set(key::kURLWhitelist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, whitelist.DeepCopy());
  provider_.UpdateChromePolicy(policies);
  CheckCanOpenURL(browser(), kAAA);
  CheckURLIsBlocked(browser(), kBBB);
  CheckCanOpenURL(browser(), kSUB_BBB);
  CheckCanOpenURL(browser(), kBBB_PATH);
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DisableScreenshotsFeedback) {
  // Make sure current screenshot can be taken and displayed on feedback page.
  TestScreenshotFeedback(true);

  // Check if banning screenshots disabled feedback page's ability to grab a
  // screenshot.
  TestScreenshotFeedback(false);
}

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
#endif

namespace {

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
  }

  void HomepageIsNotNTP() {
    // Verifies that policy can set the startup pages to the homepage, when
    // the homepage is not the NTP.
    PolicyMap policies;
    policies.Set(
        key::kRestoreOnStartup, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        base::Value::CreateIntegerValue(
            SessionStartupPref::kPrefValueHomePage));
    policies.Set(
        key::kHomepageIsNewTabPage, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        base::Value::CreateBooleanValue(false));
    policies.Set(
        key::kHomepageLocation, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        base::Value::CreateStringValue(kURLs[1]));
    provider_.UpdateChromePolicy(policies);

    expected_urls_.push_back(GURL(kURLs[1]));
  }

  void HomepageIsNTP() {
    // Verifies that policy can set the startup pages to the homepage, when
    // the homepage is the NTP.
    PolicyMap policies;
    policies.Set(
        key::kRestoreOnStartup, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        base::Value::CreateIntegerValue(
            SessionStartupPref::kPrefValueHomePage));
    policies.Set(
        key::kHomepageIsNewTabPage, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        base::Value::CreateBooleanValue(true));
    provider_.UpdateChromePolicy(policies);

    expected_urls_.push_back(GURL(kURLs[0]));
  }

  void ListOfURLs() {
    // Verifies that policy can set the startup pages to a list of URLs.
    base::ListValue urls;
    for (size_t i = 0; i < arraysize(kURLs); ++i) {
      urls.Append(base::Value::CreateStringValue(kURLs[i]));
      expected_urls_.push_back(GURL(kURLs[i]));
    }
    PolicyMap policies;
    policies.Set(
        key::kRestoreOnStartup, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        base::Value::CreateIntegerValue(SessionStartupPref::kPrefValueURLs));
    policies.Set(
        key::kRestoreOnStartupURLs, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        urls.DeepCopy());
    provider_.UpdateChromePolicy(policies);
  }

  void NTP() {
    // Verifies that policy can set the startup page to the NTP.
    PolicyMap policies;
    policies.Set(
        key::kRestoreOnStartup, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        base::Value::CreateIntegerValue(SessionStartupPref::kPrefValueNewTab));
    provider_.UpdateChromePolicy(policies);
    expected_urls_.push_back(GURL(kURLs[0]));
  }

  void Last() {
    // Verifies that policy can set the startup pages to the last session.
    PolicyMap policies;
    policies.Set(
        key::kRestoreOnStartup, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        base::Value::CreateIntegerValue(SessionStartupPref::kPrefValueLast));
    provider_.UpdateChromePolicy(policies);
    // This should restore the tabs opened at PRE_RunTest below.
    for (size_t i = 0; i < arraysize(kURLs); ++i)
      expected_urls_.push_back(GURL(kURLs[i]));
  }

  std::vector<GURL> expected_urls_;
};

IN_PROC_BROWSER_TEST_P(RestoreOnStartupPolicyTest, PRE_RunTest) {
  // Open some tabs to verify if they are restored after the browser restarts.
  // Most policy settings override this, except kPrefValueLast which enforces
  // a restore.
  ui_test_utils::NavigateToURL(browser(), GURL(kURLs[0]));
  for (size_t i = 1; i < arraysize(kURLs); ++i) {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::AddSelectedTabWithURL(browser(), GURL(kURLs[i]),
                                  content::PAGE_TRANSITION_LINK);
    observer.Wait();
  }
}

IN_PROC_BROWSER_TEST_P(RestoreOnStartupPolicyTest, RunTest) {
  TabStripModel* model = browser()->tab_strip_model();
  int size = static_cast<int>(expected_urls_.size());
  EXPECT_EQ(size, model->count());
  for (int i = 0; i < size && i < model->count(); ++i) {
    EXPECT_EQ(expected_urls_[i],
              model->GetTabContentsAt(i)->web_contents()->GetURL());
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

}  // namespace policy
