// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/device/tcp_device_provider.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "components/app_modal/native_app_modal_dialog.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/worker_service.h"
#include "content/public/browser/worker_service_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/switches.h"
#include "extensions/common/value_builder.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_http_job.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gl/gl_switches.h"
#include "url/gurl.h"

using app_modal::AppModalDialog;
using app_modal::JavaScriptAppModalDialog;
using app_modal::NativeAppModalDialog;
using content::BrowserThread;
using content::DevToolsAgentHost;
using content::NavigationController;
using content::RenderViewHost;
using content::WebContents;
using content::WorkerService;
using content::WorkerServiceObserver;
using extensions::Extension;

namespace {

const char kDebuggerTestPage[] = "files/devtools/debugger_test_page.html";
const char kPauseWhenLoadingDevTools[] =
    "files/devtools/pause_when_loading_devtools.html";
const char kPauseWhenScriptIsRunning[] =
    "files/devtools/pause_when_script_is_running.html";
const char kPageWithContentScript[] =
    "files/devtools/page_with_content_script.html";
const char kNavigateBackTestPage[] =
    "files/devtools/navigate_back.html";
const char kWindowOpenTestPage[] = "files/devtools/window_open.html";
const char kLatencyInfoTestPage[] = "files/devtools/latency_info.html";
const char kChunkedTestPage[] = "chunked";
const char kPushTestPage[] = "files/devtools/push_test_page.html";
// The resource is not really pushed, but mock url request job pretends it is.
const char kPushTestResource[] = "devtools/image.png";
const char kPushUseNullEndTime[] = "pushUseNullEndTime";
const char kSlowTestPage[] =
    "chunked?waitBeforeHeaders=100&waitBetweenChunks=100&chunksNumber=2";
const char kSharedWorkerTestPage[] =
    "files/workers/workers_ui_shared_worker.html";
const char kSharedWorkerTestWorker[] =
    "files/workers/workers_ui_shared_worker.js";
const char kReloadSharedWorkerTestPage[] =
    "files/workers/debug_shared_worker_initialization.html";
const char kReloadSharedWorkerTestWorker[] =
    "files/workers/debug_shared_worker_initialization.js";

template <typename... T>
void DispatchOnTestSuiteSkipCheck(DevToolsWindow* window,
                                  const char* method,
                                  T... args) {
  RenderViewHost* rvh = DevToolsWindowTesting::Get(window)
                            ->main_web_contents()
                            ->GetRenderViewHost();
  std::string result;
  const char* args_array[] = {method, args...};
  std::ostringstream script;
  script << "uiTests.dispatchOnTestSuite([";
  for (size_t i = 0; i < arraysize(args_array); ++i)
    script << (i ? "," : "") << '\"' << args_array[i] << '\"';
  script << "])";
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractString(rvh, script.str(), &result));
  EXPECT_EQ("[OK]", result);
}

template <typename... T>
void DispatchOnTestSuite(DevToolsWindow* window,
                         const char* method,
                         T... args) {
  std::string result;
  RenderViewHost* rvh = DevToolsWindowTesting::Get(window)
                            ->main_web_contents()
                            ->GetRenderViewHost();
  // At first check that JavaScript part of the front-end is loaded by
  // checking that global variable uiTests exists(it's created after all js
  // files have been loaded) and has runTest method.
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractString(
          rvh,
          "window.domAutomationController.send("
          "    '' + (window.uiTests && (typeof uiTests.dispatchOnTestSuite)));",
          &result));
  ASSERT_EQ("function", result) << "DevTools front-end is broken.";
  DispatchOnTestSuiteSkipCheck(window, method, args...);
}

void RunTestFunction(DevToolsWindow* window, const char* test_name) {
  DispatchOnTestSuite(window, test_name);
}

void SwitchToPanel(DevToolsWindow* window, const char* panel) {
  DispatchOnTestSuite(window, "switchToPanel", panel);
}

// Version of SwitchToPanel that works with extension-created panels.
void SwitchToExtensionPanel(DevToolsWindow* window,
                            const Extension* devtools_extension,
                            const char* panel_name) {
  // The full name is the concatenation of the extension URL (stripped of its
  // trailing '/') and the |panel_name| that was passed to panels.create().
  std::string prefix = base::TrimString(devtools_extension->url().spec(), "/",
                                        base::TRIM_TRAILING)
                           .as_string();
  SwitchToPanel(window, (prefix + panel_name).c_str());
}

class PushTimesMockURLRequestJob : public net::URLRequestMockHTTPJob {
 public:
  PushTimesMockURLRequestJob(net::URLRequest* request,
                             net::NetworkDelegate* network_delegate,
                             base::FilePath file_path)
      : net::URLRequestMockHTTPJob(
            request,
            network_delegate,
            file_path,
            BrowserThread::GetBlockingPool()->GetTaskRunnerWithShutdownBehavior(
                base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)) {}

  void Start() override {
    load_timing_info_.socket_reused = true;
    load_timing_info_.request_start_time = base::Time::Now();
    load_timing_info_.request_start = base::TimeTicks::Now();
    load_timing_info_.send_start = base::TimeTicks::Now();
    load_timing_info_.send_end = base::TimeTicks::Now();
    load_timing_info_.receive_headers_end = base::TimeTicks::Now();

    net::URLRequestMockHTTPJob::Start();
  }

  void GetLoadTimingInfo(net::LoadTimingInfo* load_timing_info) const override {
    load_timing_info_.push_start = load_timing_info_.request_start -
                                   base::TimeDelta::FromMilliseconds(100);
    if (load_timing_info_.push_end.is_null() &&
        request()->url().query() != kPushUseNullEndTime) {
      load_timing_info_.push_end = base::TimeTicks::Now();
    }
    *load_timing_info = load_timing_info_;
  }

