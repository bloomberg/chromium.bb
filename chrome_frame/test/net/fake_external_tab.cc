// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/net/fake_external_tab.h"

#include <atlbase.h>
#include <atlcom.h>
#include <exdisp.h>
#include <Winsock2.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/icu_util.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/prefs/json_pref_store.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/system_monitor/system_monitor.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"
#include "chrome/app/chrome_main_delegate.h"
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/logging/win/file_logger.h"
#include "chrome/test/logging/win/log_file_printer.h"
#include "chrome/test/logging/win/test_log_collector.h"
#include "chrome_frame/crash_server_init.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/ie_configurator.h"
#include "chrome_frame/test/net/test_automation_resource_message_filter.h"
#include "chrome_frame/test/simulate_input.h"
#include "chrome_frame/test/win_event_receiver.h"
#include "chrome_frame/utils.h"
#include "content/public/app/content_main.h"
#include "content/public/app/startup_helper_win.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_paths.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_test_util.h"
#include "sandbox/win/src/sandbox_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_bundle_win.h"
#include "ui/base/ui_base_paths.h"

using content::BrowserThread;

namespace {

// We must store this globally so that our main delegate can set it.
static CFUrlRequestUnittestRunner* g_test_suite = NULL;

// Copied here for access by CreateBrowserMainParts and InitGoogleTest.
static int g_argc = 0;
static char** g_argv = NULL;

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

// Uses the IAccessible interface for the window to set the focus.
// This can be useful when you don't have control over the thread that
// owns the window.
// NOTE: this depends on oleacc.lib which the net tests already depend on
// but other unit tests don't depend on oleacc so we can't just add the method
// directly into chrome_frame_test_utils.cc (without adding a
// #pragma comment(lib, "oleacc.lib")).
bool SetFocusToAccessibleWindow(HWND hwnd) {
  bool ret = false;
  base::win::ScopedComPtr<IAccessible> acc;
  AccessibleObjectFromWindow(hwnd, OBJID_WINDOW, IID_IAccessible,
      reinterpret_cast<void**>(acc.Receive()));
  if (acc) {
    VARIANT self = { VT_I4 };
    self.lVal = CHILDID_SELF;
    ret = SUCCEEDED(acc->accSelect(SELFLAG_TAKEFOCUS, self));
  }
  return ret;
}

class FakeContentBrowserClient : public chrome::ChromeContentBrowserClient {
 public:
  virtual ~FakeContentBrowserClient() {}

  virtual content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) OVERRIDE;
};

base::LazyInstance<chrome::ChromeContentClient>
    g_chrome_content_client = LAZY_INSTANCE_INITIALIZER;

// Override the default ContentBrowserClient to let Chrome participate in
// content logic.  Must be done before any tabs are created.
base::LazyInstance<FakeContentBrowserClient>
    g_browser_client = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<chrome::ChromeContentRendererClient>
    g_renderer_client = LAZY_INSTANCE_INITIALIZER;

class FakeMainDelegate : public content::ContentMainDelegate {
 public:
  virtual ~FakeMainDelegate() {}

  virtual bool BasicStartupComplete(int* exit_code) OVERRIDE {
    logging_win::InstallTestLogCollector(
        testing::UnitTest::GetInstance());

    content::SetContentClient(&g_chrome_content_client.Get());
    content::GetContentClient()->set_renderer_for_testing(
        &g_renderer_client.Get());
    return false;
  }

  // Override the default ContentBrowserClient to let Chrome participate in
  // content logic.  We use a subclass of Chrome's implementation,
  // FakeContentBrowserClient, to override CreateBrowserMainParts.  Must
  // be done before any tabs are created.
  virtual content::ContentBrowserClient* CreateContentBrowserClient() OVERRIDE {
    return &g_browser_client.Get();
  };
};

