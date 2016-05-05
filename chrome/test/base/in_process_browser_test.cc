// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"

#include "ash/ash_switches.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_file_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/non_thread_safe.h"
#include "build/build_config.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/google/core/browser/google_util.h"
#include "components/os_crypt/os_crypt.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#include "chrome/test/base/scoped_bundle_swizzler_mac.h"
#endif

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "ui/base/win/atl_module.h"
#endif

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_service.h"
#endif

#if !defined(OS_ANDROID)
#include "components/storage_monitor/test_storage_monitor.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#endif

#if defined(USE_ASH)
#include "chrome/test/base/default_ash_event_generator_delegate.h"
#endif

namespace {

// Passed as value of kTestType.
const char kBrowserTestType[] = "browser";

}  // namespace

// Library used for testing accessibility.
const base::FilePath::CharType kAXSTesting[] =
    FILE_PATH_LITERAL("third_party/accessibility-audit/axs_testing.js");
// JavaScript snippet to configure and run the accessibility audit.
const char kAccessibilityTestString[] =
    "var config = new axs.AuditConfiguration();"
    "/* Disable warning about rules that cannot be checked. */"
    "config.showUnsupportedRulesWarning = false;"
    "config.auditRulesToIgnore = ["
    "  /*"
    "   * The 'elements with meaningful background image' accessibility"
    "   * audit (AX_IMAGE_01) does not apply, since Chrome doesn't"
    "   * disable background images in high-contrast mode like some"
    "   * browsers do."
    "   */"
    "  'elementsWithMeaningfulBackgroundImage',"
    "  /*"
    "   * Most WebUI pages are inside an IFrame, so the 'web page should"
    "   * have a title that describes topic or purpose' test (AX_TITLE_01)"
    "   * generally does not apply."
    "   */"
    "  'pageWithoutTitle',"
    "  /*"
    "   * Enable when crbug.com/267035 is fixed."
    "   * Until then it's just noise."
    "   */"
    "  'lowContrastElements'"
    "];"
    "var result = axs.Audit.run(config);"
    "var error = '';"
    "for (var i = 0; i < result.length; ++i) {"
    "  if (result[i].result == axs.constants.AuditResult.FAIL) {"
    "    error = axs.Audit.createReport(result);"
    "    break;"
    "  }"
    "}"
    "domAutomationController.send(error);";

InProcessBrowserTest::InProcessBrowserTest()
    : browser_(NULL),
      exit_when_last_browser_closes_(true),
      open_about_blank_on_browser_launch_(true),
      multi_desktop_test_(false),
      run_accessibility_checks_for_test_case_(false)
#if defined(OS_MACOSX)
      , autorelease_pool_(NULL)
#endif  // OS_MACOSX
    {
#if defined(OS_MACOSX)
  // TODO(phajdan.jr): Make browser_tests self-contained on Mac, remove this.
  // Before we run the browser, we have to hack the path to the exe to match
  // what it would be if Chrome was running, because it is used to fork renderer
  // processes, on Linux at least (failure to do so will cause a browser_test to
  // be run instead of a renderer).
  base::FilePath chrome_path;
  CHECK(PathService::Get(base::FILE_EXE, &chrome_path));
  chrome_path = chrome_path.DirName();
  chrome_path = chrome_path.Append(chrome::kBrowserProcessExecutablePath);
  CHECK(PathService::Override(base::FILE_EXE, chrome_path));
#endif  // defined(OS_MACOSX)

  CreateTestServer(base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  base::FilePath src_dir;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));

  // chrome::DIR_TEST_DATA isn't going to be setup until after we call
  // ContentMain. However that is after tests' constructors or SetUp methods,
  // which sometimes need it. So just override it.
  CHECK(PathService::Override(chrome::DIR_TEST_DATA,
                              src_dir.AppendASCII("chrome/test/data")));

#if defined(OS_MACOSX)
  bundle_swizzler_.reset(new ScopedBundleSwizzlerMac);
#endif

#if defined(USE_ASH)
  DefaultAshEventGeneratorDelegate::GetInstance();
#endif
}

InProcessBrowserTest::~InProcessBrowserTest() {
}

