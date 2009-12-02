// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/net/fake_external_tab.h"

#include <exdisp.h>

#include "app/app_paths.h"
#include "app/resource_bundle.h"
#include "app/win_util.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/path_service.h"
#include "base/scoped_bstr_win.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_variant_win.h"

#include "chrome/browser/browser_prefs.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome_frame/utils.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/net/dialog_watchdog.h"
#include "chrome_frame/test/net/test_automation_resource_message_filter.h"

namespace {

// A special command line switch to allow developers to manually launch the
// browser and debug CF inside the browser.
const wchar_t kManualBrowserLaunch[] = L"manual-browser";

// Pops up a message box after the test environment has been set up
// and before tearing it down.  Useful for when debugging tests and not
// the test environment that's been set up.
const wchar_t kPromptAfterSetup[] = L"prompt-after-setup";

const int kTestServerPort = 4666;
// The test HTML we use to initialize Chrome Frame.
// Note that there's a little trick in there to avoid an extra URL request
// that the browser will otherwise make for the site's favicon.
// If we don't do this the browser will create a new URL request after
// the CF page has been initialized and that URL request will confuse the
// global URL instance counter in the unit tests and subsequently trip
// some DCHECKs.
const char kChromeFrameHtml[] = "<html><head>"
    "<meta http-equiv=\"X-UA-Compatible\" content=\"chrome=1\" />"
    "<link rel=\"shortcut icon\" href=\"file://c:\\favicon.ico\"/>"
    "</head><body>Chrome Frame should now be loaded</body></html>";

bool ShouldLaunchBrowser() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(kManualBrowserLaunch);
}

bool PromptAfterSetup() {
  return CommandLine::ForCurrentProcess()->HasSwitch(kPromptAfterSetup);
}

}  // end namespace

FakeExternalTab::FakeExternalTab() {
  PathService::Get(chrome::DIR_USER_DATA, &overridden_user_dir_);
  GetProfilePath(&user_data_dir_);
  PathService::Override(chrome::DIR_USER_DATA, user_data_dir_);
  process_singleton_.reset(new ProcessSingleton(user_data_dir_));
}

FakeExternalTab::~FakeExternalTab() {
  if (!overridden_user_dir_.empty()) {
    PathService::Override(chrome::DIR_USER_DATA, overridden_user_dir_);
  }
}

std::wstring FakeExternalTab::GetProfileName() {
  return L"iexplore";
}

bool FakeExternalTab::GetProfilePath(FilePath* path) {
  if (!chrome::GetChromeFrameUserDataDirectory(path))
    return false;
  *path = path->Append(GetProfileName());
  return true;
}