 private:
  mutable net::LoadTimingInfo load_timing_info_;
  DISALLOW_COPY_AND_ASSIGN(PushTimesMockURLRequestJob);
};

class TestInterceptor : public net::URLRequestInterceptor {
 public:
  // Creates TestInterceptor and registers it with the URLRequestFilter,
  // which takes ownership of it.
  static void Register(const GURL& url, const base::FilePath& file_path) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        url.scheme(), url.host(),
        base::WrapUnique(new TestInterceptor(url, file_path)));
  }

  // Unregisters previously created TestInterceptor, which should delete it.
  static void Unregister(const GURL& url) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    net::URLRequestFilter::GetInstance()->RemoveHostnameHandler(url.scheme(),
                                                                url.host());
  }

  // net::URLRequestJobFactory::ProtocolHandler implementation:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (request->url().path() != url_.path())
      return nullptr;
    return new PushTimesMockURLRequestJob(request, network_delegate,
                                          file_path_);
  }

 private:
  TestInterceptor(const GURL& url, const base::FilePath& file_path)
      : url_(url), file_path_(file_path) {}

  const GURL url_;
  const base::FilePath file_path_;

  DISALLOW_COPY_AND_ASSIGN(TestInterceptor);
};

}  // namespace

class DevToolsSanityTest : public InProcessBrowserTest {
 public:
  DevToolsSanityTest() : window_(NULL) {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  void RunTest(const std::string& test_name, const std::string& test_page) {
    OpenDevToolsWindow(test_page, false);
    RunTestFunction(window_, test_name.c_str());
    CloseDevToolsWindow();
  }

  template <typename... T>
  void RunTestMethod(const char* method, T... args) {
    DispatchOnTestSuiteSkipCheck(window_, method, args...);
  }

  template <typename... T>
  void DispatchAndWait(const char* method, T... args) {
    DispatchOnTestSuiteSkipCheck(window_, "waitForAsync", method, args...);
  }

  template <typename... T>
  void DispatchInPageAndWait(const char* method, T... args) {
    DispatchAndWait("invokePageFunctionAsync", method, args...);
  }

  void LoadTestPage(const std::string& test_page) {
    GURL url = spawned_test_server()->GetURL(test_page);
    ui_test_utils::NavigateToURL(browser(), url);
  }

  void OpenDevToolsWindow(const std::string& test_page, bool is_docked) {
    ASSERT_TRUE(spawned_test_server()->Start());
    LoadTestPage(test_page);

    window_ = DevToolsWindowTesting::OpenDevToolsWindowSync(GetInspectedTab(),
                                                            is_docked);
  }

  WebContents* GetInspectedTab() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  void CloseDevToolsWindow() {
    DevToolsWindowTesting::CloseDevToolsWindowSync(window_);
  }

  WebContents* main_web_contents() {
    return DevToolsWindowTesting::Get(window_)->main_web_contents();
  }

  WebContents* toolbox_web_contents() {
    return DevToolsWindowTesting::Get(window_)->toolbox_web_contents();
  }

  DevToolsWindow* window_;
};

// Used to block until a dev tools window gets beforeunload event.
class DevToolsWindowBeforeUnloadObserver
    : public content::WebContentsObserver {
 public:
  explicit DevToolsWindowBeforeUnloadObserver(DevToolsWindow*);
  void Wait();
 private:
  // Invoked when the beforeunload handler fires.
  void BeforeUnloadFired(const base::TimeTicks& proceed_time) override;

  bool m_fired;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsWindowBeforeUnloadObserver);
};

DevToolsWindowBeforeUnloadObserver::DevToolsWindowBeforeUnloadObserver(
    DevToolsWindow* devtools_window)
    : WebContentsObserver(
          DevToolsWindowTesting::Get(devtools_window)->main_web_contents()),
      m_fired(false) {
}

void DevToolsWindowBeforeUnloadObserver::Wait() {
  if (m_fired)
    return;
  message_loop_runner_ = new content::MessageLoopRunner;
  message_loop_runner_->Run();
}

void DevToolsWindowBeforeUnloadObserver::BeforeUnloadFired(
    const base::TimeTicks& proceed_time) {
  m_fired = true;
  if (message_loop_runner_.get())
    message_loop_runner_->Quit();
}

