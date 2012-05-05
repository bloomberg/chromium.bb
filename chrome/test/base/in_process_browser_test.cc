// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/test/test_file_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/renderer/mock_content_renderer_client.h"
#include "content/test/test_browser_thread.h"
#include "content/test/test_launcher.h"
#include "net/base/mock_host_resolver.h"
#include "net/test/test_server.h"
#include "ui/compositor/compositor_switches.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/audio/audio_handler.h"
#elif defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace {

// Passed as value of kTestType.
const char kBrowserTestType[] = "browser";

}  // namespace

InProcessBrowserTest::InProcessBrowserTest()
    : browser_(NULL),
      dom_automation_enabled_(false)
#if defined(OS_POSIX)
      , handle_sigterm_(true)
#endif
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
  FilePath chrome_path;
  CHECK(PathService::Get(base::FILE_EXE, &chrome_path));
  chrome_path = chrome_path.DirName();
  chrome_path = chrome_path.Append(chrome::kBrowserProcessExecutablePath);
  CHECK(PathService::Override(base::FILE_EXE, chrome_path));
#endif  // defined(OS_MACOSX)

  test_server_.reset(new net::TestServer(
      net::TestServer::TYPE_HTTP,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("chrome/test/data"))));
}

InProcessBrowserTest::~InProcessBrowserTest() {
}

void InProcessBrowserTest::SetUp() {
  // Undo TestingBrowserProcess creation in ChromeTestSuite.
  // TODO(phajdan.jr): Extract a smaller test suite so we don't need this.
  DCHECK(g_browser_process);
  delete g_browser_process;
  g_browser_process = NULL;

  CommandLine* command_line = CommandLine::ForCurrentProcess();
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

  // Single-process mode is not set in BrowserMain, so process it explicitly,
  // and set up renderer.
  if (command_line->HasSwitch(switches::kSingleProcess)) {
    content::RenderProcessHost::set_run_renderer_in_process(true);
    single_process_renderer_client_.reset(
        new content::MockContentRendererClient);
    content::GetContentClient()->set_renderer(
        single_process_renderer_client_.get());
  }

#if defined(OS_CHROMEOS)
  // Make sure that the log directory exists.
  FilePath log_dir = logging::GetSessionLogFile(*command_line).DirName();
  file_util::CreateDirectory(log_dir);
#endif  // defined(OS_CHROMEOS)

  host_resolver_ = new net::RuleBasedHostResolverProc(NULL);

  // Something inside the browser does this lookup implicitly. Make it fail
  // to avoid external dependency. It won't break the tests.
  host_resolver_->AddSimulatedFailure("*.google.com");

  // See http://en.wikipedia.org/wiki/Web_Proxy_Autodiscovery_Protocol
  // We don't want the test code to use it.
  host_resolver_->AddSimulatedFailure("wpad");

  net::ScopedDefaultHostResolverProc scoped_host_resolver_proc(
      host_resolver_.get());

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
  BrowserTestBase::SetUp();
}

void InProcessBrowserTest::PrepareTestCommandLine(CommandLine* command_line) {
  // Propagate commandline settings from test_launcher_utils.
  test_launcher_utils::PrepareBrowserCommandLineForTests(command_line);

  if (dom_automation_enabled_)
    command_line->AppendSwitch(switches::kDomAutomationController);

  // This is a Browser test.
  command_line->AppendSwitchASCII(switches::kTestType, kBrowserTestType);

#if defined(OS_WIN)
  // The Windows sandbox requires that the browser and child processes are the
  // same binary.  So we launch browser_process.exe which loads chrome.dll
  command_line->AppendSwitchPath(switches::kBrowserSubprocessPath,
                                 command_line->GetProgram());
#else
  // Explicitly set the path of the binary used for child processes, otherwise
  // they'll try to use browser_tests which doesn't contain ChromeMain.
  FilePath subprocess_path;
  PathService::Get(base::FILE_EXE, &subprocess_path);
#if defined(OS_MACOSX)
  // Recreate the real environment, run the helper within the app bundle.
  subprocess_path = subprocess_path.DirName().DirName();
  DCHECK_EQ(subprocess_path.BaseName().value(), "Contents");
  subprocess_path =
      subprocess_path.Append("Versions").Append(chrome::kChromeVersion);
  subprocess_path =
      subprocess_path.Append(chrome::kHelperProcessExecutablePath);
#endif
  command_line->AppendSwitchPath(switches::kBrowserSubprocessPath,
                                 subprocess_path);
#endif

  // TODO(pkotwicz): Investigate if we can remove this switch.
  command_line->AppendSwitch(switches::kDisableZeroBrowsersOpenForTests);

  if (!command_line->HasSwitch(switches::kHomePage)) {
      command_line->AppendSwitchASCII(
          switches::kHomePage, chrome::kAboutBlankURL);
  }
  if (command_line->GetArgs().empty())
    command_line->AppendArg(chrome::kAboutBlankURL);
}

bool InProcessBrowserTest::CreateUserDataDirectory() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  FilePath user_data_dir =
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
  BrowserTestBase::TearDown();
}

content::BrowserContext* InProcessBrowserTest::GetBrowserContext() {
  return browser_->profile();
}

content::ResourceContext* InProcessBrowserTest::GetResourceContext() {
  return browser_->profile()->GetResourceContext();
}

void InProcessBrowserTest::AddTabAtIndexToBrowser(
    Browser* browser,
    int index,
    const GURL& url,
    content::PageTransition transition) {
  browser::NavigateParams params(browser, url, transition);
  params.tabstrip_index = index;
  params.disposition = NEW_FOREGROUND_TAB;
  browser::Navigate(&params);
}