void FilterDisabledTests() {
  if (::testing::FLAGS_gtest_filter.length() &&
      ::testing::FLAGS_gtest_filter != "*") {
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

    // ChromeFrame does not support load timing.
    "URLRequestTestHTTP.BasicAuthLoadTiming",
    "URLRequestTestHTTP.GetTestLoadTiming",
    "URLRequestTestHTTP.RedirectLoadTiming",

    // HTTPS tests temporarily disabled due to the certificate error dialog.
    // TODO(tommi): The tests currently fail though, so need to fix.
    "HTTPSRequestTest.HTTPSMismatchedTest",
    "HTTPSRequestTest.HTTPSExpiredTest",
    "HTTPSRequestTest.ClientAuthTest",

    // More HTTPS tests failing due to certificate dialogs.
    // http://crbug.com/102991
    "URLRequestTestHTTP.HTTPSToHTTPRedirectNoRefererTest",
    "HTTPSRequestTest.HTTPSGetTest",

    // Tests chrome's network stack's cache (might not apply to CF).
    "URLRequestTestHTTP.VaryHeader",
    "URLRequestTestHTTP.GetZippedTest",

    // Tests that requests can be blocked asynchronously in states
    // OnBeforeURLRequest, OnBeforeSendHeaders and OnHeadersReceived. At least
    // the second state is not supported by CF.
    "URLRequestTestHTTP.NetworkDelegateBlockAsynchronously",

    // Tests for cancelling requests in states OnBeforeSendHeaders and
    // OnHeadersReceived, which do not seem supported by CF.
    "URLRequestTestHTTP.NetworkDelegateCancelRequestSynchronously2",
    "URLRequestTestHTTP.NetworkDelegateCancelRequestSynchronously3",
    "URLRequestTestHTTP.NetworkDelegateCancelRequestAsynchronously2",
    "URLRequestTestHTTP.NetworkDelegateCancelRequestAsynchronously3",

    // Tests that requests can be cancelled while blocking in
    // OnBeforeSendHeaders state. But this state is not supported by CF.
    "URLRequestTestHTTP.NetworkDelegateCancelWhileWaiting2",

    // Tests that requests can be cancelled while blocking in
    // OnHeadersRecevied state. At first glance, this state does not appear to
    // be supported by CF.
    "URLRequestTestHTTP.NetworkDelegateCancelWhileWaiting3",

    // Tests that requests can be cancelled while blocking in OnAuthRequired
    // state. At first glance, this state does not appear to be supported by CF.
    // IE displays a credentials prompt during this test - I (erikwright)
    // believe that, from Chrome's point of view this is not a state change. In
    // any case, I also believe that we do not have support for handling the
    // credentials dialog during tests.
    "URLRequestTestHTTP.NetworkDelegateCancelWhileWaiting4",

    // I suspect we can only get this one to work (if at all) on IE8 and
    // later by using the new INTERNET_OPTION_SUPPRESS_BEHAVIOR flags
    // See http://msdn.microsoft.com/en-us/library/aa385328(VS.85).aspx
    "URLRequestTest.DoNotSaveCookies",
    "URLRequestTest.DelayedCookieCallback",

    // TODO(ananta): This test has been consistently failing. Disabling it for
    // now.
    "URLRequestTestHTTP.GetTest_NoCache",

    // These tests use HTTPS, and IE's trust store does not have the test
    // certs. So these tests time out waiting for user input. The
    // functionality they test (HTTP Strict Transport Security) does not
    // work in Chrome Frame anyway.
    "URLRequestTestHTTP.ProcessSTS",
    "URLRequestTestHTTP.ProcessSTSOnce",

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

    // These tests are disabled as they rely on functionality provided by
    // Chrome's HTTP stack like the ability to set the proxy for a URL, etc.
    "URLRequestTestHTTP.ProxyTunnelRedirectTest",
    "URLRequestTestHTTP.NetworkDelegateTunnelConnectionFailed",
    "URLRequestTestHTTP.UnexpectedServerAuthTest",

    // These tests are disabled as they expect an empty UA to be echoed back
    // from the server which is not the case in ChromeFrame.
    "URLRequestTestHTTP.DefaultUserAgent",
    "URLRequestTestHTTP.EmptyHttpUserAgentSettings",
    // This test modifies the UploadData object after it has been marshaled to
    // ChromeFrame. We don't support this.
    "URLRequestTestHTTP.TestPostChunkedDataAfterStart",

    // Do not work in CF, it may well be that IE is unconditionally
    // adding Accept-Encoding header by default to outgoing requests.
    "URLRequestTestHTTP.DefaultAcceptEncoding",
    "URLRequestTestHTTP.OverrideAcceptEncoding",

    // Not supported in ChromeFrame as we use IE's network stack.
    "URLRequestTest.NetworkDelegateProxyError",
    "URLRequestTest.AcceptClockSkewCookieWithWrongDateTimezone",

    // URLRequestAutomationJob needs to support NeedsAuth.
    // http://crbug.com/98446
    "URLRequestTestHTTP.NetworkDelegateOnAuthRequiredSyncNoAction",
    "URLRequestTestHTTP.NetworkDelegateOnAuthRequiredSyncSetAuth",
    "URLRequestTestHTTP.NetworkDelegateOnAuthRequiredSyncCancel",
    "URLRequestTestHTTP.NetworkDelegateOnAuthRequiredAsyncNoAction",
    "URLRequestTestHTTP.NetworkDelegateOnAuthRequiredAsyncSetAuth",
    "URLRequestTestHTTP.NetworkDelegateOnAuthRequiredAsyncCancel",

    // Flaky on the tryservers, http://crbug.com/103097
    "URLRequestTestHTTP.MultipleRedirectTest",
    "URLRequestTestHTTP.NetworkDelegateRedirectRequest",

    // These tests are unsupported in CF.
    "HTTPSRequestTest.HTTPSPreloadedHSTSTest",
    "HTTPSRequestTest.HTTPSErrorsNoClobberTSSTest",
    "HTTPSRequestTest.HSTSPreservesPosts",
    "HTTPSRequestTest.ResumeTest",
    "HTTPSRequestTest.SSLSessionCacheShardTest",
    "HTTPSRequestTest.SSLSessionCacheShardTest",
    "HTTPSRequestTest.SSLv3Fallback",
    "HTTPSRequestTest.TLSv1Fallback",
    "HTTPSOCSPTest.*",
    "HTTPSEVCRLSetTest.*",
    "HTTPSCRLSetTest.*"
  };

  const char* ie9_disabled_tests[] = {
    // These always hang on Joi's box with IE9, http://crbug.com/105435.
    // Several other tests, e.g. URLRequestTestHTTP.CancelTest2, 3 and
    // 5, often hang but not always.
    "URLRequestTestHTTP.NetworkDelegateRedirectRequestPost",
    "URLRequestTestHTTP.GetTest",
    "HTTPSRequestTest.HTTPSPreloadedHSTSTest",
    // This always hangs on erikwright's box with IE9.
    "URLRequestTestHTTP.Redirect302Tests"
  };

  std::string filter("-");  // All following filters will be negative.
  for (int i = 0; i < arraysize(disabled_tests); ++i) {
    if (i > 0)
      filter += ":";

    // If the rule has the form TestSuite.TestCase, also filter out
    // TestSuite.FLAKY_TestCase . This way the exclusion rules above
    // don't need to be updated when a test is marked flaky.
    base::StringPiece test_name(disabled_tests[i]);
    size_t dot_index = test_name.find('.');
    if (dot_index != base::StringPiece::npos &&
        dot_index + 1 < test_name.size()) {
      test_name.substr(0, dot_index).AppendToString(&filter);
      filter += ".FLAKY_";
      test_name.substr(dot_index + 1).AppendToString(&filter);
      filter += ":";
    }
    filter += disabled_tests[i];
  }

  if (chrome_frame_test::GetInstalledIEVersion() >= IE_9) {
    for (int i = 0; i < arraysize(ie9_disabled_tests); ++i) {
      filter += ":";
      filter += ie9_disabled_tests[i];
    }
  }

  ::testing::FLAGS_gtest_filter = filter;
}

}  // namespace