class DevToolsBeforeUnloadTest: public DevToolsSanityTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kDisableHangMonitor);
  }

  void CloseInspectedTab() {
    browser()->tab_strip_model()->CloseWebContentsAt(0,
        TabStripModel::CLOSE_NONE);
  }

  void CloseDevToolsWindowAsync() {
    DevToolsWindowTesting::CloseDevToolsWindow(window_);
  }

  void CloseInspectedBrowser() {
    chrome::CloseWindow(browser());
  }

 protected:
  void InjectBeforeUnloadListener(content::WebContents* web_contents) {
    ASSERT_TRUE(content::ExecuteScript(web_contents->GetRenderViewHost(),
        "window.addEventListener('beforeunload',"
        "function(event) { event.returnValue = 'Foo'; });"));
  }

  void RunBeforeUnloadSanityTest(bool is_docked,
                                 base::Callback<void(void)> close_method,
                                 bool wait_for_browser_close = true) {
    OpenDevToolsWindow(kDebuggerTestPage, is_docked);
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    DevToolsWindowTesting::Get(window_)->
        SetCloseCallback(runner->QuitClosure());
    InjectBeforeUnloadListener(main_web_contents());
    {
      DevToolsWindowBeforeUnloadObserver before_unload_observer(window_);
      close_method.Run();
      CancelModalDialog();
      before_unload_observer.Wait();
    }
    {
      content::WindowedNotificationObserver close_observer(
          chrome::NOTIFICATION_BROWSER_CLOSED,
          content::Source<Browser>(browser()));
      close_method.Run();
      AcceptModalDialog();
      if (wait_for_browser_close)
        close_observer.Wait();
    }
    runner->Run();
  }

  DevToolsWindow* OpenDevToolWindowOnWebContents(
      content::WebContents* contents, bool is_docked) {
    DevToolsWindow* window =
        DevToolsWindowTesting::OpenDevToolsWindowSync(contents, is_docked);
    return window;
  }

  void OpenDevToolsPopupWindow(DevToolsWindow* devtools_window) {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    ASSERT_TRUE(content::ExecuteScript(
        DevToolsWindowTesting::Get(devtools_window)->
            main_web_contents()->GetRenderViewHost(),
        "window.open(\"\", \"\", \"location=0\");"));
    observer.Wait();
  }

  void CloseDevToolsPopupWindow(DevToolsWindow* devtools_window) {
    DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window);
  }

  void AcceptModalDialog() {
    NativeAppModalDialog* native_dialog = GetDialog();
    native_dialog->AcceptAppModalDialog();
  }

  void CancelModalDialog() {
    NativeAppModalDialog* native_dialog = GetDialog();
    native_dialog->CancelAppModalDialog();
  }

  NativeAppModalDialog* GetDialog() {
    AppModalDialog* dialog = ui_test_utils::WaitForAppModalDialog();
    EXPECT_TRUE(dialog->IsJavaScriptModalDialog());
    JavaScriptAppModalDialog* js_dialog =
        static_cast<JavaScriptAppModalDialog*>(dialog);
    NativeAppModalDialog* native_dialog = js_dialog->native_dialog();
    EXPECT_TRUE(native_dialog);
    return native_dialog;
  }
};

class DevToolsUnresponsiveBeforeUnloadTest: public DevToolsBeforeUnloadTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {}
};

void TimeoutCallback(const std::string& timeout_message) {
  ADD_FAILURE() << timeout_message;
  base::MessageLoop::current()->QuitWhenIdle();
}

// Base class for DevTools tests that test devtools functionality for
// extensions and content scripts.
class DevToolsExtensionTest : public DevToolsSanityTest,
                              public content::NotificationObserver {
 public:
  DevToolsExtensionTest() : DevToolsSanityTest() {
    PathService::Get(chrome::DIR_TEST_DATA, &test_extensions_dir_);
    test_extensions_dir_ = test_extensions_dir_.AppendASCII("devtools");
    test_extensions_dir_ = test_extensions_dir_.AppendASCII("extensions");
  }

 protected:
  // Load an extension from test\data\devtools\extensions\<extension_name>
  void LoadExtension(const char* extension_name) {
    base::FilePath path = test_extensions_dir_.AppendASCII(extension_name);
    ASSERT_TRUE(LoadExtensionFromPath(path)) << "Failed to load extension.";
  }

  const Extension* LoadExtensionFromPath(const base::FilePath& path) {
    ExtensionService* service = extensions::ExtensionSystem::Get(
        browser()->profile())->extension_service();
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(browser()->profile());
    size_t num_before = registry->enabled_extensions().size();
    {
      content::NotificationRegistrar registrar;
      registrar.Add(this,
                    extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
                    content::NotificationService::AllSources());
      base::CancelableClosure timeout(
          base::Bind(&TimeoutCallback, "Extension load timed out."));
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, timeout.callback(), TestTimeouts::action_timeout());
      extensions::UnpackedInstaller::Create(service)->Load(path);
      content::RunMessageLoop();
      timeout.Cancel();
    }
    size_t num_after = registry->enabled_extensions().size();
    if (num_after != (num_before + 1))
      return nullptr;

    if (!WaitForExtensionViewsToLoad())
      return nullptr;

    return GetExtensionByPath(registry->enabled_extensions(), path);
  }

 private:
  const Extension* GetExtensionByPath(
      const extensions::ExtensionSet& extensions,
      const base::FilePath& path) {
    base::FilePath extension_path = base::MakeAbsoluteFilePath(path);
    EXPECT_TRUE(!extension_path.empty());
    for (const scoped_refptr<const Extension>& extension : extensions) {
      if (extension->path() == extension_path) {
        return extension.get();
      }
    }
    return nullptr;
  }

  bool WaitForExtensionViewsToLoad() {
    // Wait for all the extension render views that exist to finish loading.
    // NOTE: This assumes that the extension views list is not changing while
    // this method is running.

    content::NotificationRegistrar registrar;
    registrar.Add(this,
                  extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD,
                  content::NotificationService::AllSources());
    base::CancelableClosure timeout(
        base::Bind(&TimeoutCallback, "Extension host load timed out."));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, timeout.callback(), TestTimeouts::action_timeout());

    extensions::ProcessManager* manager =
        extensions::ProcessManager::Get(browser()->profile());
    extensions::ProcessManager::FrameSet all_frames = manager->GetAllFrames();
    for (extensions::ProcessManager::FrameSet::const_iterator iter =
             all_frames.begin();
         iter != all_frames.end();) {
      if (!content::WebContents::FromRenderFrameHost(*iter)->IsLoading())
        ++iter;
      else
        content::RunMessageLoop();
    }

    timeout.Cancel();
    return true;
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    switch (type) {
      case extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED:
      case extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD:
        base::MessageLoopForUI::current()->QuitWhenIdle();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  base::FilePath test_extensions_dir_;
};

class DevToolsExperimentalExtensionTest : public DevToolsExtensionTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        extensions::switches::kEnableExperimentalExtensionApis);
  }
};

class WorkerDevToolsSanityTest : public InProcessBrowserTest {
 public:
  WorkerDevToolsSanityTest() : window_(NULL) {}

