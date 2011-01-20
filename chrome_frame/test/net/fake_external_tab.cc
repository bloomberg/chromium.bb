// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/net/fake_external_tab.h"

#include <atlbase.h>
#include <atlcom.h>
#include <exdisp.h>

#include "app/app_paths.h"
#include "app/win/scoped_com_initializer.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/i18n/icu_util.h"
#include "base/path_service.h"
#include "base/scoped_comptr_win.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/win/scoped_handle.h"
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/net/test_automation_resource_message_filter.h"
#include "chrome_frame/test/simulate_input.h"
#include "chrome_frame/test/win_event_receiver.h"
#include "chrome_frame/utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace {

// A special command line switch to allow developers to manually launch the
// browser and debug CF inside the browser.
const char kManualBrowserLaunch[] = "manual-browser";

// Pops up a message box after the test environment has been set up
// and before tearing it down.  Useful for when debugging tests and not
// the test environment that's been set up.
const char kPromptAfterSetup[] = "prompt-after-setup";

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

// Uses the IAccessible interface for the window to set the focus.
// This can be useful when you don't have control over the thread that
// owns the window.
// NOTE: this depends on oleacc.lib which the net tests already depend on
// but other unit tests don't depend on oleacc so we can't just add the method
// directly into chrome_frame_test_utils.cc (without adding a
// #pragma comment(lib, "oleacc.lib")).
bool SetFocusToAccessibleWindow(HWND hwnd) {
  bool ret = false;
  ScopedComPtr<IAccessible> acc;
  AccessibleObjectFromWindow(hwnd, OBJID_WINDOW, IID_IAccessible,
      reinterpret_cast<void**>(acc.Receive()));
  if (acc) {
    VARIANT self = { VT_I4 };
    self.lVal = CHILDID_SELF;
    ret = SUCCEEDED(acc->accSelect(SELFLAG_TAKEFOCUS, self));
  }
  return ret;
}

}  // namespace


class SupplyProxyCredentials : public WindowObserver {
 public:
  SupplyProxyCredentials(const char* username, const char* password);

 protected:
  struct DialogProps {
    HWND username_;
    HWND password_;
  };

  virtual void OnWindowOpen(HWND hwnd);
  virtual void OnWindowClose(HWND hwnd);
  static BOOL CALLBACK EnumChildren(HWND hwnd, LPARAM param);

 protected:
  std::string username_;
  std::string password_;
};


SupplyProxyCredentials::SupplyProxyCredentials(const char* username,
                                               const char* password)
    : username_(username), password_(password) {
}

void SupplyProxyCredentials::OnWindowClose(HWND hwnd) { }

void SupplyProxyCredentials::OnWindowOpen(HWND hwnd) {
  DialogProps props = {0};
  ::EnumChildWindows(hwnd, EnumChildren, reinterpret_cast<LPARAM>(&props));
  DCHECK(::IsWindow(props.username_));
  DCHECK(::IsWindow(props.password_));

  // We can't use SetWindowText to set the username/password, so simulate
  // keyboard input instead.
  simulate_input::ForceSetForegroundWindow(hwnd);
  CHECK(SetFocusToAccessibleWindow(props.username_));
  simulate_input::SendStringA(username_.c_str());
  Sleep(100);

  simulate_input::SendCharA(VK_TAB, simulate_input::NONE);
  Sleep(100);
  simulate_input::SendStringA(password_.c_str());

  Sleep(100);
  simulate_input::SendCharA(VK_RETURN, simulate_input::NONE);
}

// static
BOOL SupplyProxyCredentials::EnumChildren(HWND hwnd, LPARAM param) {
  if (!::IsWindowVisible(hwnd))
    return TRUE;  // Ignore but continue to enumerate.

  DialogProps* props = reinterpret_cast<DialogProps*>(param);

  char class_name[MAX_PATH] = {0};
  ::GetClassNameA(hwnd, class_name, arraysize(class_name));
  if (lstrcmpiA(class_name, "Edit") == 0) {
    if (props->username_ == NULL || props->username_ == hwnd) {
      props->username_ = hwnd;
    } else if (props->password_ == NULL) {
      props->password_ = hwnd;
    }
  } else {
    EnumChildWindows(hwnd, EnumChildren, param);
  }

  return TRUE;
}