// Same as BrowserProcessImpl, but uses custom profile manager.
class FakeBrowserProcessImpl : public BrowserProcessImpl {
 public:
  FakeBrowserProcessImpl(base::SequencedTaskRunner* local_state_task_runner,
                         const CommandLine& command_line)
      : BrowserProcessImpl(local_state_task_runner, command_line) {
    profiles_dir_.CreateUniqueTempDir();
  }

  virtual ~FakeBrowserProcessImpl() {}

  virtual ProfileManager* profile_manager() OVERRIDE {
    if (!profile_manager_.get()) {
      profile_manager_.reset(
          new ProfileManagerWithoutInit(profiles_dir_.path()));
    }
    return profile_manager_.get();
  }

  virtual MetricsService* metrics_service() OVERRIDE {
    return NULL;
  }

  void DestroyProfileManager() {
    profile_manager_.reset();
  }

 private:
  base::ScopedTempDir profiles_dir_;
  scoped_ptr<ProfileManager> profile_manager_;
};

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

  if (file_util::PathExists(user_data_dir_)) {
    VLOG(1) << __FUNCTION__ << " deleting IE Profile user data directory "
            << user_data_dir_.value();
    bool deleted = file_util::Delete(user_data_dir_, true);
    LOG_IF(ERROR, !deleted) << "Failed to delete user data directory directory "
                            << user_data_dir_.value();
  }

  PathService::Get(chrome::DIR_USER_DATA, &overridden_user_dir_);
  PathService::Override(chrome::DIR_USER_DATA, user_data_dir_);
}