void InProcessBrowserTest::SetUp() {
  // Browser tests will create their own g_browser_process later.
  DCHECK(!g_browser_process);

  // Clear the FeatureList instance from base/test/test_suite.cc. Since this is
  // a browser test, a FeatureList will be registered as part of normal browser
  // start up in ChromeBrowserMainParts::SetupMetricsAndFieldTrials().
  base::FeatureList::ClearInstanceForTesting();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Auto-reload breaks many browser tests, which assume error pages won't be
  // reloaded out from under them. Tests that expect or desire this behavior can
  // append switches::kEnableOfflineAutoReload, which will override the disable
  // here.
  command_line->AppendSwitch(switches::kDisableOfflineAutoReload);

  // Allow subclasses to change the command line before running any tests.
  SetUpCommandLine(command_line);
  // Add command line arguments that are used by all InProcessBrowserTests.
  PrepareTestCommandLine(command_line);

  // Create a temporary user data directory if required.
  ASSERT_TRUE(CreateUserDataDirectory())
      << "Could not create user data directory.";

  // Allow subclasses the opportunity to make changes to the default user data
  // dir before running any tests.
  ASSERT_TRUE(SetUpUserDataDirectory())
      << "Could not set up user data directory.";

#if defined(OS_CHROMEOS)
  // Make sure that the log directory exists.
  base::FilePath log_dir = logging::GetSessionLogFile(*command_line).DirName();
  base::CreateDirectory(log_dir);
  // Disable IME extension loading to avoid many browser tests failures.
  chromeos::input_method::DisableExtensionLoading();
  if (!command_line->HasSwitch(ash::switches::kAshHostWindowBounds)) {
    // Adjusting window location & size so that the ash desktop window fits
    // inside the Xvfb'x defualt resolution.
    command_line->AppendSwitchASCII(ash::switches::kAshHostWindowBounds,
                                    "0+0-1280x800");
  }
#endif  // defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
  // Always use the MockKeychain if OS encription is used (which is when
  // anything sensitive gets stored, including Cookies).  Without this,
  // many tests will hang waiting for a user to approve KeyChain access.
  OSCrypt::UseMockKeychain(true);
#endif

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalService::set_state_for_testing(
      CaptivePortalService::DISABLED_FOR_TESTING);
#endif

  chrome_browser_net::NetErrorTabHelper::set_state_for_testing(
      chrome_browser_net::NetErrorTabHelper::TESTING_FORCE_DISABLED);

  google_util::SetMockLinkDoctorBaseURLForTesting();

  BrowserTestBase::SetUp();
}

void InProcessBrowserTest::PrepareTestCommandLine(
    base::CommandLine* command_line) {
  // Propagate commandline settings from test_launcher_utils.
  test_launcher_utils::PrepareBrowserCommandLineForTests(command_line);

  // This is a Browser test.
  command_line->AppendSwitchASCII(switches::kTestType, kBrowserTestType);

#if defined(OS_MACOSX)
  // Explicitly set the path of the binary used for child processes, otherwise
  // they'll try to use browser_tests which doesn't contain ChromeMain.
  base::FilePath subprocess_path;
  PathService::Get(base::FILE_EXE, &subprocess_path);
  // Recreate the real environment, run the helper within the app bundle.
  subprocess_path = subprocess_path.DirName().DirName();
  DCHECK_EQ(subprocess_path.BaseName().value(), "Contents");
  subprocess_path =
      subprocess_path.Append("Versions").Append(chrome::kChromeVersion);
  subprocess_path =
      subprocess_path.Append(chrome::kHelperProcessExecutablePath);
  command_line->AppendSwitchPath(switches::kBrowserSubprocessPath,
                                 subprocess_path);
#endif

  // TODO(pkotwicz): Investigate if we can remove this switch.
  if (exit_when_last_browser_closes_)
    command_line->AppendSwitch(switches::kDisableZeroBrowsersOpenForTests);

  if (open_about_blank_on_browser_launch_ && command_line->GetArgs().empty())
    command_line->AppendArg(url::kAboutBlankURL);
}

bool InProcessBrowserTest::RunAccessibilityChecks(std::string* error_message) {
  if (!browser()) {
    *error_message = "browser is NULL";
    return false;
  }
  auto tab_strip = browser()->tab_strip_model();
  if (!tab_strip) {
    *error_message = "tab_strip is NULL";
    return false;
  }
  auto web_contents = tab_strip->GetActiveWebContents();
  if (!web_contents) {
    *error_message = "web_contents is NULL";
    return false;
  }
  auto focused_frame = web_contents->GetFocusedFrame();
  if (!focused_frame) {
    *error_message = "focused_frame is NULL";
    return false;
  }

  // Load accessibility library.
  base::FilePath src_dir;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &src_dir)) {
    *error_message = "PathService::Get failed";
    return false;
  }
  base::FilePath script_path = src_dir.Append(kAXSTesting);
  std::string script;
  if (!base::ReadFileToString(script_path, &script)) {
    *error_message = "Could not read accessibility library";
    return false;
  }
  if (!content::ExecuteScript(web_contents, script)) {
    *error_message = "Failed to load accessibility library";
    return false;
  }

  // Run accessibility audit.
  if (!content::ExecuteScriptAndExtractString(focused_frame,
                                              kAccessibilityTestString,
                                              error_message)) {
    *error_message = "Failed to run accessibility audit";
    return false;
  }

  // Test result should be empty if there are no errors.
  return error_message->empty();
}