void FakeExternalTab::Initialize() {
  DCHECK(g_browser_process == NULL);
  SystemMonitor system_monitor;

  // The gears plugin causes the PluginRequestInterceptor to kick in and it
  // will cause problems when it tries to intercept URL requests.
  PathService::Override(chrome::FILE_GEARS_PLUGIN, FilePath());

  icu_util::Initialize();

  chrome::RegisterPathProvider();
  app::RegisterPathProvider();

  // Load Chrome.dll as our resource dll.
  FilePath dll;
  PathService::Get(base::DIR_MODULE, &dll);
  dll = dll.Append(chrome::kBrowserResourcesDll);
  HMODULE res_mod = ::LoadLibraryExW(dll.value().c_str(),
      NULL, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
  DCHECK(res_mod);
  _AtlBaseModule.SetResourceInstance(res_mod);

  ResourceBundle::InitSharedInstance(L"en-US");

  CommandLine* cmd = CommandLine::ForCurrentProcess();
  cmd->AppendSwitch(switches::kDisableWebResources);
  cmd->AppendSwitch(switches::kSingleProcess);

  browser_process_.reset(new BrowserProcessImpl(*cmd));
  RenderProcessHost::set_run_renderer_in_process(true);
  // BrowserProcessImpl's constructor should set g_browser_process.
  DCHECK(g_browser_process);

  Profile* profile = g_browser_process->profile_manager()->
      GetDefaultProfile(FilePath(user_data()));
  PrefService* prefs = profile->GetPrefs();
  DCHECK(prefs != NULL);

  WebCacheManager::RegisterPrefs(prefs);
  PrefService* local_state = browser_process_->local_state();
  local_state->RegisterStringPref(prefs::kApplicationLocale, L"");
  local_state->RegisterBooleanPref(prefs::kMetricsReportingEnabled, false);

  browser::RegisterLocalState(local_state);

  // Override some settings to avoid hitting some preferences that have not
  // been registered.
  prefs->SetBoolean(prefs::kPasswordManagerEnabled, false);
  prefs->SetBoolean(prefs::kAlternateErrorPagesEnabled, false);
  prefs->SetBoolean(prefs::kSafeBrowsingEnabled, false);

  profile->InitExtensions();
}

void FakeExternalTab::Shutdown() {
  browser_process_.reset();
  g_browser_process = NULL;
  process_singleton_.reset();

  ResourceBundle::CleanupSharedInstance();
}

CFUrlRequestUnittestRunner::CFUrlRequestUnittestRunner(int argc, char** argv)
    : NetTestSuite(argc, argv),
      chrome_frame_html_("/chrome_frame", kChromeFrameHtml) {
  // Register the main thread by instantiating it, but don't call any methods.
  main_thread_.reset(new ChromeThread(ChromeThread::UI,
                                      MessageLoop::current()));
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  fake_chrome_.Initialize();
  pss_subclass_.reset(new ProcessSingletonSubclass(this));
  EXPECT_TRUE(pss_subclass_->Subclass(fake_chrome_.user_data()));
  StartChromeFrameInHostBrowser();
}

CFUrlRequestUnittestRunner::~CFUrlRequestUnittestRunner() {
  fake_chrome_.Shutdown();
}

DWORD WINAPI NavigateIE(void* param) {
  return 0;
  win_util::ScopedCOMInitializer com;
  BSTR url = reinterpret_cast<BSTR>(param);

  bool found = false;
  int retries = 0;
  const int kMaxRetries = 20;
  while (!found && retries < kMaxRetries) {
    ScopedComPtr<IShellWindows> windows;
    HRESULT hr = ::CoCreateInstance(__uuidof(ShellWindows), NULL, CLSCTX_ALL,
        IID_IShellWindows, reinterpret_cast<void**>(windows.Receive()));
    DCHECK(SUCCEEDED(hr)) << "CoCreateInstance";

    if (SUCCEEDED(hr)) {
      hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
      long count = 0;  // NOLINT
      windows->get_Count(&count);
      VARIANT i = { VT_I4 };
      for (i.lVal = 0; i.lVal < count; ++i.lVal) {
        ScopedComPtr<IDispatch> folder;
        windows->Item(i, folder.Receive());
        if (folder != NULL) {
          ScopedComPtr<IWebBrowser2> browser;
          if (SUCCEEDED(browser.QueryFrom(folder))) {
            found = true;
            browser->Stop();
            Sleep(1000);
            VARIANT empty = ScopedVariant::kEmptyVariant;
            hr = browser->Navigate(url, &empty, &empty, &empty, &empty);
            DCHECK(SUCCEEDED(hr)) << "Failed to navigate";
            break;
          }
        }
      }
    }
    if (!found) {
      DLOG(INFO) << "Waiting for browser to initialize...";
      ::Sleep(100);
      retries++;
    }
  }

  DCHECK(retries < kMaxRetries);
  DCHECK(found);

  ::SysFreeString(url);

  return 0;
}

void CFUrlRequestUnittestRunner::StartChromeFrameInHostBrowser() {
  if (!ShouldLaunchBrowser())
    return;

  win_util::ScopedCOMInitializer com;
  chrome_frame_test::CloseAllIEWindows();

  test_http_server_.reset(new test_server::SimpleWebServer(kTestServerPort));
  test_http_server_->AddResponse(&chrome_frame_html_);
  std::wstring url(StringPrintf(L"http://localhost:%i/chrome_frame",
                                kTestServerPort).c_str());

  // Launch IE.  This launches IE correctly on Vista too.
  ScopedHandle ie_process(chrome_frame_test::LaunchIE(url));
  EXPECT_TRUE(ie_process.IsValid());

  // NOTE: If you're running IE8 and CF is not being loaded, you need to
  // disable IE8's prebinding until CF properly handles that situation.
  //
  // HKCU\Software\Microsoft\Internet Explorer\Main
  // Value name: EnablePreBinding (REG_DWORD)
  // Value: 0
}

void CFUrlRequestUnittestRunner::ShutDownHostBrowser() {
  if (ShouldLaunchBrowser()) {
    win_util::ScopedCOMInitializer com;
    chrome_frame_test::CloseAllIEWindows();
  }
}

// Override virtual void Initialize to not call icu initialize
void CFUrlRequestUnittestRunner::Initialize() {
  DCHECK(::GetCurrentThreadId() == test_thread_id_);

  // Start by replicating some of the steps that would otherwise be
  // done by TestSuite::Initialize.  We can't call the base class
  // directly because it will attempt to initialize some things such as
  // ICU that have already been initialized for this process.
  InitializeLogging();
  base::Time::UseHighResolutionTimer(true);

#if !defined(PURIFY) && defined(OS_WIN)
  logging::SetLogAssertHandler(UnitTestAssertHandler);
#endif  // !defined(PURIFY)

  // Next, do some initialization for NetTestSuite.
  NetTestSuite::InitializeTestThread();
}

void CFUrlRequestUnittestRunner::Shutdown() {
  DCHECK(::GetCurrentThreadId() == test_thread_id_);
  NetTestSuite::Shutdown();
}

void CFUrlRequestUnittestRunner::OnConnectAutomationProviderToChannel(
    const std::string& channel_id) {
  Profile* profile = g_browser_process->profile_manager()->
      GetDefaultProfile(fake_chrome_.user_data());

  AutomationProviderList* list =
      g_browser_process->InitAutomationProviderList();
  DCHECK(list);
  list->AddProvider(TestAutomationProvider::NewAutomationProvider(profile,
      channel_id, this));
}

void CFUrlRequestUnittestRunner::OnInitialTabLoaded() {
  test_http_server_.reset();
  StartTests();
}

void CFUrlRequestUnittestRunner::RunMainUIThread() {
  DCHECK(MessageLoop::current());
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);
  MessageLoop::current()->Run();
}