FakeExternalTab::~FakeExternalTab() {
  if (!overridden_user_dir_.empty()) {
    PathService::Override(chrome::DIR_USER_DATA, overridden_user_dir_);
  }
}

void FakeExternalTab::Initialize() {
  DCHECK(g_browser_process == NULL);

  TestTimeouts::Initialize();

  // Load Chrome.dll as our resource dll.
  base::FilePath dll;
  PathService::Get(base::DIR_MODULE, &dll);
  dll = dll.Append(chrome::kBrowserResourcesDll);
  HMODULE res_mod = ::LoadLibraryExW(dll.value().c_str(),
      NULL, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
  DCHECK(res_mod);
  _AtlBaseModule.SetResourceInstance(res_mod);

  // Point the ResourceBundle at chrome.dll.
  ui::SetResourcesDataDLL(_AtlBaseModule.GetResourceInstance());

  ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);

  CommandLine* cmd = CommandLine::ForCurrentProcess();
  cmd->AppendSwitch(switches::kDisableWebResources);
  cmd->AppendSwitch(switches::kSingleProcess);

  base::FilePath local_state_path;
  CHECK(PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path));
  scoped_refptr<base::SequencedTaskRunner> local_state_task_runner =
      JsonPrefStore::GetTaskRunnerForFile(local_state_path,
                                          BrowserThread::GetBlockingPool());
  browser_process_.reset(new FakeBrowserProcessImpl(local_state_task_runner,
                                                    *cmd));
  // BrowserProcessImpl's constructor should set g_browser_process.
  DCHECK(g_browser_process);
  g_browser_process->SetApplicationLocale("en-US");

  content::RenderProcessHost::SetRunRendererInProcess(true);

  // TODO(joi): Registration should be done up front via browser_prefs.cc
  scoped_refptr<PrefRegistrySimple> registry = static_cast<PrefRegistrySimple*>(
      browser_process_->local_state()->DeprecatedGetPrefRegistry());
  if (!browser_process_->local_state()->FindPreference(
          prefs::kMetricsReportingEnabled)) {
    registry->RegisterBooleanPref(prefs::kMetricsReportingEnabled, false);
  }
}

void FakeExternalTab::InitializePostThreadsCreated() {
  base::FilePath profile_path(
      ProfileManager::GetDefaultProfileDir(user_data()));
  Profile* profile =
      g_browser_process->profile_manager()->GetProfile(profile_path);
}

void FakeExternalTab::Shutdown() {
  browser_process_.reset();
  g_browser_process = NULL;

  ResourceBundle::CleanupSharedInstance();
}

FakeBrowserProcessImpl* FakeExternalTab::browser_process() const {
  return browser_process_.get();
}

CFUrlRequestUnittestRunner::CFUrlRequestUnittestRunner(int argc, char** argv)
    : NetTestSuite(argc, argv, false),
      chrome_frame_html_("/chrome_frame", kChromeFrameHtml),
      registrar_(chrome_frame_test::GetTestBedType()),
      test_result_(0),
      launch_browser_(
          !CommandLine::ForCurrentProcess()->HasSwitch(kManualBrowserLaunch)),
      prompt_after_setup_(
          CommandLine::ForCurrentProcess()->HasSwitch(kPromptAfterSetup)),
      tests_ran_(false) {
}

CFUrlRequestUnittestRunner::~CFUrlRequestUnittestRunner() {
}

void CFUrlRequestUnittestRunner::StartChromeFrameInHostBrowser() {
  if (!launch_browser_)
    return;

  chrome_frame_test::CloseAllIEWindows();

  // Tweak IE settings to make it amenable to testing before launching it.
  ie_configurator_.reset(chrome_frame_test::CreateConfigurator());
  if (ie_configurator_.get() != NULL) {
    ie_configurator_->Initialize();
    ie_configurator_->ApplySettings();
  }

  test_http_server_.reset(new test_server::SimpleWebServer("127.0.0.1",
                                                           kTestServerPort));
  test_http_server_->AddResponse(&chrome_frame_html_);
  std::wstring url(base::StringPrintf(L"http://localhost:%i/chrome_frame",
                                      kTestServerPort));

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
  if (launch_browser_)
    chrome_frame_test::CloseAllIEWindows();
}