 protected:
  class WorkerData : public base::RefCountedThreadSafe<WorkerData> {
   public:
    WorkerData() : worker_process_id(0), worker_route_id(0) {}
    int worker_process_id;
    int worker_route_id;

   private:
    friend class base::RefCountedThreadSafe<WorkerData>;
    ~WorkerData() {}
  };

  class WorkerCreationObserver : public WorkerServiceObserver {
   public:
    explicit WorkerCreationObserver(const std::string& path,
                                    WorkerData* worker_data)
        : path_(path), worker_data_(worker_data) {}

   private:
    ~WorkerCreationObserver() override {}

    void WorkerCreated(const GURL& url,
                       const base::string16& name,
                       int process_id,
                       int route_id) override {
      if (url.path().rfind(path_) == std::string::npos)
        return;
      worker_data_->worker_process_id = process_id;
      worker_data_->worker_route_id = route_id;
      WorkerService::GetInstance()->RemoveObserver(this);
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              base::MessageLoop::QuitWhenIdleClosure());
      delete this;
    }
    std::string path_;
    scoped_refptr<WorkerData> worker_data_;
  };

  class WorkerTerminationObserver : public WorkerServiceObserver {
   public:
    explicit WorkerTerminationObserver(WorkerData* worker_data)
        : worker_data_(worker_data) {
    }

   private:
    ~WorkerTerminationObserver() override {}

    void WorkerDestroyed(int process_id, int route_id) override {
      ASSERT_EQ(worker_data_->worker_process_id, process_id);
      ASSERT_EQ(worker_data_->worker_route_id, route_id);
      WorkerService::GetInstance()->RemoveObserver(this);
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              base::MessageLoop::QuitWhenIdleClosure());
      delete this;
    }
    scoped_refptr<WorkerData> worker_data_;
  };

  void RunTest(const char* test_name,
               const char* test_page,
               const char* worker_path) {
    ASSERT_TRUE(spawned_test_server()->Start());
    GURL url = spawned_test_server()->GetURL(test_page);
    ui_test_utils::NavigateToURL(browser(), url);

    scoped_refptr<WorkerData> worker_data =
        WaitForFirstSharedWorker(worker_path);
    OpenDevToolsWindowForSharedWorker(worker_data.get());
    RunTestFunction(window_, test_name);
    CloseDevToolsWindow();
  }

  static void TerminateWorkerOnIOThread(scoped_refptr<WorkerData> worker_data) {
    if (!WorkerService::GetInstance()->TerminateWorker(
        worker_data->worker_process_id, worker_data->worker_route_id))
      FAIL() << "Failed to terminate worker.\n";
    WorkerService::GetInstance()->AddObserver(
        new WorkerTerminationObserver(worker_data.get()));
  }

  static void TerminateWorker(scoped_refptr<WorkerData> worker_data) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&TerminateWorkerOnIOThread, worker_data));
    content::RunMessageLoop();
  }

  static void WaitForFirstSharedWorkerOnIOThread(
      const std::string& path,
      scoped_refptr<WorkerData> worker_data) {
    std::vector<WorkerService::WorkerInfo> worker_info =
        WorkerService::GetInstance()->GetWorkers();
    for (size_t i = 0; i < worker_info.size(); i++) {
      if (worker_info[i].url.path().rfind(path) == std::string::npos)
        continue;
      worker_data->worker_process_id = worker_info[0].process_id;
      worker_data->worker_route_id = worker_info[0].route_id;
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              base::MessageLoop::QuitWhenIdleClosure());
      return;
    }

    WorkerService::GetInstance()->AddObserver(
        new WorkerCreationObserver(path, worker_data.get()));
  }

  static scoped_refptr<WorkerData> WaitForFirstSharedWorker(const char* path) {
    scoped_refptr<WorkerData> worker_data(new WorkerData());
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&WaitForFirstSharedWorkerOnIOThread, path, worker_data));
    content::RunMessageLoop();
    return worker_data;
  }

  void OpenDevToolsWindowForSharedWorker(WorkerData* worker_data) {
    Profile* profile = browser()->profile();
    scoped_refptr<DevToolsAgentHost> agent_host(
        DevToolsAgentHost::GetForWorker(
            worker_data->worker_process_id,
            worker_data->worker_route_id));
    window_ = DevToolsWindowTesting::OpenDevToolsWindowForWorkerSync(
        profile, agent_host.get());
  }

  void CloseDevToolsWindow() {
    DevToolsWindowTesting::CloseDevToolsWindowSync(window_);
  }

  DevToolsWindow* window_;
};

// Tests that BeforeUnload event gets called on docked devtools if
// we try to close them.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest, TestDockedDevToolsClose) {
  RunBeforeUnloadSanityTest(true, base::Bind(
      &DevToolsBeforeUnloadTest::CloseDevToolsWindowAsync, this), false);
}

// Tests that BeforeUnload event gets called on docked devtools if
// we try to close the inspected page.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestDockedDevToolsInspectedTabClose) {
  RunBeforeUnloadSanityTest(true, base::Bind(
      &DevToolsBeforeUnloadTest::CloseInspectedTab, this));
}

// Tests that BeforeUnload event gets called on docked devtools if
// we try to close the inspected browser.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestDockedDevToolsInspectedBrowserClose) {
  RunBeforeUnloadSanityTest(true, base::Bind(
      &DevToolsBeforeUnloadTest::CloseInspectedBrowser, this));
}

// Tests that BeforeUnload event gets called on undocked devtools if
// we try to close them.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest, TestUndockedDevToolsClose) {
  RunBeforeUnloadSanityTest(false, base::Bind(
      &DevToolsBeforeUnloadTest::CloseDevToolsWindowAsync, this), false);
}