bool InProcessBrowserTest::CreateUserDataDirectory() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::FilePath user_data_dir =
      command_line->GetSwitchValuePath(switches::kUserDataDir);
  if (user_data_dir.empty()) {
    if (temp_user_data_dir_.CreateUniqueTempDir() &&
        temp_user_data_dir_.IsValid()) {
      user_data_dir = temp_user_data_dir_.path();
    } else {
      LOG(ERROR) << "Could not create temporary user data directory \""
                 << temp_user_data_dir_.path().value() << "\".";
      return false;
    }
  }
  return test_launcher_utils::OverrideUserDataDir(user_data_dir);
}

void InProcessBrowserTest::TearDown() {
  DCHECK(!g_browser_process);
#if defined(OS_WIN)
  com_initializer_.reset();
#endif
  BrowserTestBase::TearDown();
}

void InProcessBrowserTest::CloseBrowserSynchronously(Browser* browser) {
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(browser));
  CloseBrowserAsynchronously(browser);
  observer.Wait();
}

void InProcessBrowserTest::CloseBrowserAsynchronously(Browser* browser) {
  browser->window()->Close();
#if defined(OS_MACOSX)
  // BrowserWindowController depends on the auto release pool being recycled
  // in the message loop to delete itself.
  AutoreleasePool()->Recycle();
#endif
}

void InProcessBrowserTest::CloseAllBrowsers() {
  chrome::CloseAllBrowsers();
#if defined(OS_MACOSX)
  // BrowserWindowController depends on the auto release pool being recycled
  // in the message loop to delete itself.
  AutoreleasePool()->Recycle();
#endif
}

// TODO(alexmos): This function should expose success of the underlying
// navigation to tests, which should make sure navigations succeed when
// appropriate. See https://crbug.com/425335
void InProcessBrowserTest::AddTabAtIndexToBrowser(
    Browser* browser,
    int index,
    const GURL& url,
    ui::PageTransition transition,
    bool check_navigation_success) {
  chrome::NavigateParams params(browser, url, transition);
  params.tabstrip_index = index;
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);

  if (check_navigation_success)
    content::WaitForLoadStop(params.target_contents);
  else
    content::WaitForLoadStopWithoutSuccessCheck(params.target_contents);
}

void InProcessBrowserTest::AddTabAtIndex(
    int index,
    const GURL& url,
    ui::PageTransition transition) {
  AddTabAtIndexToBrowser(browser(), index, url, transition, true);
}

bool InProcessBrowserTest::SetUpUserDataDirectory() {
  return true;
}

#if !defined(OS_MACOSX)
void InProcessBrowserTest::OpenDevToolsWindow(
    content::WebContents* web_contents) {
  ASSERT_FALSE(content::DevToolsAgentHost::HasFor(web_contents));
  DevToolsWindow::OpenDevToolsWindow(web_contents);
  ASSERT_TRUE(content::DevToolsAgentHost::HasFor(web_contents));
}

Browser* InProcessBrowserTest::OpenURLOffTheRecord(Profile* profile,
                                                   const GURL& url) {
  chrome::OpenURLOffTheRecord(profile, url);
  Browser* browser =
      chrome::FindTabbedBrowser(profile->GetOffTheRecordProfile(), false);
  content::TestNavigationObserver observer(
      browser->tab_strip_model()->GetActiveWebContents());
  observer.Wait();
  return browser;
}

// Creates a browser with a single tab (about:blank), waits for the tab to
// finish loading and shows the browser.
Browser* InProcessBrowserTest::CreateBrowser(Profile* profile) {
  Browser* browser = new Browser(Browser::CreateParams(profile));
  AddBlankTabAndShow(browser);
  return browser;
}

Browser* InProcessBrowserTest::CreateIncognitoBrowser() {
  // Create a new browser with using the incognito profile.
  Browser* incognito = new Browser(
      Browser::CreateParams(browser()->profile()->GetOffTheRecordProfile()));
  AddBlankTabAndShow(incognito);
  return incognito;
}

Browser* InProcessBrowserTest::CreateBrowserForPopup(Profile* profile) {
  Browser* browser =
      new Browser(Browser::CreateParams(Browser::TYPE_POPUP, profile));
  AddBlankTabAndShow(browser);
  return browser;
}

Browser* InProcessBrowserTest::CreateBrowserForApp(
    const std::string& app_name,
    Profile* profile) {
  Browser* browser = new Browser(
      Browser::CreateParams::CreateForApp(
          app_name, false /* trusted_source */, gfx::Rect(), profile));
  AddBlankTabAndShow(browser);
  return browser;
}
#endif  // !defined(OS_MACOSX)