void CFUrlRequestUnittestRunner::OnIEShutdownFailure() {
  LOG(ERROR) << "Failed to shutdown IE and npchrome_frame cleanly after test "
                "execution.";

  if (ie_configurator_.get() != NULL)
    ie_configurator_->RevertSettings();

  StopFileLogger(true);
  ::ExitProcess(0);
}

// Override virtual void Initialize to not call icu initialize.
void CFUrlRequestUnittestRunner::Initialize() {
  DCHECK(::GetCurrentThreadId() == test_thread_id_);

  // Start by replicating some of the steps that would otherwise be
  // done by TestSuite::Initialize.  We can't call the base class
  // directly because it will attempt to initialize some things such as
  // ICU that have already been initialized for this process.
  CFUrlRequestUnittestRunner::InitializeLogging();

  SuppressErrorDialogs();
  base::debug::SetSuppressDebugUI(true);
  logging::SetLogAssertHandler(UnitTestAssertHandler);

  // Next, do some initialization for NetTestSuite.
  NetTestSuite::InitializeTestThreadNoNetworkChangeNotifier();

  // Finally, override the host used by the HTTP tests. See
  // http://crbug.com/114369 .
  OverrideHttpHost();
}

void CFUrlRequestUnittestRunner::Shutdown() {
  DCHECK(::GetCurrentThreadId() == test_thread_id_);
  NetTestSuite::Shutdown();
  OleUninitialize();
}

void CFUrlRequestUnittestRunner::OnInitialTabLoaded() {
  test_http_server_.reset();
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&CFUrlRequestUnittestRunner::CancelInitializationTimeout,
                 base::Unretained(this)));
  StartTests();
}

void CFUrlRequestUnittestRunner::OnProviderDestroyed() {
  if (tests_ran_) {
    StopFileLogger(false);

    if (ie_configurator_.get() != NULL)
      ie_configurator_->RevertSettings();

    if (crash_service_)
      base::KillProcess(crash_service_, 0, false);

    ::ExitProcess(test_result());
  } else {
    DLOG(ERROR) << "Automation Provider shutting down before test execution "
                   "has completed.";
  }
}

void CFUrlRequestUnittestRunner::StartTests() {
  if (prompt_after_setup_)
    MessageBoxA(NULL, "click ok to run", "", MB_OK);

  DCHECK_EQ(test_thread_.IsValid(), false);
  StopFileLogger(false);
  test_thread_.Set(::CreateThread(NULL, 0, RunAllUnittests, this, 0,
                                  &test_thread_id_));
  DCHECK(test_thread_.IsValid());
}

// static
DWORD CFUrlRequestUnittestRunner::RunAllUnittests(void* param) {
  base::PlatformThread::SetName("CFUrlRequestUnittestRunner");
  CFUrlRequestUnittestRunner* me =
      reinterpret_cast<CFUrlRequestUnittestRunner*>(param);
  me->test_result_ = me->Run();
  me->tests_ran_ = true;
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&CFUrlRequestUnittestRunner::TakeDownBrowser,
                 base::Unretained(me)));
  return 0;
}

void CFUrlRequestUnittestRunner::TakeDownBrowser() {
  if (prompt_after_setup_)
    MessageBoxA(NULL, "click ok to exit", "", MB_OK);

  // Start capturing logs from npchrome_frame and the in-process Chrome to help
  // diagnose failures in IE shutdown. This will be Stopped in either
  // OnIEShutdownFailure or OnProviderDestroyed.
  StartFileLogger();

  // AddRef to ensure that IE going away does not trigger the Chrome shutdown
  // process.
  // IE shutting down will, however, trigger the automation channel to shut
  // down, at which time we will exit the process (see OnProviderDestroyed).
  g_browser_process->AddRefModule();
  ShutDownHostBrowser();

  // In case IE is somehow hung, make sure we don't sit around until a try-bot
  // kills us. OnIEShutdownFailure will log and exit with an error.
  BrowserThread::PostDelayedTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&CFUrlRequestUnittestRunner::OnIEShutdownFailure,
                 base::Unretained(this)),
      TestTimeouts::action_max_timeout());
}