// Tests that BeforeUnload event gets called on undocked devtools if
// we try to close the inspected page.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestUndockedDevToolsInspectedTabClose) {
  RunBeforeUnloadSanityTest(false, base::Bind(
      &DevToolsBeforeUnloadTest::CloseInspectedTab, this));
}

// Tests that BeforeUnload event gets called on undocked devtools if
// we try to close the inspected browser.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestUndockedDevToolsInspectedBrowserClose) {
  RunBeforeUnloadSanityTest(false, base::Bind(
      &DevToolsBeforeUnloadTest::CloseInspectedBrowser, this));
}

// Tests that BeforeUnload event gets called on undocked devtools if
// we try to exit application.
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestUndockedDevToolsApplicationClose) {
  RunBeforeUnloadSanityTest(false, base::Bind(
      &chrome::CloseAllBrowsers));
}

// Tests that inspected tab gets closed if devtools renderer
// becomes unresponsive during beforeunload event interception.
// @see http://crbug.com/322380
// Disabled because of http://crbug.com/410327
IN_PROC_BROWSER_TEST_F(DevToolsUnresponsiveBeforeUnloadTest,
                       DISABLED_TestUndockedDevToolsUnresponsive) {
  ASSERT_TRUE(spawned_test_server()->Start());
  LoadTestPage(kDebuggerTestPage);
  DevToolsWindow* devtools_window = OpenDevToolWindowOnWebContents(
      GetInspectedTab(), false);

  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  DevToolsWindowTesting::Get(devtools_window)->SetCloseCallback(
      runner->QuitClosure());

  ASSERT_TRUE(content::ExecuteScript(
      DevToolsWindowTesting::Get(devtools_window)->main_web_contents()->
          GetRenderViewHost(),
      "window.addEventListener('beforeunload',"
      "function(event) { while (true); });"));
  CloseInspectedTab();
  runner->Run();
}

// Tests that closing worker inspector window does not cause browser crash
// @see http://crbug.com/323031
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       TestWorkerWindowClosing) {
  ASSERT_TRUE(spawned_test_server()->Start());
  LoadTestPage(kDebuggerTestPage);
  DevToolsWindow* devtools_window = OpenDevToolWindowOnWebContents(
      GetInspectedTab(), false);

  OpenDevToolsPopupWindow(devtools_window);
  CloseDevToolsPopupWindow(devtools_window);
}

// Tests that BeforeUnload event gets called on devtools that are opened
// on another devtools.
// Disabled because of http://crbug.com/497857
IN_PROC_BROWSER_TEST_F(DevToolsBeforeUnloadTest,
                       DISABLED_TestDevToolsOnDevTools) {
  ASSERT_TRUE(spawned_test_server()->Start());
  LoadTestPage(kDebuggerTestPage);

  std::vector<DevToolsWindow*> windows;
  std::vector<content::WindowedNotificationObserver*> close_observers;
  content::WebContents* inspected_web_contents = GetInspectedTab();
  for (int i = 0; i < 3; ++i) {
    DevToolsWindow* devtools_window = OpenDevToolWindowOnWebContents(
      inspected_web_contents, i == 0);
    windows.push_back(devtools_window);
    content::WindowedNotificationObserver* close_observer =
        new content::WindowedNotificationObserver(
                content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                content::Source<content::WebContents>(
                    DevToolsWindowTesting::Get(devtools_window)->
                        main_web_contents()));
    close_observers.push_back(close_observer);
    inspected_web_contents =
        DevToolsWindowTesting::Get(devtools_window)->main_web_contents();
  }

  InjectBeforeUnloadListener(
      DevToolsWindowTesting::Get(windows[0])->main_web_contents());
  InjectBeforeUnloadListener(
      DevToolsWindowTesting::Get(windows[2])->main_web_contents());
  // Try to close second devtools.
  {
    content::WindowedNotificationObserver cancel_browser(
        chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED,
        content::NotificationService::AllSources());
    chrome::CloseWindow(DevToolsWindowTesting::Get(windows[1])->browser());
    CancelModalDialog();
    cancel_browser.Wait();
  }
  // Try to close browser window.
  {
    content::WindowedNotificationObserver cancel_browser(
        chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED,
        content::NotificationService::AllSources());
    chrome::CloseWindow(browser());
    AcceptModalDialog();
    CancelModalDialog();
    cancel_browser.Wait();
  }
  // Try to exit application.
  {
    content::WindowedNotificationObserver close_observer(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::Source<Browser>(browser()));
    chrome::CloseAllBrowsers();
    AcceptModalDialog();
    AcceptModalDialog();
    close_observer.Wait();
  }
  for (size_t i = 0; i < close_observers.size(); ++i) {
    close_observers[i]->Wait();
    delete close_observers[i];
  }
}

// Tests scripts panel showing.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestShowScriptsTab) {
  RunTest("testShowScriptsTab", kDebuggerTestPage);
}

// Tests that scripts tab is populated with inspected scripts even if it
// hadn't been shown by the moment inspected paged refreshed.
// @see http://crbug.com/26312
IN_PROC_BROWSER_TEST_F(
    DevToolsSanityTest,
    TestScriptsTabIsPopulatedOnInspectedPageRefresh) {
  RunTest("testScriptsTabIsPopulatedOnInspectedPageRefresh",
          kDebuggerTestPage);
}

// Tests that chrome.devtools extension is correctly exposed.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       TestDevToolsExtensionAPI) {
  LoadExtension("devtools_extension");
  RunTest("waitForTestResultsInConsole", std::string());
}