void InProcessBrowserTest::AddBlankTabAndShow(Browser* browser) {
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::AddSelectedTabWithURL(browser,
                                GURL(url::kAboutBlankURL),
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  observer.Wait();

  browser->window()->Show();
}

#if !defined(OS_MACOSX)
base::CommandLine InProcessBrowserTest::GetCommandLineForRelaunch() {
  base::CommandLine new_command_line(
      base::CommandLine::ForCurrentProcess()->GetProgram());
  base::CommandLine::SwitchMap switches =
      base::CommandLine::ForCurrentProcess()->GetSwitches();
  switches.erase(switches::kUserDataDir);
  switches.erase(content::kSingleProcessTestsFlag);
  switches.erase(switches::kSingleProcess);
  new_command_line.AppendSwitch(content::kLaunchAsBrowser);

  base::FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  new_command_line.AppendSwitchPath(switches::kUserDataDir, user_data_dir);

  for (base::CommandLine::SwitchMap::const_iterator iter = switches.begin();
       iter != switches.end(); ++iter) {
    new_command_line.AppendSwitchNative((*iter).first, (*iter).second);
  }
  return new_command_line;
}
#endif

void InProcessBrowserTest::RunTestOnMainThreadLoop() {
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();

  // Pump startup related events.
  content::RunAllPendingInMessageLoop();

  const BrowserList* active_browser_list = BrowserList::GetInstance();
  if (!active_browser_list->empty()) {
    browser_ = active_browser_list->get(0);
#if defined(USE_ASH)
    // There are cases where windows get created maximized by default.
    if (browser_->window()->IsMaximized())
      browser_->window()->Restore();
#endif
    content::WaitForLoadStop(
        browser_->tab_strip_model()->GetActiveWebContents());
  }

#if !defined(OS_ANDROID)
  // Do not use the real StorageMonitor for tests, which introduces another
  // source of variability and potential slowness.
  ASSERT_TRUE(storage_monitor::TestStorageMonitor::CreateForBrowserTests());
#endif

#if defined(OS_MACOSX)
  // On Mac, without the following autorelease pool, code which is directly
  // executed (as opposed to executed inside a message loop) would autorelease
  // objects into a higher-level pool. This pool is not recycled in-sync with
  // the message loops' pools and causes problems with code relying on
  // deallocation via an autorelease pool (such as browser window closure and
  // browser shutdown). To avoid this, the following pool is recycled after each
  // time code is directly executed.
  autorelease_pool_ = new base::mac::ScopedNSAutoreleasePool;
#endif

  // Pump any pending events that were created as a result of creating a
  // browser.
  content::RunAllPendingInMessageLoop();

  // run_accessibility_checks_for_test_case_ must be set before calling
  // SetUpOnMainThread or RunTestOnMainThread so that one or all tests can
  // enable/disable the accessibility audit.
  run_accessibility_checks_for_test_case_ = false;

  SetUpOnMainThread();
#if defined(OS_MACOSX)
  autorelease_pool_->Recycle();
#endif

  if (!HasFatalFailure())
    RunTestOnMainThread();
#if defined(OS_MACOSX)
  autorelease_pool_->Recycle();
#endif

  if (run_accessibility_checks_for_test_case_) {
    std::string error_message;
    EXPECT_TRUE(RunAccessibilityChecks(&error_message));
    EXPECT_EQ("", error_message);
  }

  // Invoke cleanup and quit even if there are failures. This is similar to
  // gtest in that it invokes TearDown even if Setup fails.
  TearDownOnMainThread();
#if defined(OS_MACOSX)
  autorelease_pool_->Recycle();
#endif

  // Sometimes tests leave Quit tasks in the MessageLoop (for shame), so let's
  // run all pending messages here to avoid preempting the QuitBrowsers tasks.
  // TODO(jbates) Once crbug.com/134753 is fixed, this can be removed because it
  // will not be possible to post Quit tasks.
  content::RunAllPendingInMessageLoop();

  QuitBrowsers();
  // BrowserList should be empty at this point.
  CHECK(BrowserList::GetInstance()->empty());
}

void InProcessBrowserTest::QuitBrowsers() {
  if (chrome::GetTotalBrowserCount() == 0) {
    chrome::NotifyAppTerminating();
    return;
  }

  // Invoke AttemptExit on a running message loop.
  // AttemptExit exits the message loop after everything has been
  // shut down properly.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&chrome::AttemptExit));
  content::RunMessageLoop();

#if defined(OS_MACOSX)
  // chrome::AttemptExit() will attempt to close all browsers by deleting
  // their tab contents. The last tab contents being removed triggers closing of
  // the browser window.
  //
  // On the Mac, this eventually reaches
  // -[BrowserWindowController windowWillClose:], which will post a deferred
  // -autorelease on itself to ultimately destroy the Browser object. The line
  // below is necessary to pump these pending messages to ensure all Browsers
  // get deleted.
  content::RunAllPendingInMessageLoop();
  delete autorelease_pool_;
  autorelease_pool_ = NULL;
#endif
}