void CFUrlRequestUnittestRunner::StartTests() {
  if (PromptAfterSetup())
    MessageBoxA(NULL, "click ok to run", "", MB_OK);

  DCHECK_EQ(test_thread_.IsValid(), false);
  test_thread_.Set(::CreateThread(NULL, 0, RunAllUnittests, this, 0,
                                  &test_thread_id_));
  DCHECK(test_thread_.IsValid());
}

// static
DWORD CFUrlRequestUnittestRunner::RunAllUnittests(void* param) {
  PlatformThread::SetName("CFUrlRequestUnittestRunner");
  // Needed for some url request tests like the intercept job tests, etc.
  NotificationService service;
  CFUrlRequestUnittestRunner* me =
      reinterpret_cast<CFUrlRequestUnittestRunner*>(param);
  me->Run();
  me->fake_chrome_.ui_loop()->PostTask(FROM_HERE,
      NewRunnableFunction(TakeDownBrowser, me));
  return 0;
}

// static
void CFUrlRequestUnittestRunner::TakeDownBrowser(
    CFUrlRequestUnittestRunner* me) {
  if (PromptAfterSetup())
    MessageBoxA(NULL, "click ok to exit", "", MB_OK);

  me->ShutDownHostBrowser();
}

void CFUrlRequestUnittestRunner::InitializeLogging() {
  FilePath exe;
  PathService::Get(base::FILE_EXE, &exe);
  FilePath log_filename = exe.ReplaceExtension(FILE_PATH_LITERAL("log"));
  logging::InitLogging(log_filename.value().c_str(),
                       logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
                       logging::LOCK_LOG_FILE,
                       logging::DELETE_OLD_LOG_FILE);
  // We want process and thread IDs because we may have multiple processes.
  // Note: temporarily enabled timestamps in an effort to catch bug 6361.
  logging::SetLogItems(true, true, true, true);
}

void FilterDisabledTests() {
  if (::testing::FLAGS_gtest_filter.length() &&
      ::testing::FLAGS_gtest_filter.Compare("*") != 0) {
    // Don't override user specified filters.
    return;
  }

  const char* disabled_tests[] = {
    // Tests disabled since they're testing the same functionality used
    // by the TestAutomationProvider.
    "URLRequestTest.Intercept",
    "URLRequestTest.InterceptNetworkError",
    "URLRequestTest.InterceptRestartRequired",
    "URLRequestTest.InterceptRespectsCancelMain",
    "URLRequestTest.InterceptRespectsCancelRedirect",
    "URLRequestTest.InterceptRespectsCancelFinal",
    "URLRequestTest.InterceptRespectsCancelInRestart",
    "URLRequestTest.InterceptRedirect",
    "URLRequestTest.InterceptServerError",
    "URLRequestTestFTP.*",

    // Tests that are currently not working:

    // Temporarily disabled because they needs user input (login dialog).
    "URLRequestTestHTTP.BasicAuth",
    "URLRequestTestHTTP.BasicAuthWithCookies",

    // HTTPS tests temporarily disabled due to the certificate error dialog.
    // TODO(tommi): The tests currently fail though, so need to fix.
    "HTTPSRequestTest.HTTPSMismatchedTest",
    "HTTPSRequestTest.HTTPSExpiredTest",

    // Tests chrome's network stack's cache (might not apply to CF).
    "URLRequestTestHTTP.VaryHeader",

    // I suspect we can only get this one to work (if at all) on IE8 and
    // later by using the new INTERNET_OPTION_SUPPRESS_BEHAVIOR flags
    // See http://msdn.microsoft.com/en-us/library/aa385328(VS.85).aspx
    "URLRequestTest.DoNotSaveCookies",
  };

  std::string filter("-");  // All following filters will be negative.
  for (int i = 0; i < arraysize(disabled_tests); ++i) {
    if (i > 0)
      filter += ":";
    filter += disabled_tests[i];
  }

  ::testing::FLAGS_gtest_filter = filter;
}

int main(int argc, char** argv) {
  DialogWatchdog watchdog;
  // See url_request_unittest.cc for these credentials.
  SupplyProxyCredentials credentials("user", "secret");
  watchdog.AddObserver(&credentials);
  testing::InitGoogleTest(&argc, argv);
  FilterDisabledTests();
  PluginService::EnableChromePlugins(false);
  CFUrlRequestUnittestRunner test_suite(argc, argv);
  test_suite.RunMainUIThread();
  return 0;
}