void InProcessBrowserTest::AddTabAtIndex(
    int index,
    const GURL& url,
    content::PageTransition transition) {
  AddTabAtIndexToBrowser(browser(), index, url, transition);
}

bool InProcessBrowserTest::SetUpUserDataDirectory() {
  return true;
}

// Creates a browser with a single tab (about:blank), waits for the tab to
// finish loading and shows the browser.
Browser* InProcessBrowserTest::CreateBrowser(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  AddBlankTabAndShow(browser);
  return browser;
}

Browser* InProcessBrowserTest::CreateIncognitoBrowser() {
  // Create a new browser with using the incognito profile.
  Browser* incognito =
      Browser::Create(browser()->profile()->GetOffTheRecordProfile());
  AddBlankTabAndShow(incognito);
  return incognito;
}

Browser* InProcessBrowserTest::CreateBrowserForPopup(Profile* profile) {
  Browser* browser = Browser::CreateWithParams(
      Browser::CreateParams(Browser::TYPE_POPUP, profile));
  AddBlankTabAndShow(browser);
  return browser;
}

Browser* InProcessBrowserTest::CreateBrowserForApp(
    const std::string& app_name,
    Profile* profile) {
  Browser* browser = Browser::CreateWithParams(
      Browser::CreateParams::CreateForApp(
          Browser::TYPE_POPUP, app_name, gfx::Rect(), profile));
  AddBlankTabAndShow(browser);
  return browser;
}

void InProcessBrowserTest::AddBlankTabAndShow(Browser* browser) {
  ui_test_utils::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
       content::NotificationService::AllSources());
  browser->AddSelectedTabWithURL(
      GURL(chrome::kAboutBlankURL), content::PAGE_TRANSITION_START_PAGE);
  observer.Wait();

  browser->window()->Show();
}

#if !defined(OS_MACOSX)
CommandLine InProcessBrowserTest::GetCommandLineForRelaunch() {
  CommandLine new_command_line(CommandLine::ForCurrentProcess()->GetProgram());
  CommandLine::SwitchMap switches =
      CommandLine::ForCurrentProcess()->GetSwitches();
  switches.erase(switches::kUserDataDir);
  switches.erase(test_launcher::kSingleProcessTestsFlag);
  switches.erase(test_launcher::kSingleProcessTestsAndChromeFlag);
  new_command_line.AppendSwitch(ChromeTestSuite::kLaunchAsBrowser);

#if defined(USE_AURA)
  // Copy what UITestBase::SetLaunchSwitches() does, and also what
  // ChromeTestSuite does if the process had went into the test path. Otherwise
  // tests will fail on bots.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableTestCompositor)) {
    new_command_line.AppendSwitch(switches::kTestCompositor);
  }
#endif

  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  new_command_line.AppendSwitchPath(switches::kUserDataDir, user_data_dir);

  for (CommandLine::SwitchMap::const_iterator iter = switches.begin();
        iter != switches.end(); ++iter) {
    new_command_line.AppendSwitchNative((*iter).first, (*iter).second);
  }
  return new_command_line;
}
#endif

#if defined(OS_POSIX)
// On SIGTERM (sent by the runner on timeouts), dump a stack trace (to make
// debugging easier) and also exit with a known error code (so that the test
// framework considers this a failure -- http://crbug.com/57578).
static void DumpStackTraceSignalHandler(int signal) {
  base::debug::StackTrace().PrintBacktrace();
  _exit(128 + signal);
}
#endif  // defined(OS_POSIX)

void InProcessBrowserTest::RunTestOnMainThreadLoop() {
#if defined(OS_POSIX)
  if (handle_sigterm_)
    signal(SIGTERM, DumpStackTraceSignalHandler);
#endif  // defined(OS_POSIX)

  // Pump startup related events.
  ui_test_utils::RunAllPendingInMessageLoop();

#if defined(OS_MACOSX)
  autorelease_pool_->Recycle();
#endif

  if (BrowserList::size()) {
    browser_ = *BrowserList::begin();
    ui_test_utils::WaitForLoadStop(browser_->GetSelectedWebContents());
  }

  // Pump any pending events that were created as a result of creating a
  // browser.
  ui_test_utils::RunAllPendingInMessageLoop();

  SetUpOnMainThread();
#if defined(OS_MACOSX)
  autorelease_pool_->Recycle();
#endif

  if (!HasFatalFailure())
    RunTestOnMainThread();
#if defined(OS_MACOSX)
  autorelease_pool_->Recycle();
#endif

  // Invoke cleanup and quit even if there are failures. This is similar to
  // gtest in that it invokes TearDown even if Setup fails.
  CleanUpOnMainThread();
#if defined(OS_MACOSX)
  autorelease_pool_->Recycle();
#endif

  QuitBrowsers();
}

void InProcessBrowserTest::QuitBrowsers() {
  if (BrowserList::size() == 0)
    return;

  // Invoke CloseAllBrowsersAndMayExit on a running message loop.
  // CloseAllBrowsersAndMayExit exits the message loop after everything has been
  // shut down properly.
  MessageLoopForUI::current()->PostTask(FROM_HERE,
                                        base::Bind(&BrowserList::AttemptExit));
  ui_test_utils::RunMessageLoop();

#if defined(OS_MACOSX)
  delete autorelease_pool_;
  autorelease_pool_ = NULL;
#endif
}