// Tests a chrome.devtools extension panel that embeds an http:// iframe.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest, DevToolsExtensionWithHttpIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Our extension must load an URL from the test server, whose port is only
  // known at runtime. So, to embed the URL, we must dynamically generate the
  // extension, rather than loading it from static content.
  std::unique_ptr<extensions::TestExtensionDir> dir(
      new extensions::TestExtensionDir());

  extensions::DictionaryBuilder manifest;
  dir->WriteManifest(extensions::DictionaryBuilder()
                         .Set("name", "Devtools Panel w/ HTTP Iframe")
                         .Set("version", "1")
                         .Set("manifest_version", 2)
                         .Set("devtools_page", "devtools.html")
                         .ToJSON());

  dir->WriteFile(
      FILE_PATH_LITERAL("devtools.html"),
      "<html><head><script src='devtools.js'></script></head></html>");

  dir->WriteFile(
      FILE_PATH_LITERAL("devtools.js"),
      "chrome.devtools.panels.create('iframe_panel',\n"
      "    null,\n"
      "    'panel.html',\n"
      "    function(panel) {\n"
      "      chrome.devtools.inspectedWindow.eval('console.log(\"PASS\")');\n"
      "    }\n"
      ");\n");

  GURL http_iframe =
      embedded_test_server()->GetURL("a.com", "/popup_iframe.html");
  dir->WriteFile(FILE_PATH_LITERAL("panel.html"),
                 "<html><body>Extension panel.<iframe src='" +
                     http_iframe.spec() + "'></iframe>");

  // Install the extension.
  const Extension* extension = LoadExtensionFromPath(dir->unpacked_path());
  ASSERT_TRUE(extension);

  // Open a devtools window.
  OpenDevToolsWindow(kDebuggerTestPage, false);

  // Wait for the panel extension to finish loading -- it'll output 'PASS'
  // when it's installed. waitForTestResultsInConsole waits until that 'PASS'.
  RunTestFunction(window_, "waitForTestResultsInConsole");

  // Now that we know the panel is loaded, switch to it. We'll wait until we
  // see a 'DONE' message sent from popup_iframe.html, indicating that it
  // loaded successfully.
  content::DOMMessageQueue message_queue;
  SwitchToExtensionPanel(window_, extension, "iframe_panel");
  std::string message;
  while (true) {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
    if (message == "\"DONE\"")
      break;
  }
}

// Disabled on Windows due to flakiness. http://crbug.com/183649
#if defined(OS_WIN)
#define MAYBE_TestDevToolsExtensionMessaging DISABLED_TestDevToolsExtensionMessaging
#else
#define MAYBE_TestDevToolsExtensionMessaging TestDevToolsExtensionMessaging
#endif

// Tests that chrome.devtools extension can communicate with background page
// using extension messaging.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       MAYBE_TestDevToolsExtensionMessaging) {
  LoadExtension("devtools_messaging");
  RunTest("waitForTestResultsInConsole", std::string());
}

// Tests that chrome.experimental.devtools extension is correctly exposed
// when the extension has experimental permission.
IN_PROC_BROWSER_TEST_F(DevToolsExperimentalExtensionTest,
                       TestDevToolsExperimentalExtensionAPI) {
  LoadExtension("devtools_experimental");
  RunTest("waitForTestResultsInConsole", std::string());
}

// Tests that a content script is in the scripts list.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionTest,
                       TestContentScriptIsPresent) {
  LoadExtension("simple_content_script");
  RunTest("testContentScriptIsPresent", kPageWithContentScript);
}

// Tests that scripts are not duplicated after Scripts Panel switch.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest,
                       TestNoScriptDuplicatesOnPanelSwitch) {
  RunTest("testNoScriptDuplicatesOnPanelSwitch", kDebuggerTestPage);
}

// Tests that debugger works correctly if pause event occurs when DevTools
// frontend is being loaded.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest,
                       TestPauseWhenLoadingDevTools) {
  RunTest("testPauseWhenLoadingDevTools", kPauseWhenLoadingDevTools);
}

// Tests that pressing 'Pause' will pause script execution if the script
// is already running.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on linux ARM bot: https://crbug/238453
#define MAYBE_TestPauseWhenScriptIsRunning DISABLED_TestPauseWhenScriptIsRunning
#else
#define MAYBE_TestPauseWhenScriptIsRunning TestPauseWhenScriptIsRunning
#endif
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest,
                       MAYBE_TestPauseWhenScriptIsRunning) {
  RunTest("testPauseWhenScriptIsRunning", kPauseWhenScriptIsRunning);
}

// Tests network timing.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestNetworkTiming) {
  RunTest("testNetworkTiming", kSlowTestPage);
}

// Tests network size.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestNetworkSize) {
  RunTest("testNetworkSize", kChunkedTestPage);
}

// Tests raw headers text.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestNetworkSyncSize) {
  RunTest("testNetworkSyncSize", kChunkedTestPage);
}

// Tests raw headers text.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestNetworkRawHeadersText) {
  RunTest("testNetworkRawHeadersText", kChunkedTestPage);
}

IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestNetworkPushTime) {
  OpenDevToolsWindow(kPushTestPage, false);
  GURL push_url = spawned_test_server()->GetURL(kPushTestResource);
  base::FilePath file_path =
      spawned_test_server()->document_root().AppendASCII(kPushTestResource);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&TestInterceptor::Register, push_url, file_path));

  DispatchOnTestSuite(window_, "testPushTimes", push_url.spec().c_str());

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&TestInterceptor::Unregister, push_url));

  CloseDevToolsWindow();
}

// Tests that console messages are not duplicated on navigation back.
#if defined(OS_WIN)
// Flaking on windows swarm try runs: crbug.com/409285.
#define MAYBE_TestConsoleOnNavigateBack DISABLED_TestConsoleOnNavigateBack
#else
#define MAYBE_TestConsoleOnNavigateBack TestConsoleOnNavigateBack
#endif
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, MAYBE_TestConsoleOnNavigateBack) {
  RunTest("testConsoleOnNavigateBack", kNavigateBackTestPage);
}

IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestDeviceEmulation) {
  RunTest("testDeviceMetricsOverrides", "about:blank");
}

// Tests that settings are stored in profile correctly.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestSettings) {
  OpenDevToolsWindow("about:blank", true);
  RunTestFunction(window_, "testSettings");
  CloseDevToolsWindow();
}

// Tests that external navigation from inspector page is always handled by
// DevToolsWindow and results in inspected page navigation.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestDevToolsExternalNavigation) {
  OpenDevToolsWindow(kDebuggerTestPage, true);
  GURL url = spawned_test_server()->GetURL(kNavigateBackTestPage);
  ui_test_utils::UrlLoadObserver observer(url,
      content::NotificationService::AllSources());
  ASSERT_TRUE(content::ExecuteScript(
      main_web_contents(),
      std::string("window.location = \"") + url.spec() + "\""));
  observer.Wait();

  ASSERT_TRUE(main_web_contents()->GetURL().
                  SchemeIs(content::kChromeDevToolsScheme));
  ASSERT_EQ(url, GetInspectedTab()->GetURL());
  CloseDevToolsWindow();
}

// Tests that toolbox window is loaded when DevTools window is undocked.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestToolboxLoadedUndocked) {
  OpenDevToolsWindow(kDebuggerTestPage, false);
  ASSERT_TRUE(toolbox_web_contents());
  DevToolsWindow* on_self =
      DevToolsWindowTesting::OpenDevToolsWindowSync(main_web_contents(), false);
  ASSERT_FALSE(DevToolsWindowTesting::Get(on_self)->toolbox_web_contents());
  DevToolsWindowTesting::CloseDevToolsWindowSync(on_self);
  CloseDevToolsWindow();
}

// Tests that toolbox window is not loaded when DevTools window is docked.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestToolboxNotLoadedDocked) {
  OpenDevToolsWindow(kDebuggerTestPage, true);
  ASSERT_FALSE(toolbox_web_contents());
  DevToolsWindow* on_self =
      DevToolsWindowTesting::OpenDevToolsWindowSync(main_web_contents(), false);
  ASSERT_FALSE(DevToolsWindowTesting::Get(on_self)->toolbox_web_contents());
  DevToolsWindowTesting::CloseDevToolsWindowSync(on_self);
  CloseDevToolsWindow();
}

// Tests that inspector will reattach to inspected page when it is reloaded
// after a crash. See http://crbug.com/101952
// Disabled. it doesn't check anything right now: http://crbug.com/461790
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, DISABLED_TestReattachAfterCrash) {
  RunTest("testReattachAfterCrash", std::string());
}

IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestPageWithNoJavaScript) {
  OpenDevToolsWindow("about:blank", false);
  std::string result;
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractString(
          main_web_contents()->GetRenderViewHost(),
          "window.domAutomationController.send("
          "    '' + (window.uiTests && (typeof uiTests.dispatchOnTestSuite)));",
          &result));
  ASSERT_EQ("function", result) << "DevTools front-end is broken.";
  CloseDevToolsWindow();
}

class DevToolsAutoOpenerTest : public DevToolsSanityTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kAutoOpenDevToolsForTabs);
    observer_.reset(new DevToolsWindowCreationObserver());
  }
 protected:
  std::unique_ptr<DevToolsWindowCreationObserver> observer_;
};

IN_PROC_BROWSER_TEST_F(DevToolsAutoOpenerTest, TestAutoOpenForTabs) {
  {
    DevToolsWindowCreationObserver observer;
    AddTabAtIndexToBrowser(browser(), 0, GURL("about:blank"),
        ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
    observer.WaitForLoad();
  }
  Browser* new_browser = nullptr;
  {
    DevToolsWindowCreationObserver observer;
    new_browser = CreateBrowser(browser()->profile());
    observer.WaitForLoad();
  }
  {
    DevToolsWindowCreationObserver observer;
    AddTabAtIndexToBrowser(new_browser, 0, GURL("about:blank"),
        ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
    observer.WaitForLoad();
  }
  observer_->CloseAllSync();
}

class DevToolsReattachAfterCrashTest : public DevToolsSanityTest {
 protected:
  void RunTestWithPanel(const char* panel_name) {
    OpenDevToolsWindow("about:blank", false);
    SwitchToPanel(window_, panel_name);
    ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

    content::RenderProcessHostWatcher crash_observer(
        GetInspectedTab(),
        content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    ui_test_utils::NavigateToURL(browser(), GURL(content::kChromeUICrashURL));
    crash_observer.Wait();
    content::TestNavigationObserver navigation_observer(GetInspectedTab(), 1);
    chrome::Reload(browser(), CURRENT_TAB);
    navigation_observer.Wait();
  }
};

IN_PROC_BROWSER_TEST_F(DevToolsReattachAfterCrashTest,
                       TestReattachAfterCrashOnTimeline) {
  RunTestWithPanel("timeline");
}

IN_PROC_BROWSER_TEST_F(DevToolsReattachAfterCrashTest,
                       TestReattachAfterCrashOnNetwork) {
  RunTestWithPanel("network");
}

IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, AutoAttachToWindowOpen) {
  OpenDevToolsWindow(kWindowOpenTestPage, false);
  DispatchOnTestSuite(window_, "enableAutoAttachToCreatedPages");
  DevToolsWindowCreationObserver observer;
  ASSERT_TRUE(content::ExecuteScript(
      GetInspectedTab(), "window.open('window_open.html', '_blank');"));
  observer.WaitForLoad();
  DispatchOnTestSuite(observer.devtools_window(), "waitForDebuggerPaused");
  DevToolsWindowTesting::CloseDevToolsWindowSync(observer.devtools_window());
  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(WorkerDevToolsSanityTest, InspectSharedWorker) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests))
    return;
#endif

  RunTest("testSharedWorker", kSharedWorkerTestPage, kSharedWorkerTestWorker);
}