void CFUrlRequestUnittestRunner::InitializeLogging() {
  base::FilePath exe;
  PathService::Get(base::FILE_EXE, &exe);
  base::FilePath log_filename = exe.ReplaceExtension(FILE_PATH_LITERAL("log"));
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

void CFUrlRequestUnittestRunner::CancelInitializationTimeout() {
  timeout_closure_.Cancel();
}

void CFUrlRequestUnittestRunner::StartInitializationTimeout() {
  timeout_closure_.Reset(
      base::Bind(&CFUrlRequestUnittestRunner::OnInitializationTimeout,
                 base::Unretained(this)));
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      timeout_closure_.callback(),
      TestTimeouts::action_max_timeout());
}

void CFUrlRequestUnittestRunner::OnInitializationTimeout() {
  LOG(ERROR) << "Failed to start Chrome Frame in the host browser.";

  base::FilePath snapshot;
  if (ui_test_utils::SaveScreenSnapshotToDesktop(&snapshot))
    LOG(ERROR) << "Screen snapshot saved to " << snapshot.value();

  StopFileLogger(true);

  if (launch_browser_)
    chrome_frame_test::CloseAllIEWindows();

  if (ie_configurator_.get() != NULL)
    ie_configurator_->RevertSettings();

  if (crash_service_)
    base::KillProcess(crash_service_, 0, false);

  ::ExitProcess(1);
}

void CFUrlRequestUnittestRunner::OverrideHttpHost() {
  override_http_host_.reset(
      new net::ScopedCustomUrlRequestTestHttpHost(
          chrome_frame_test::GetLocalIPv4Address()));
}

void CFUrlRequestUnittestRunner::PreEarlyInitialization() {
  testing::InitGoogleTest(&g_argc, g_argv);
  FilterDisabledTests();
  StartFileLogger();
}

int CFUrlRequestUnittestRunner::PreCreateThreads() {
  fake_chrome_.reset(new FakeExternalTab());
  fake_chrome_->Initialize();
  fake_chrome_->browser_process()->PreCreateThreads();
  ProcessSingleton::NotificationCallback callback(
      base::Bind(
          &CFUrlRequestUnittestRunner::ProcessSingletonNotificationCallback,
          base::Unretained(this)));
  process_singleton_.reset(new ProcessSingleton(fake_chrome_->user_data(),
                                                callback));
  process_singleton_->Lock(NULL);
  return 0;
}

bool CFUrlRequestUnittestRunner::ProcessSingletonNotificationCallback(
    const CommandLine& command_line, const base::FilePath& current_directory) {
  std::string channel_id = command_line.GetSwitchValueASCII(
      switches::kAutomationClientChannelID);
  EXPECT_FALSE(channel_id.empty());

  Profile* profile = g_browser_process->profile_manager()->GetLastUsedProfile(
      fake_chrome_->user_data());

  AutomationProviderList* list = g_browser_process->GetAutomationProviderList();
  DCHECK(list);
  list->AddProvider(
      TestAutomationProvider::NewAutomationProvider(profile, channel_id, this));
  return true;
}

void CFUrlRequestUnittestRunner::PreMainMessageLoopRun() {
  fake_chrome_->InitializePostThreadsCreated();
  // Call Create directly instead of NotifyOtherProcessOrCreate as failure is
  // prefered to notifying another process here.
  if (!process_singleton_->Create()) {
    LOG(FATAL) << "Failed to start up ProcessSingleton. Is another test "
               << "executable or Chrome Frame running?";
    if (crash_service_)
      base::KillProcess(crash_service_, 0, false);
    ::ExitProcess(1);
  }

  StartChromeFrameInHostBrowser();
}

bool CFUrlRequestUnittestRunner::MainMessageLoopRun(int* result_code) {
  DCHECK(MessageLoop::current());
  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);

  // We need to allow IO on the main thread for these tests.
  base::ThreadRestrictions::SetIOAllowed(true);
  process_singleton_->Unlock();
  StartInitializationTimeout();
  return false;
}

void CFUrlRequestUnittestRunner::PostMainMessageLoopRun() {
  process_singleton_->Cleanup();
  fake_chrome_->browser_process()->StartTearDown();

  // Must do this separately as the mock profile_manager_ is not the
  // same member as BrowserProcessImpl::StartTearDown resets.
  fake_chrome_->browser_process()->DestroyProfileManager();

  if (crash_service_)
    base::KillProcess(crash_service_, 0, false);

  base::KillProcesses(chrome_frame_test::kIEImageName, 0, NULL);
  base::KillProcesses(chrome_frame_test::kIEBrokerImageName, 0, NULL);
}