FakeExternalTab::FakeExternalTab() {
  user_data_dir_ = chrome_frame_test::GetProfilePathForIE();
  PathService::Get(chrome::DIR_USER_DATA, &overridden_user_dir_);
  PathService::Override(chrome::DIR_USER_DATA, user_data_dir_);
  process_singleton_.reset(new ProcessSingleton(user_data_dir_));
}

FakeExternalTab::~FakeExternalTab() {
  if (!overridden_user_dir_.empty()) {
    PathService::Override(chrome::DIR_USER_DATA, overridden_user_dir_);
  }
}

void FakeExternalTab::Initialize() {
  DCHECK(g_browser_process == NULL);
  ui::SystemMonitor system_monitor;

  // The gears plugin causes the PluginRequestInterceptor to kick in and it
  // will cause problems when it tries to intercept URL requests.
  PathService::Override(chrome::FILE_GEARS_PLUGIN, FilePath());

  icu_util::Initialize();

  chrome::RegisterPathProvider();
  app::RegisterPathProvider();
  ui::RegisterPathProvider();

  // Load Chrome.dll as our resource dll.
  FilePath dll;
  PathService::Get(base::DIR_MODULE, &dll);
  dll = dll.Append(chrome::kBrowserResourcesDll);
  HMODULE res_mod = ::LoadLibraryExW(dll.value().c_str(),
      NULL, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
  DCHECK(res_mod);
  _AtlBaseModule.SetResourceInstance(res_mod);

  ResourceBundle::InitSharedInstance("en-US");

  CommandLine* cmd = CommandLine::ForCurrentProcess();
  cmd->AppendSwitch(switches::kDisableWebResources);
  cmd->AppendSwitch(switches::kSingleProcess);

  browser_process_.reset(new BrowserProcessImpl(*cmd));
  // BrowserProcessImpl's constructor should set g_browser_process.
  DCHECK(g_browser_process);
  // Set the app locale and create the child threads.
  g_browser_process->SetApplicationLocale("en-US");
  g_browser_process->db_thread();
  g_browser_process->file_thread();
  g_browser_process->io_thread();

  RenderProcessHost::set_run_renderer_in_process(true);

  FilePath profile_path(ProfileManager::GetDefaultProfileDir(user_data()));
  Profile* profile = g_browser_process->profile_manager()->GetProfile(
      profile_path, false);
  PrefService* prefs = profile->GetPrefs();
  DCHECK(prefs != NULL);
  WebCacheManager::RegisterPrefs(prefs);

  PrefService* local_state = browser_process_->local_state();
  local_state->RegisterStringPref(prefs::kApplicationLocale, "");
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
  main_thread_.reset(new BrowserThread(BrowserThread::UI,
                                       MessageLoop::current()));
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  fake_chrome_.Initialize();
  pss_subclass_.reset(new ProcessSingletonSubclass(this));
  EXPECT_TRUE(pss_subclass_->Subclass(fake_chrome_.user_data()));
  StartChromeFrameInHostBrowser();
}

CFUrlRequestUnittestRunner::~CFUrlRequestUnittestRunner() {
  fake_chrome_.Shutdown();
}

void CFUrlRequestUnittestRunner::StartChromeFrameInHostBrowser() {
  if (!ShouldLaunchBrowser())
    return;

  app::win::ScopedCOMInitializer com;
  chrome_frame_test::CloseAllIEWindows();

  test_http_server_.reset(new test_server::SimpleWebServer(kTestServerPort));
  test_http_server_->AddResponse(&chrome_frame_html_);
  std::wstring url(base::StringPrintf(L"http://localhost:%i/chrome_frame",
                                      kTestServerPort).c_str());

  // Launch IE.  This launches IE correctly on Vista too.
  base::win::ScopedHandle ie_process(chrome_frame_test::LaunchIE(url));
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
    app::win::ScopedCOMInitializer com;
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
  CFUrlRequestUnittestRunner::InitializeLogging();
  base::Time::EnableHighResolutionTimer(true);

  SuppressErrorDialogs();
  base::debug::SetSuppressDebugUI(true);
#if !defined(PURIFY)
  logging::SetLogAssertHandler(UnitTestAssertHandler);
#endif  // !defined(PURIFY)

  // Next, do some initialization for NetTestSuite.
  NetTestSuite::InitializeTestThread();
}

void CFUrlRequestUnittestRunner::Shutdown() {
  DCHECK(::GetCurrentThreadId() == test_thread_id_);
  NetTestSuite::Shutdown();
  OleUninitialize();
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
  base::PlatformThread::SetName("CFUrlRequestUnittestRunner");
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
  logging::InitLogging(
      log_filename.value().c_str(),
      logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
      logging::LOCK_LOG_FILE,
      logging::DELETE_OLD_LOG_FILE,
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
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
    "HTTPSRequestTest.ClientAuthTest",

    // Tests chrome's network stack's cache (might not apply to CF).
    "URLRequestTestHTTP.VaryHeader",

    // I suspect we can only get this one to work (if at all) on IE8 and
    // later by using the new INTERNET_OPTION_SUPPRESS_BEHAVIOR flags
    // See http://msdn.microsoft.com/en-us/library/aa385328(VS.85).aspx
    "URLRequestTest.DoNotSaveCookies",

    // TODO(ananta): This test has been consistently failing. Disabling it for
    // now.
    "URLRequestTestHTTP.GetTest_NoCache",

    // These tests have been disabled as the Chrome cookie policies don't make
    // sense or have not been implemented for the host network stack.
    "URLRequestTest.DoNotSaveCookies_ViaPolicy",
    "URLRequestTest.DoNotSendCookies_ViaPolicy",
    "URLRequestTest.DoNotSaveCookies_ViaPolicy_Async",
    "URLRequestTest.CookiePolicy_ForceSession",
    "URLRequestTest.DoNotSendCookies",
    "URLRequestTest.DoNotSendCookies_ViaPolicy_Async",
    "URLRequestTest.CancelTest_During_OnGetCookies",
    "URLRequestTest.CancelTest_During_OnSetCookie",

    // These tests are disabled as the rely on functionality provided by
    // Chrome's HTTP stack like the ability to set the proxy for a URL, etc.
    "URLRequestTestHTTP.ProxyTunnelRedirectTest",
    "URLRequestTestHTTP.UnexpectedServerAuthTest",
  };

  std::string filter("-");  // All following filters will be negative.
  for (int i = 0; i < arraysize(disabled_tests); ++i) {
    if (i > 0)
      filter += ":";
    filter += disabled_tests[i];
  }

  ::testing::FLAGS_gtest_filter = filter;
}

// We need a module since some of the accessibility code that gets pulled
// in here uses ATL.
class ObligatoryModule: public CAtlExeModuleT<ObligatoryModule> {
 public:
  static HRESULT InitializeCom() {
    return OleInitialize(NULL);
  }

  static void UninitializeCom() {
    OleUninitialize();
  }
};

ObligatoryModule g_obligatory_atl_module;

int main(int argc, char** argv) {
  if (chrome_frame_test::GetInstalledIEVersion() == IE_9) {
    // Adding this here as the command line and the logging stuff gets
    // initialized in the NetTestSuite constructor. Did not want to break that.
    base::AtExitManager at_exit_manager_;
    CommandLine::Init(argc, argv);
    CFUrlRequestUnittestRunner::InitializeLogging();
    LOG(INFO) << "Not running ChromeFrame net tests on IE9";
    return 0;
  }
  WindowWatchdog watchdog;
  // See url_request_unittest.cc for these credentials.
  SupplyProxyCredentials credentials("user", "secret");
  watchdog.AddObserver(&credentials, "Windows Security", "");
  testing::InitGoogleTest(&argc, argv);
  FilterDisabledTests();
  PluginService::EnableChromePlugins(false);
  CFUrlRequestUnittestRunner test_suite(argc, argv);
  test_suite.RunMainUIThread();
  return 0;
}