IN_PROC_BROWSER_TEST_F(WorkerDevToolsSanityTest,
                       PauseInSharedWorkerInitialization) {
  ASSERT_TRUE(spawned_test_server()->Start());
  GURL url = spawned_test_server()->GetURL(kReloadSharedWorkerTestPage);
  ui_test_utils::NavigateToURL(browser(), url);

  scoped_refptr<WorkerData> worker_data =
      WaitForFirstSharedWorker(kReloadSharedWorkerTestWorker);
  OpenDevToolsWindowForSharedWorker(worker_data.get());

  // We should make sure that the worker inspector has loaded before
  // terminating worker.
  RunTestFunction(window_, "testPauseInSharedWorkerInitialization1");

  TerminateWorker(worker_data);

  // Reload page to restart the worker.
  ui_test_utils::NavigateToURL(browser(), url);

  // Wait until worker script is paused on the debugger statement.
  RunTestFunction(window_, "testPauseInSharedWorkerInitialization2");
  CloseDevToolsWindow();
}

class DevToolsAgentHostTest : public InProcessBrowserTest {};

// Tests DevToolsAgentHost retention by its target.
IN_PROC_BROWSER_TEST_F(DevToolsAgentHostTest, TestAgentHostReleased) {
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  WebContents* web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  DevToolsAgentHost* agent_raw =
      DevToolsAgentHost::GetOrCreateFor(web_contents).get();
  const std::string agent_id = agent_raw->GetId();
  ASSERT_EQ(agent_raw, DevToolsAgentHost::GetForId(agent_id).get())
      << "DevToolsAgentHost cannot be found by id";
  browser()->tab_strip_model()->
      CloseWebContentsAt(0, TabStripModel::CLOSE_NONE);
  ASSERT_FALSE(DevToolsAgentHost::GetForId(agent_id).get())
      << "DevToolsAgentHost is not released when the tab is closed";
}

class RemoteDebuggingTest : public ExtensionApiTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kRemoteDebuggingPort, "9222");

    // Override the extension root path.
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
    test_data_dir_ = test_data_dir_.AppendASCII("devtools");
  }
};

// Fails on CrOS. crbug.com/431399
#if defined(OS_CHROMEOS)
#define MAYBE_RemoteDebugger DISABLED_RemoteDebugger
#else
#define MAYBE_RemoteDebugger RemoteDebugger
#endif
IN_PROC_BROWSER_TEST_F(RemoteDebuggingTest, MAYBE_RemoteDebugger) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests))
    return;
#endif

  ASSERT_TRUE(RunExtensionTest("target_list")) << message_;
}

using DevToolsPolicyTest = InProcessBrowserTest;
IN_PROC_BROWSER_TEST_F(DevToolsPolicyTest, PolicyTrue) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kDevToolsDisabled, true);
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  scoped_refptr<content::DevToolsAgentHost> agent(
      content::DevToolsAgentHost::GetOrCreateFor(web_contents));
  DevToolsWindow::OpenDevToolsWindow(web_contents);
  DevToolsWindow* window = DevToolsWindow::FindDevToolsWindow(agent.get());
  ASSERT_FALSE(window);
}

class DevToolsPixelOutputTests : public DevToolsSanityTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnablePixelOutputInTests);
    command_line->AppendSwitch(switches::kUseGpuInTests);
  }
};

// This test enables switches::kUseGpuInTests which causes false positives
// with MemorySanitizer.
#if defined(MEMORY_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    (defined(OS_CHROMEOS) && defined(OFFICIAL_BUILD))
#define MAYBE_TestScreenshotRecording DISABLED_TestScreenshotRecording
#else
#define MAYBE_TestScreenshotRecording TestScreenshotRecording
#endif
IN_PROC_BROWSER_TEST_F(DevToolsPixelOutputTests,
                       MAYBE_TestScreenshotRecording) {
  RunTest("testScreenshotRecording", std::string());
}

// This test enables switches::kUseGpuInTests which causes false positives
// with MemorySanitizer.
#if defined(MEMORY_SANITIZER) || defined(ADDRESS_SANITIZER)
#define MAYBE_TestLatencyInfoInstrumentation \
  DISABLED_TestLatencyInfoInstrumentation
#else
#define MAYBE_TestLatencyInfoInstrumentation TestLatencyInfoInstrumentation
#endif
IN_PROC_BROWSER_TEST_F(DevToolsPixelOutputTests,
                       MAYBE_TestLatencyInfoInstrumentation) {
  WebContents* web_contents = GetInspectedTab();
  OpenDevToolsWindow(kLatencyInfoTestPage, false);
  RunTestMethod("enableExperiment", "timelineLatencyInfo");
  DispatchAndWait("startTimeline");

  for (int i = 0; i < 3; ++i) {
    SimulateMouseEvent(web_contents, blink::WebInputEvent::MouseMove,
                       gfx::Point(30, 60));
    DispatchInPageAndWait("waitForEvent", "mousemove");
  }

  SimulateMouseClickAt(web_contents, 0, blink::WebPointerProperties::ButtonLeft,
                       gfx::Point(30, 60));
  DispatchInPageAndWait("waitForEvent", "click");

  SimulateMouseWheelEvent(web_contents, gfx::Point(300, 100),
                          gfx::Vector2d(0, 120));
  DispatchInPageAndWait("waitForEvent", "wheel");

  SimulateTapAt(web_contents, gfx::Point(30, 60));
  DispatchInPageAndWait("waitForEvent", "gesturetap");

  DispatchAndWait("stopTimeline");
  RunTestMethod("checkInputEventsPresent", "MouseMove", "MouseDown",
                "MouseWheel", "GestureTap");

  CloseDevToolsWindow();
}