void CFUrlRequestUnittestRunner::PostDestroyThreads() {
  process_singleton_.reset();
  fake_chrome_->browser_process()->PostDestroyThreads();
  fake_chrome_->Shutdown();
  fake_chrome_.reset();

#ifndef NDEBUG
  // Avoid CRT cleanup in debug test runs to ensure that webkit ASSERTs which
  // check if globals are created and destroyed on the same thread don't fire.
  // Webkit global objects are created on the inproc renderer thread.
  ::ExitProcess(test_result());
#endif
}

void CFUrlRequestUnittestRunner::StartFileLogger() {
  if (file_util::CreateTemporaryFile(&log_file_)) {
    file_logger_.reset(new logging_win::FileLogger());
    file_logger_->Initialize();
    file_logger_->StartLogging(log_file_);
  } else {
    LOG(ERROR) << "Failed to create an ETW log file";
  }
}

void CFUrlRequestUnittestRunner::StopFileLogger(bool print) {
  if (file_logger_.get() != NULL && file_logger_->is_logging()) {
    file_logger_->StopLogging();

    if (print) {
      // Flushing stdout should prevent unrelated output from being interleaved
      // with the log file output.
      std::cout.flush();
      // Dump the log to stderr.
      logging_win::PrintLogFile(log_file_, &std::cerr);
      std::cerr.flush();
    }
  }

  if (!log_file_.empty() && !file_util::Delete(log_file_, false))
    LOG(ERROR) << "Failed to delete log file " << log_file_.value();

  log_file_.clear();
  file_logger_.reset();
}

const char* IEVersionToString(IEVersion version) {
  switch (version) {
    case IE_6:
      return "IE6";
    case IE_7:
      return "IE7";
    case IE_8:
      return "IE8";
    case IE_9:
      return "IE9";
    case IE_10:
      return "IE10";
    case IE_UNSUPPORTED:
      return "Unknown IE Version";
    case NON_IE:
      return "Could not find IE";
    default:
      return "Error.";
  }
}

content::BrowserMainParts* FakeContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  // We never delete this, as the content module takes ownership.
  //
  // We must not construct this earlier, or we will have out-of-order
  // AtExitManager creation/destruction.
  g_test_suite = new CFUrlRequestUnittestRunner(g_argc, g_argv);
  g_test_suite->set_crash_service(chrome_frame_test::StartCrashService());
  return g_test_suite;
}

// We must provide a few functions content/ expects to link with.  They
// are never called for this executable.
int PluginMain(const content::MainFunctionParams& parameters) {
  NOTREACHED();
  return 0;
}

int PpapiBrokerMain(const content::MainFunctionParams& parameters) {
  return PluginMain(parameters);
}

int PpapiPluginMain(const content::MainFunctionParams& parameters) {
  return PluginMain(parameters);
}

int WorkerMain(const content::MainFunctionParams& parameters) {
  return PluginMain(parameters);
}

int main(int argc, char** argv) {
  ScopedChromeFrameRegistrar::RegisterAndExitProcessIfDirected();
  g_argc = argc;
  g_argv = argv;

  google_breakpad::scoped_ptr<google_breakpad::ExceptionHandler> breakpad(
      InitializeCrashReporting(HEADLESS));

  // Display the IE version we run with. This must be done after
  // CFUrlRequestUnittestRunner is constructed since that initializes logging.
  IEVersion ie_version = chrome_frame_test::GetInstalledIEVersion();
  LOG(INFO) << "Running CF net tests with IE version: "
            << IEVersionToString(ie_version);

  // See url_request_unittest.cc for these credentials.
  SupplyProxyCredentials credentials("user", "secret");
  WindowWatchdog watchdog;
  watchdog.AddObserver(&credentials, "Windows Security", "");

  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  content::InitializeSandboxInfo(&sandbox_info);
  FakeMainDelegate delegate;
  content::ContentMain(
      reinterpret_cast<HINSTANCE>(GetModuleHandle(NULL)),
      &sandbox_info,
      &delegate);

  // Note:  In debug builds, we ExitProcess during PostDestroyThreads.
  return g_test_suite->test_result();
}
