// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome_frame/test/perf/chrome_frame_perftest.h"

#include <atlwin.h>
#include <atlhost.h>
#include <map>
#include <vector>
#include <string>

#include "chrome_tab.h"  // Generated from chrome_tab.idl.

#include "base/debug/trace_event_win.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/win/event_trace_consumer.h"
#include "base/win/event_trace_controller.h"
#include "base/win/registry.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/test/chrome_process_util.h"
#include "chrome/test/ui/ui_perf_test.h"
#include "chrome_frame/test_utils.h"
#include "chrome_frame/utils.h"

const wchar_t kSilverlightControlKey[] =
    L"CLSID\\{DFEAF541-F3E1-4c24-ACAC-99C30715084A}\\InprocServer32";

const wchar_t kFlashControlKey[] =
    L"CLSID\\{D27CDB6E-AE6D-11cf-96B8-444553540000}\\InprocServer32";

using base::TimeDelta;
using base::TimeTicks;

// This class implements an ActiveX container which hosts the ChromeFrame
// ActiveX control. It provides hooks which can be implemented by derived
// classes for implementing performance measurement, etc.
class ChromeFrameActiveXContainer
    : public CWindowImpl<ChromeFrameActiveXContainer, CWindow, CWinTraits <
          WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
          WS_EX_APPWINDOW | WS_EX_WINDOWEDGE> >,
      public CComObjectRootEx<CComSingleThreadModel>,
      public IPropertyNotifySink {
 public:
  ~ChromeFrameActiveXContainer() {
    if (m_hWnd)
      DestroyWindow();
  }

  DECLARE_WND_CLASS_EX(L"ChromeFrameActiveX_container", 0, 0)

  BEGIN_COM_MAP(ChromeFrameActiveXContainer)
    COM_INTERFACE_ENTRY(IPropertyNotifySink)
  END_COM_MAP()

  BEGIN_MSG_MAP(ChromeFrameActiveXContainer)
    MESSAGE_HANDLER(WM_CREATE, OnCreate)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
  END_MSG_MAP()

  HRESULT OnMessageCallback(const VARIANT* param) {
    DVLOG(1) << __FUNCTION__;
    OnMessageCallbackImpl(param);
    return S_OK;
  }

  HRESULT OnLoadErrorCallback(const VARIANT* param) {
    DVLOG(1) << __FUNCTION__ << " " << param->bstrVal;
    OnLoadErrorCallbackImpl(param);
    return S_OK;
  }

  HRESULT OnLoadCallback(const VARIANT* param) {
    DVLOG(1) << __FUNCTION__ << " " << param->bstrVal;
    OnLoadCallbackImpl(param);
    return S_OK;
  }

  ChromeFrameActiveXContainer() :
      prop_notify_cookie_(0),
      onmsg_(this, &ChromeFrameActiveXContainer::OnMessageCallback),
      onloaderror_(this, &ChromeFrameActiveXContainer::OnLoadErrorCallback),
      onload_(this, &ChromeFrameActiveXContainer::OnLoadCallback) {
  }

  LRESULT OnCreate(UINT , WPARAM , LPARAM , BOOL& ) {
    chromeview_.Attach(m_hWnd);
    return 0;
  }

  // This will be called twice.
  // Once from CAxHostWindow::OnDestroy (through DefWindowProc)
  // and once more from the ATL since CAxHostWindow::OnDestroy claims the
  // message is not handled.
  LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {  // NOLINT
    if (prop_notify_cookie_) {
      AtlUnadvise(tab_, IID_IPropertyNotifySink, prop_notify_cookie_);
      prop_notify_cookie_ = 0;
    }

    tab_.Release();
    return 0;
  }

  virtual void OnFinalMessage(HWND /*hWnd*/) {
    ::PostQuitMessage(6);
  }

  static const wchar_t* GetWndCaption() {
    return L"ChromeFrame Container";
  }

  // IPropertyNotifySink
  STDMETHOD(OnRequestEdit)(DISPID disp_id) {
    OnRequestEditImpl(disp_id);
    return S_OK;
  }

  STDMETHOD(OnChanged)(DISPID disp_id) {
    if (disp_id != DISPID_READYSTATE)
      return S_OK;

    long ready_state;
    HRESULT hr = tab_->get_readyState(&ready_state);
    DCHECK(hr == S_OK);

    OnReadyStateChanged(ready_state);

    if (ready_state == READYSTATE_COMPLETE) {
      if (!starting_url_.empty()) {
        Navigate(starting_url_.c_str());
      } else {
        PostMessage(WM_CLOSE);
      }
    } else if (ready_state == READYSTATE_UNINITIALIZED) {
      DLOG(ERROR) << __FUNCTION__ << " Chrome launch failed.";
    }

    return S_OK;
  }

  void CreateChromeFrameWindow(const std::string& starting_url) {
    starting_url_ = starting_url;
    RECT rc = { 0, 0, 800, 600 };
    Create(NULL, rc);
    DCHECK(m_hWnd);
    ShowWindow(SW_SHOWDEFAULT);
  }

  void CreateControl(bool setup_event_sinks) {
    HRESULT hr = chromeview_.CreateControl(L"ChromeTab.ChromeFrame");
    EXPECT_HRESULT_SUCCEEDED(hr);
    hr = chromeview_.QueryControl(tab_.Receive());
    EXPECT_HRESULT_SUCCEEDED(hr);

    if (setup_event_sinks)
      SetupEventSinks();
  }

  void Navigate(const char* url) {
    BeforeNavigateImpl(url);

    HRESULT hr = tab_->put_src(base::win::ScopedBstr(UTF8ToWide(url).c_str()));
    DCHECK(hr == S_OK) << "Chrome frame NavigateToURL(" << url
                       << base::StringPrintf(L") failed 0x%08X", hr);
  }

  void SetupEventSinks() {
    HRESULT hr = AtlAdvise(tab_, this, IID_IPropertyNotifySink,
                           &prop_notify_cookie_);
    DCHECK(hr == S_OK) << "AtlAdvice for IPropertyNotifySink failed " << hr;

    base::win::ScopedVariant onmessage(onmsg_.ToDispatch());
    base::win::ScopedVariant onloaderror(onloaderror_.ToDispatch());
    base::win::ScopedVariant onload(onload_.ToDispatch());
    EXPECT_HRESULT_SUCCEEDED(tab_->put_onmessage(onmessage));
    EXPECT_HRESULT_SUCCEEDED(tab_->put_onloaderror(onloaderror));
    EXPECT_HRESULT_SUCCEEDED(tab_->put_onload(onload));
  }

 protected:
  // These functions are implemented by derived classes for special behavior
  // like performance measurement, etc.
  virtual void OnReadyStateChanged(long ready_state) {}
  virtual void OnRequestEditImpl(DISPID disp_id) {}

  virtual void OnMessageCallbackImpl(const VARIANT* param) {}

  virtual void OnLoadCallbackImpl(const VARIANT* param) {
    PostMessage(WM_CLOSE);
  }

  virtual void OnLoadErrorCallbackImpl(const VARIANT* param) {
    PostMessage(WM_CLOSE);
  }
  virtual void BeforeNavigateImpl(const char* url) {}

  CAxWindow chromeview_;
  base::win::ScopedComPtr<IChromeFrame> tab_;
  DWORD prop_notify_cookie_;
  DispCallback<ChromeFrameActiveXContainer> onmsg_;
  DispCallback<ChromeFrameActiveXContainer> onloaderror_;
  DispCallback<ChromeFrameActiveXContainer> onload_;
  std::string starting_url_;
};

// This class overrides the hooks provided by the ChromeFrameActiveXContainer
// class and measures performance at various stages, like initialzation of
// the Chrome frame widget, navigation, etc.
class ChromeFrameActiveXContainerPerf : public ChromeFrameActiveXContainer {
 public:
  ChromeFrameActiveXContainerPerf() {}

  void CreateControl(bool setup_event_sinks) {
    perf_initialize_.reset(new PerfTimeLogger("Fully initialized"));
    PerfTimeLogger perf_create("Create Control");

    HRESULT hr = chromeview_.CreateControl(L"ChromeTab.ChromeFrame");
    EXPECT_HRESULT_SUCCEEDED(hr);
    hr = chromeview_.QueryControl(tab_.Receive());
    EXPECT_HRESULT_SUCCEEDED(hr);

    perf_create.Done();
    if (setup_event_sinks)
      SetupEventSinks();
  }

 protected:
  virtual void OnReadyStateChanged(long ready_state) {
    // READYSTATE_COMPLETE is fired when the automation server is ready.
    if (ready_state == READYSTATE_COMPLETE) {
      perf_initialize_->Done();
    } else if (ready_state == READYSTATE_INTERACTIVE) {
      // Window ready.  Currently we never receive this notification because it
      // is fired before we finish setting up our hosting environment.
      // This is because of how ATL is written.  Moving forward we might
      // have our own hosting classes and then have more control over when we
      // set up the prop notify sink.
    } else {
      DCHECK(ready_state != READYSTATE_UNINITIALIZED) << "failed to initialize";
    }
  }

  virtual void OnLoadCallbackImpl(const VARIANT* param) {
    PostMessage(WM_CLOSE);
    perf_navigate_->Done();
  }

  virtual void OnLoadErrorCallbackImpl(const VARIANT* param) {
    PostMessage(WM_CLOSE);
    perf_navigate_->Done();
  }

  virtual void BeforeNavigateImpl(const char* url ) {
    std::string test_name = "Navigate ";
    test_name += url;
    perf_navigate_.reset(new PerfTimeLogger(test_name.c_str()));
  }

  scoped_ptr<PerfTimeLogger> perf_initialize_;
  scoped_ptr<PerfTimeLogger> perf_navigate_;
};

// This class provides common functionality which can be used for most of the
// ChromeFrame/Tab performance tests.
class ChromeFramePerfTestBase : public UIPerfTest {
 public:
  ChromeFramePerfTestBase() {}
 protected:
  scoped_ptr<ScopedChromeFrameRegistrar> chrome_frame_registrar_;
};

class ChromeFrameStartupTest : public ChromeFramePerfTestBase {
 public:
  ChromeFrameStartupTest() {}

  virtual void SetUp() {
    ASSERT_TRUE(PathService::Get(chrome::DIR_APP, &dir_app_));

    chrome_dll_ = dir_app_.Append(FILE_PATH_LITERAL("chrome.dll"));
    chrome_exe_ = dir_app_.Append(
        FilePath::FromWStringHack(chrome::kBrowserProcessExecutableName));
    chrome_frame_dll_ = dir_app_.Append(FILE_PATH_LITERAL("servers"));
    chrome_frame_dll_ = chrome_frame_dll_.Append(
        FilePath::FromWStringHack(kChromeFrameDllName));
    icu_dll_ = dir_app_.Append(FILE_PATH_LITERAL("icudt46.dll"));
    gears_dll_ = dir_app_.Append(FILE_PATH_LITERAL("gears.dll"));
    avcodec52_dll_ = dir_app_.Append(FILE_PATH_LITERAL("avcodec-52.dll"));
    avformat52_dll_ = dir_app_.Append(FILE_PATH_LITERAL("avformat-52.dll"));
    avutil50_dll_ = dir_app_.Append(FILE_PATH_LITERAL("avutil-50.dll"));
  }

  // TODO(iyengar)
  // This function is similar to the RunStartupTest function used in chrome
  // startup tests. Refactor into a common implementation.
  void RunStartupTest(const char* graph, const char* trace,
                      const char* startup_url, bool test_cold,
                      int total_binaries, const FilePath binaries_to_evict[],
                      bool important, bool ignore_cache_error) {
    const int kNumCycles = 20;

    startup_url_ = startup_url;

    TimeDelta timings[kNumCycles];

    for (int i = 0; i < kNumCycles; ++i) {
      if (test_cold) {
        for (int binary_index = 0; binary_index < total_binaries;
             binary_index++) {
          bool result = EvictFileFromSystemCacheWrapper(
              binaries_to_evict[binary_index]);
          if (!ignore_cache_error) {
            ASSERT_TRUE(result);
          } else if (!result) {
            LOG(ERROR) << GetLastError();
            printf("\nFailed to evict file %ls from cache. Not running test\n",
                   binaries_to_evict[binary_index].value().c_str());
            return;
          }
        }
      }

      TimeTicks start_time, end_time;

      RunStartupTestImpl(&start_time, &end_time);

      timings[i] = end_time - start_time;

      CoFreeUnusedLibraries();

      // TODO(beng): Can't shut down so quickly. Figure out why, and fix. If we
      // do, we crash.
      base::PlatformThread::Sleep(50);
    }

    std::string times;
    for (int i = 0; i < kNumCycles; ++i)
      base::StringAppendF(&times, "%.2f,", timings[i].InMillisecondsF());

    PrintResultList(graph, "", trace, times, "ms", important);
  }

  FilePath dir_app_;
  FilePath chrome_dll_;
  FilePath chrome_exe_;
  FilePath chrome_frame_dll_;
  FilePath icu_dll_;
  FilePath gears_dll_;
  FilePath avcodec52_dll_;
  FilePath avformat52_dll_;
  FilePath avutil50_dll_;

 protected:
  // Individual startup tests should implement this function.
  virtual void RunStartupTestImpl(TimeTicks* start_time,
                                  TimeTicks* end_time) {}

  // The host is torn down by this function. It should not be used after
  // this function returns.
  static void ReleaseHostComReferences(CAxWindow& host) {
    CComPtr<IAxWinHostWindow> spWinHost;
    host.QueryHost(&spWinHost);
    ASSERT_TRUE(spWinHost != NULL);

    // Hack to get the host to release all interfaces and thus ensure that
    // the COM server can be unloaded.
    CAxHostWindow* host_window = static_cast<CAxHostWindow*>(spWinHost.p);
    host_window->ReleaseAll();
    host.DestroyWindow();
  }

  std::string startup_url_;
};

class ChromeFrameStartupTestActiveX : public ChromeFrameStartupTest {
 public:
  virtual void SetUp() {
    // Register the Chrome Frame DLL in the build directory.
    chrome_frame_registrar_.reset(new ScopedChromeFrameRegistrar(
        ScopedChromeFrameRegistrar::SYSTEM_LEVEL));

    ChromeFrameStartupTest::SetUp();
  }

 protected:
  virtual void RunStartupTestImpl(TimeTicks* start_time,
                                  TimeTicks* end_time) {
    *start_time = TimeTicks::Now();
    SimpleModule module;
    AtlAxWinInit();
    CComObjectStackEx<ChromeFrameActiveXContainer> wnd;
    wnd.CreateChromeFrameWindow(startup_url_);
    wnd.CreateControl(true);
    module.RunMessageLoop();
    *end_time = TimeTicks::Now();
  }
};

// This class measures the load time of chrome and chrome frame binaries
class ChromeFrameBinariesLoadTest : public ChromeFrameStartupTestActiveX {
  static const size_t kStepSize = 4 * 1024;
 public:
  ChromeFrameBinariesLoadTest()
      : pre_read_(false),
        step_size_(kStepSize),
        bytes_to_read_(0) {}

 protected:
  virtual void RunStartupTestImpl(TimeTicks* start_time,
                                  TimeTicks* end_time) {
    *start_time = TimeTicks::Now();

    if (pre_read_) {
      EXPECT_TRUE(file_util::PreReadImage(chrome_exe_.value().c_str(),
                                          bytes_to_read_,
                                          step_size_));
      EXPECT_TRUE(file_util::PreReadImage(chrome_dll_.value().c_str(),
                                          bytes_to_read_,
                                          step_size_));
    }

    HMODULE chrome_exe = LoadLibrary(chrome_exe_.value().c_str());
    EXPECT_TRUE(chrome_exe != NULL);

    HMODULE chrome_dll = LoadLibrary(chrome_dll_.value().c_str());
    EXPECT_TRUE(chrome_dll != NULL);

    *end_time = TimeTicks::Now();

    FreeLibrary(chrome_exe);
    FreeLibrary(chrome_dll);
  }

  bool pre_read_;
  size_t bytes_to_read_;
  size_t step_size_;
};

// This class provides functionality to run the startup performance test for
// the ChromeFrame ActiveX against a reference build. At this point we only run
// this test in warm mode.
class ChromeFrameStartupTestActiveXReference
    : public ChromeFrameStartupTestActiveX {
 public:
  // override the browser directory to use the reference build instead.
  virtual void SetUp() {
    // Register the reference build Chrome Frame DLL.
    chrome_frame_registrar_.reset(new ScopedChromeFrameRegistrar(
        ScopedChromeFrameRegistrar::SYSTEM_LEVEL));
    chrome_frame_registrar_->RegisterReferenceChromeFrameBuild();

    ChromeFrameStartupTest::SetUp();

    chrome_frame_dll_ = FilePath(
        chrome_frame_registrar_->GetReferenceChromeFrameDllPath());
    DVLOG(1) << __FUNCTION__ << ": " << chrome_frame_dll_.value();
  }

  virtual void TearDown() {
    // Reregister the Chrome Frame DLL in the build directory.
    chrome_frame_registrar_.reset(NULL);
  }
};

// This class provides base functionality to measure ChromeFrame memory
// usage.
// TODO(iyengar)
// Some of the functionality in this class like printing the results, etc
// is based on the chrome\test\memory_test.cc. We need to factor out
// the common code.
class ChromeFrameMemoryTest : public ChromeFramePerfTestBase {
  // Contains information about the memory consumption of a process.
  class ProcessMemoryInfo {
   public:
    // Default constructor
    // Added to enable us to add ProcessMemoryInfo instances to a map.
    ProcessMemoryInfo()
        : process_id_(0),
          virtual_size_(0),
          working_set_size_(0),
          chrome_browser_process_(false),
          chrome_frame_memory_test_instance_(NULL) {}

    ProcessMemoryInfo(base::ProcessId process_id, bool chrome_browser_process,
                     ChromeFrameMemoryTest* memory_test_instance)
        : process_id_(process_id),
          virtual_size_(0),
          working_set_size_(0),
          chrome_browser_process_(chrome_browser_process),
          chrome_frame_memory_test_instance_(memory_test_instance) {}

    bool GetMemoryConsumptionDetails() {
      base::ProcessHandle process_handle;
      if (!base::OpenPrivilegedProcessHandle(process_id_, &process_handle)) {
        NOTREACHED();
      }

      // TODO(sgk):  if/when base::ProcessMetrics can return real memory
      // stats on mac, convert to:
      //
      // scoped_ptr<base::ProcessMetrics> process_metrics;
      // process_metrics.reset(
      //     base::ProcessMetrics::CreateProcessMetrics(process_handle));
      scoped_ptr<ChromeTestProcessMetrics> process_metrics;
      process_metrics.reset(
          ChromeTestProcessMetrics::CreateProcessMetrics(process_handle));

      virtual_size_ = process_metrics->GetPagefileUsage();
      working_set_size_ = process_metrics->GetWorkingSetSize();

      return true;
    }

    void Print(const char* test_name) {
      std::string trace_name(test_name);

      ASSERT_TRUE(chrome_frame_memory_test_instance_ != NULL);

      if (chrome_browser_process_) {
         chrome_frame_memory_test_instance_->PrintResult(
             "vm_final_browser", "", trace_name + "_vm_b",
             virtual_size_ / 1024, "KB", false /* not important */);
         chrome_frame_memory_test_instance_->PrintResult(
             "ws_final_browser", "", trace_name + "_ws_b",
             working_set_size_ / 1024, "KB", false /* not important */);
      } else if (process_id_ == base::GetCurrentProcId()) {
        chrome_frame_memory_test_instance_->PrintResult(
            "vm_current_process", "", trace_name + "_vm_c",
            virtual_size_ / 1024, "KB", false /* not important */);
        chrome_frame_memory_test_instance_->PrintResult(
            "ws_current_process", "", trace_name + "_ws_c",
            working_set_size_ / 1024, "KB", false /* not important */);
      }

      printf("\n");
    }

    base::ProcessId process_id_;
    size_t virtual_size_;
    size_t working_set_size_;
    // Set to true if this is the chrome browser process.
    bool chrome_browser_process_;

    // A reference to the ChromeFrameMemoryTest instance. Used to print memory
    // consumption information.
    ChromeFrameMemoryTest* chrome_frame_memory_test_instance_;
  };

  // This map tracks memory usage for a process. It is keyed on the process
  // id.
  typedef std::map<DWORD, ProcessMemoryInfo> ProcessMemoryConsumptionMap;

 public:
  ChromeFrameMemoryTest() : current_url_index_(0) {
  }

  virtual void SetUp() {
    // Register the Chrome Frame DLL in the build directory.
    chrome_frame_registrar_.reset(new ScopedChromeFrameRegistrar(
        ScopedChromeFrameRegistrar::SYSTEM_LEVEL));
  }

  void RunTest(const char* test_name, char* urls[], int total_urls) {
    ASSERT_TRUE(urls != NULL);
    ASSERT_GT(total_urls, 0);

    // Record the initial CommitCharge.  This is a system-wide measurement,
    // so if other applications are running, they can create variance in this
    // test.
    start_commit_charge_ = base::GetSystemCommitCharge();

    for (int i = 0; i < total_urls; i++)
      urls_.push_back(urls[i]);

    std::string url;
    GetNextUrl(&url);
    ASSERT_TRUE(!url.empty());

    StartTest(url, test_name);
  }

  void OnNavigationSuccess(const VARIANT* param) {
    ASSERT_TRUE(param != NULL);
    ASSERT_EQ(VT_BSTR, param->vt);

    DVLOG(1) << __FUNCTION__ << " " << param->bstrVal;
    InitiateNextNavigation();
  }

  void OnNavigationFailure(const VARIANT* param) {
    ASSERT_TRUE(param != NULL);
    ASSERT_EQ(VT_BSTR, param->vt);

    DVLOG(1) << __FUNCTION__ << " " << param->bstrVal;
    InitiateNextNavigation();
  }

 protected:
  bool GetNextUrl(std::string* url) {
    if (current_url_index_ >= urls_.size())
      return false;

    *url = urls_[current_url_index_++];
    return true;
  }

  void InitiateNextNavigation() {
    // Get the memory consumption information for the child processes
    // of the chrome browser.
    ChromeProcessList child_processes = GetBrowserChildren();
    ChromeProcessList::iterator index;
    for (index = child_processes.begin(); index != child_processes.end();
         ++index) {
      AccountProcessMemoryUsage(*index);
    }

    // TODO(iyengar): Bug 2953
    // Need to verify if this is still true.
    // The automation crashes periodically if we cycle too quickly.
    // To make these tests more reliable, slowing them down a bit.
    Sleep(200);

    std::string url;
    bool next_url = GetNextUrl(&url);
    if (!url.empty()) {
      NavigateImpl(url);
    } else {
      TestCompleted();
    }
  }

  void PrintResults(const char* test_name) {
    PrintMemoryUsageInfo(test_name);
    memory_consumption_map_.clear();

    // Added to give the OS some time to flush the used pages for the
    // chrome processes which would have exited by now.
    Sleep(200);

    size_t end_commit_charge = base::GetSystemCommitCharge();
    size_t commit_size = (end_commit_charge - start_commit_charge_) * 1024;

    std::string trace_name(test_name);
    trace_name.append("_cc");

    PrintResult("commit_charge", "", trace_name,
                commit_size / 1024, "KB", true /* important */);
    printf("\n");
  }

  base::ProcessId chrome_browser_process_id() {
    base::NamedProcessIterator iter(L"chrome.exe", NULL);
    const base::ProcessEntry* entry = iter.NextProcessEntry();
    if (entry) {
      return entry->pid();
    }
    return -1;
  }

  ChromeProcessList GetBrowserChildren() {
    ChromeProcessList list = GetRunningChromeProcesses(
        chrome_browser_process_id());
    ChromeProcessList::iterator browser =
        std::find(list.begin(), list.end(), chrome_browser_process_id());
    if (browser != list.end()) {
      list.erase(browser);
    }
    return list;
  }

  void AccountProcessMemoryUsage(DWORD process_id) {
    ProcessMemoryInfo process_memory_info(
        process_id, process_id == chrome_browser_process_id(), this);

    ASSERT_TRUE(process_memory_info.GetMemoryConsumptionDetails());

    memory_consumption_map_[process_id] = process_memory_info;
  }

  void PrintMemoryUsageInfo(const char* test_name) {
    printf("\n");

    std::string trace_name(test_name);

    ProcessMemoryConsumptionMap::iterator index;
    size_t total_virtual_size = 0;
    size_t total_working_set_size = 0;

    for (index = memory_consumption_map_.begin();
         index != memory_consumption_map_.end();
         ++index) {
      ProcessMemoryInfo& memory_info = (*index).second;
      memory_info.Print(test_name);

      total_virtual_size += memory_info.virtual_size_;
      total_working_set_size += memory_info.working_set_size_;
    }

    printf("\n");

    PrintResult("vm_final_total", "", trace_name + "_vm",
                total_virtual_size / 1024, "KB",
                false /* not important */);
    PrintResult("ws_final_total", "", trace_name + "_ws",
                total_working_set_size / 1024, "KB",
                true /* important */);
  }

  // Should never get called.
  virtual void StartTest(const std::string& url,
                         const std::string& test_name) = 0 {
    ASSERT_FALSE(false);
  }

  // Should never get called.
  virtual void NavigateImpl(const std::string& url) = 0 {
    ASSERT_FALSE(false);
  }

  virtual void TestCompleted() = 0 {
    ASSERT_FALSE(false);
  }

  // Holds the commit charge in KBytes at the start of the memory test run.
  size_t start_commit_charge_;

  // The index of the URL being tested.
  size_t current_url_index_;

  // Contains the list of urls against which the tests are run.
  std::vector<std::string> urls_;

  ProcessMemoryConsumptionMap memory_consumption_map_;
};

// This class provides functionality to run the memory test against a reference
// chrome frame build.
class ChromeFrameMemoryTestReference : public ChromeFrameMemoryTest {
 public:
  virtual void SetUp() {
    chrome_frame_registrar_.reset(new ScopedChromeFrameRegistrar(
        ScopedChromeFrameRegistrar::SYSTEM_LEVEL));
    chrome_frame_registrar_->RegisterReferenceChromeFrameBuild();
  }

  virtual void TearDown() {
    // Reregisters the chrome frame DLL in the build directory.
    chrome_frame_registrar_.reset(NULL);
  }
};

// This class overrides the hooks provided by the ChromeFrameActiveXContainer
// class and calls back into the ChromeFrameMemoryTest object instance,
// which measures ChromeFrame memory usage.
class ChromeFrameActiveXContainerMemory : public ChromeFrameActiveXContainer {
 public:
  ChromeFrameActiveXContainerMemory()
      : delegate_(NULL) {}

  ~ChromeFrameActiveXContainerMemory() {}

  void Initialize(ChromeFrameMemoryTest* delegate) {
    ASSERT_TRUE(delegate != NULL);
    delegate_ = delegate;
  }

 protected:
  virtual void OnLoadCallbackImpl(const VARIANT* param) {
    delegate_->OnNavigationSuccess(param);
  }

  virtual void OnLoadErrorCallbackImpl(const VARIANT* param) {
    delegate_->OnNavigationFailure(param);
  }

  ChromeFrameMemoryTest* delegate_;
};

// This class runs memory tests against the ChromeFrame ActiveX.
template<class MemoryTestBase>
class ChromeFrameActiveXMemoryTest : public MemoryTestBase {
 public:
  ChromeFrameActiveXMemoryTest()
      : chrome_frame_container_(NULL),
        test_completed_(false) {}

  ~ChromeFrameActiveXMemoryTest() {
  }

  void StartTest(const std::string& url, const std::string& test_name) {
    ASSERT_TRUE(chrome_frame_container_ == NULL);

    test_name_ = test_name;

    SimpleModule module;
    AtlAxWinInit();

    CComObject<ChromeFrameActiveXContainerMemory>::CreateInstance(
        &chrome_frame_container_);
    chrome_frame_container_->AddRef();

    chrome_frame_container_->Initialize(this);

    chrome_frame_container_->CreateChromeFrameWindow(url.c_str());
    chrome_frame_container_->CreateControl(true);

    module.RunMessageLoop();

    chrome_frame_container_->Release();

    PrintResults(test_name_.c_str());

    CoFreeUnusedLibraries();
    base::PlatformThread::Sleep(100);
  }

  void NavigateImpl(const std::string& url) {
    ASSERT_TRUE(chrome_frame_container_ != NULL);
    ASSERT_TRUE(!url.empty());
    chrome_frame_container_->Navigate(url.c_str());
  }

  void TestCompleted() {
    // This can get called multiple times if the last url results in a
    // redirect.
    if (!test_completed_) {
      // Measure memory usage for the browser process.
      AccountProcessMemoryUsage(chrome_browser_process_id());
      // Measure memory usage for the current process.
      AccountProcessMemoryUsage(GetCurrentProcessId());

      test_completed_ = true;
      EXPECT_TRUE(PostMessage(static_cast<HWND>(*chrome_frame_container_),
                              WM_CLOSE, 0, 0));
    }
  }

 protected:
  CComObject<ChromeFrameActiveXContainerMemory>* chrome_frame_container_;
  std::string test_name_;
  bool test_completed_;
};

// This class runs tests to measure chrome frame creation only. This will help
// track overall page load performance with chrome frame instances.
class ChromeFrameCreationTest : public ChromeFrameStartupTest {
 protected:
  virtual void RunStartupTestImpl(TimeTicks* start_time,
                                  TimeTicks* end_time) {
    SimpleModule module;
    AtlAxWinInit();
    CComObjectStackEx<ChromeFrameActiveXContainer> wnd;
    wnd.CreateChromeFrameWindow(startup_url_);
    *start_time = TimeTicks::Now();
    wnd.CreateControl(false);
    *end_time = TimeTicks::Now();
  }
};

// This class provides functionality to run the chrome frame
// performance test against a reference build.
class ChromeFrameCreationTestReference : public ChromeFrameCreationTest {
 public:
  // override the browser directory to use the reference build instead.
  virtual void SetUp() {
    chrome_frame_registrar_.reset(new ScopedChromeFrameRegistrar(
        ScopedChromeFrameRegistrar::SYSTEM_LEVEL));
    chrome_frame_registrar_->RegisterReferenceChromeFrameBuild();
    ChromeFrameStartupTest::SetUp();
  }

  virtual void TearDown() {
    chrome_frame_registrar_.reset(NULL);
  }
};

// This class measures the creation time for Flash, which would be used
// as a baseline to measure chrome frame creation performance.
class FlashCreationTest : public ChromeFrameStartupTest {
 protected:
  virtual void RunStartupTestImpl(TimeTicks* start_time,
                                  TimeTicks* end_time) {
    SimpleModule module;
    AtlAxWinInit();
    CAxWindow host;
    RECT rc = {0, 0, 800, 600};
    host.Create(NULL, rc, NULL,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);
    EXPECT_TRUE(host.m_hWnd != NULL);

    *start_time = TimeTicks::Now();
    HRESULT hr = host.CreateControl(L"ShockwaveFlash.ShockwaveFlash");
    EXPECT_HRESULT_SUCCEEDED(hr);
    *end_time = TimeTicks::Now();

    ReleaseHostComReferences(host);
  }
};

// This class measures the creation time for Silverlight, which would be used
// as a baseline to measure chrome frame creation performance.
class SilverlightCreationTest : public ChromeFrameStartupTest {
 protected:
  virtual void RunStartupTestImpl(TimeTicks* start_time,
                                  TimeTicks* end_time) {
    SimpleModule module;
    AtlAxWinInit();
    CAxWindow host;
    RECT rc = {0, 0, 800, 600};
    host.Create(NULL, rc, NULL,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);
    EXPECT_TRUE(host.m_hWnd != NULL);

    *start_time = TimeTicks::Now();
    HRESULT hr = host.CreateControl(L"AgControl.AgControl");
    EXPECT_HRESULT_SUCCEEDED(hr);
    *end_time = TimeTicks::Now();

    ReleaseHostComReferences(host);
  }
};

TEST(ChromeFramePerf, DISABLED_HostActiveX) {
  // TODO(stoyan): Create a low integrity level thread && perform the test there
  SimpleModule module;
  AtlAxWinInit();
  CComObjectStackEx<ChromeFrameActiveXContainerPerf> wnd;
  wnd.CreateChromeFrameWindow("http://www.google.com");
  wnd.CreateControl(true);
  module.RunMessageLoop();
}

TEST(ChromeFramePerf, DISABLED_HostActiveXInvalidURL) {
  // TODO(stoyan): Create a low integrity level thread && perform the test there
  SimpleModule module;
  AtlAxWinInit();
  CComObjectStackEx<ChromeFrameActiveXContainerPerf> wnd;
  wnd.CreateChromeFrameWindow("http://non-existent-domain.org/");
  wnd.CreateControl(true);
  module.RunMessageLoop();
}

TEST_F(ChromeFrameStartupTestActiveX, PerfWarm) {
  RunStartupTest("warm", "t", "about:blank", false /* cold */, 0, NULL,
                 true /* important */, false);
}

TEST_F(ChromeFrameBinariesLoadTest, PerfWarm) {
  RunStartupTest("binary_load_warm", "t", "", false /* cold */, 0, NULL,
                 true /* important */, false);
}

TEST_F(ChromeFrameStartupTestActiveX, PerfCold) {
  SetConfigInt(L"PreRead", 0);
  FilePath binaries_to_evict[] = { gears_dll_, avcodec52_dll_,
      avformat52_dll_, avutil50_dll_, chrome_exe_, chrome_dll_,
      chrome_frame_dll_};
  RunStartupTest("cold", "t", "about:blank", true /* cold */,
                 arraysize(binaries_to_evict), binaries_to_evict,
                 false /* not important */, false);
  DeleteConfigValue(L"PreRead");
}

TEST_F(ChromeFrameStartupTestActiveX, PerfColdPreRead) {
  SetConfigInt(L"PreRead", 1);
  FilePath binaries_to_evict[] = { gears_dll_, avcodec52_dll_,
      avformat52_dll_, avutil50_dll_, chrome_exe_, chrome_dll_,
      chrome_frame_dll_};
  RunStartupTest("cold_preread", "t", "about:blank", true /* cold */,
                 arraysize(binaries_to_evict), binaries_to_evict,
                 false /* not important */, false);
  DeleteConfigValue(L"PreRead");
}

TEST_F(ChromeFrameBinariesLoadTest, PerfCold) {
  FilePath binaries_to_evict[] = {chrome_exe_, chrome_dll_};
  RunStartupTest("binary_load_cold", "t", "", true /* cold */,
                 arraysize(binaries_to_evict), binaries_to_evict,
                 false /* not important */, false);
}

TEST_F(ChromeFrameBinariesLoadTest, PerfColdPreRead) {
  FilePath binaries_to_evict[] = {chrome_exe_, chrome_dll_};
  pre_read_ = true;
  RunStartupTest("binary_load_cold_preread", "t", "", true /* cold */,
                 arraysize(binaries_to_evict), binaries_to_evict,
                 false /* not important */, false);
}

TEST_F(ChromeFrameStartupTestActiveXReference, PerfWarm) {
  RunStartupTest("warm", "t_ref", "about:blank", false /* cold */, 0, NULL,
                 true /* important */, false);
}

TEST_F(ChromeFrameStartupTestActiveX, PerfChromeFrameInitializationWarm) {
  RunStartupTest("ChromeFrame_init_warm", "t", "", false /* cold */, 0,
                 NULL, true /* important */, false);
}

TEST_F(ChromeFrameStartupTestActiveX, PerfChromeFrameInitializationCold) {
  FilePath binaries_to_evict[] = {chrome_frame_dll_};
  RunStartupTest("ChromeFrame_init_cold", "t", "", true /* cold */,
                 arraysize(binaries_to_evict), binaries_to_evict,
                 false /* not important */, false);
}

TEST_F(ChromeFrameStartupTestActiveXReference,
       PerfChromeFrameInitializationWarm) {
  RunStartupTest("ChromeFrame_init_warm", "t_ref", "", false /* cold */, 0,
                 NULL, true /* important */, false);
}

typedef ChromeFrameActiveXMemoryTest<ChromeFrameMemoryTest>
    RegularChromeFrameActiveXMemoryTest;

TEST_F(RegularChromeFrameActiveXMemoryTest, MemoryTestAboutBlank) {
  char *urls[] = {"about:blank"};
  RunTest("memory_about_blank", urls, arraysize(urls));
}

// TODO(iyengar)
// Revisit why the chrome frame dll does not unload correctly when this test is
// run.
// http://code.google.com/p/chromium/issues/detail?id=47812
TEST_F(RegularChromeFrameActiveXMemoryTest, DISABLED_MemoryTestUrls) {
  // TODO(iyengar)
  // We should use static pages to measure memory usage.
  char *urls[] = {
    "http://www.youtube.com/watch?v=PN2HAroA12w",
    "http://www.youtube.com/watch?v=KmLJDrsaJmk&feature=channel"
  };

  RunTest("memory", urls, arraysize(urls));
}

typedef ChromeFrameActiveXMemoryTest<ChromeFrameMemoryTestReference>
    ReferenceBuildChromeFrameActiveXMemoryTest;

// Disabled to investigate why the chrome frame dll does not unload while
// running this test.
// http://code.google.com/p/chromium/issues/detail?id=47812
TEST_F(ReferenceBuildChromeFrameActiveXMemoryTest,
       DISABLED_MemoryTestAboutBlank) {
  char *urls[] = {"about:blank"};
  RunTest("memory_about_blank_reference", urls, arraysize(urls));
}

// TODO(iyengar)
// Revisit why the chrome frame dll does not unload correctly when this test is
// run.
TEST_F(ReferenceBuildChromeFrameActiveXMemoryTest, DISABLED_MemoryTestUrls) {
  // TODO(iyengar)
  // We should use static pages to measure memory usage.
  char *urls[] = {
    "http://www.youtube.com/watch?v=PN2HAroA12w",
    "http://www.youtube.com/watch?v=KmLJDrsaJmk&feature=channel"
  };

  RunTest("memory_reference", urls, arraysize(urls));
}

TEST_F(ChromeFrameCreationTest, PerfWarm) {
  RunStartupTest("creation_warm", "t", "", false /* cold */, 0,
                 NULL, true /* important */, false);
}

TEST_F(ChromeFrameCreationTestReference, PerfWarm) {
  RunStartupTest("creation_warm", "t_ref", "about:blank", false /* cold */, 0,
                 NULL, true /* not important */, false);
}

TEST_F(FlashCreationTest, DISABLED_PerfWarm) {
  RunStartupTest("creation_warm", "t_flash", "", false /* cold */, 0, NULL,
                 true /* not important */, false);
}

TEST_F(SilverlightCreationTest, DISABLED_PerfWarm) {
  RunStartupTest("creation_warm", "t_silverlight", "", false /* cold */, 0,
                 NULL, false /* not important */, false);
}

TEST_F(ChromeFrameCreationTest, PerfCold) {
  FilePath binaries_to_evict[] = {chrome_frame_dll_};

  RunStartupTest("creation_cold", "t", "", true /* cold */,
                 arraysize(binaries_to_evict), binaries_to_evict,
                 true /* important */, false);
}

// Attempt to evict the Flash control can fail on the buildbot as the dll
// is marked read only. The test run is aborted if we fail to evict the file
// from the cache. This could also fail if the Flash control is in use.
// On Vista this could fail because of UAC
TEST_F(FlashCreationTest, PerfCold) {
  base::win::RegKey flash_key(HKEY_CLASSES_ROOT, kFlashControlKey, KEY_READ);

  std::wstring plugin_path;
  ASSERT_EQ(ERROR_SUCCESS, flash_key.ReadValue(L"", &plugin_path));
  ASSERT_FALSE(plugin_path.empty());

  FilePath flash_path = FilePath::FromWStringHack(plugin_path);
  FilePath binaries_to_evict[] = {flash_path};

  RunStartupTest("creation_cold", "t_flash", "", true /* cold */,
                 arraysize(binaries_to_evict), binaries_to_evict,
                 false/* important */, true);
}

// This test would fail on Vista due to UAC or if the Silverlight control is
// in use. The test run is aborted if we fail to evict the file from the cache.
// Disabling this test as the Silverlight dll does not seem to get unloaded
// correctly causing the attempt to evict the dll from the system cache to
// fail.
TEST_F(SilverlightCreationTest, DISABLED_PerfCold) {
  base::win::RegKey silverlight_key(HKEY_CLASSES_ROOT, kSilverlightControlKey,
                                    KEY_READ);

  std::wstring plugin_path;
  ASSERT_EQ(ERROR_SUCCESS, silverlight_key.ReadValue(L"", &plugin_path));
  ASSERT_FALSE(plugin_path.empty());

  FilePath silverlight_path = FilePath::FromWStringHack(plugin_path);
  FilePath binaries_to_evict[] = {silverlight_path};

  RunStartupTest("creation_cold", "t_silverlight", "", true /* cold */,
                 arraysize(binaries_to_evict), binaries_to_evict,
                 false /* important */, true);
}

namespace {

// Derive from this class in order to receive custom events traced
// via TRACE_EVENT_XXXX macros from ChromeFrame/Chrome.
class TracedEvents {
 public:
  virtual void OnTraceEventBegin(EVENT_TRACE* event) {}
  virtual void OnTraceEventEnd(EVENT_TRACE* event) {}
  virtual void OnTraceEventInstant(EVENT_TRACE* event) {}
};

// For the time being we pass to delegate only base::kTraceEventClass32
// events i.e. these generated by TRACE_EVENT_XXXX macros.
// We may need to add kernel provider and pass Process Start/Exit events etc,
// but for the time being we stick with base::kChromeTraceProviderName
// provider only.
class EtwConsumer : public base::win::EtwTraceConsumerBase<EtwConsumer> {
 public:
  EtwConsumer() {
    set_delegate(NULL);
  }

  ~EtwConsumer() {
    set_delegate(NULL);
  }

  void set_delegate(TracedEvents* delegate) {
    delegate_ = delegate;
  }

  static void ProcessEvent(EVENT_TRACE* event) {
    DCHECK(delegate_);
    if (event->Header.Guid != base::debug::kTraceEventClass32)
      return;
    if (event->Header.Class.Version != 0)
      return;

    switch (event->Header.Class.Type) {
      case base::debug::kTraceEventTypeBegin:
        delegate_->OnTraceEventBegin(event);
        break;
      case base::debug::kTraceEventTypeEnd:
        delegate_->OnTraceEventEnd(event);
        break;
      case base::debug::kTraceEventTypeInstant:
        delegate_->OnTraceEventInstant(event);
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  static TracedEvents* delegate_;
};

TracedEvents* EtwConsumer::delegate_ = NULL;
};  // namespace

class EtwPerfSession {
 public:
  EtwPerfSession() {
  }

  ~EtwPerfSession() {
    file_util::Delete(etl_log_file_, false);
  }

  void Start() {
    // To ensure there is no session leftover from crashes, previous runs, etc.
    base::win::EtwTraceProperties ignore;
    base::win::EtwTraceController::Stop(L"cf_perf", &ignore);
    ASSERT_TRUE(file_util::CreateTemporaryFile(&etl_log_file_));
    ASSERT_HRESULT_SUCCEEDED(controller_.StartFileSession(L"cf_perf",
        etl_log_file_.value().c_str(), false));
    ASSERT_HRESULT_SUCCEEDED(controller_.EnableProvider(
        base::debug::kChromeTraceProviderName,
        TRACE_LEVEL_INFORMATION,
        ~(base::debug::CAPTURE_STACK_TRACE)));
  }

  HRESULT Stop() {
    return controller_.Stop(NULL);
  }

  void AnalyzeOutput(TracedEvents* delegate) {
    EtwConsumer consumer;
    consumer.set_delegate(delegate);
    consumer.OpenFileSession(etl_log_file_.value().c_str());
    consumer.Consume();
    consumer.Close();
  }

  FilePath etl_log_file_;
  base::win::EtwTraceController controller_;
};

// Base class for the tracing event helper classes.
class MonitorTraceBase {
 public:
  static bool IsMatchingEvent(EVENT_TRACE* event,
                              const base::StringPiece& event_to_compare) {
    return event->MofLength > event_to_compare.size() &&
      (memcmp(event_to_compare.data(), event->MofData,
              event_to_compare.size() + 1) == 0);
  }

  bool is_valid() const {
    return !start_.is_null() && !end_.is_null() && start_ <= end_;
  }

  base::TimeDelta duration() const {
    return end_ - start_;
  }

  base::Time start_;
  base::Time end_;
};

// This class measures the time between begin and end events of a particular
// type.
class MonitorTracePair : public MonitorTraceBase,
                         public TracedEvents {
 public:
  MonitorTracePair() : event_(NULL) {
  }

  void set_interesting_event(const char* event) {
    event_ = event;
  }

  virtual void OnTraceEventBegin(EVENT_TRACE* event) {
    if (IsMatchingEvent(event, event_)) {
      EXPECT_TRUE(start_.is_null());
      start_ = base::Time::FromFileTime(
          reinterpret_cast<FILETIME&>(event->Header.TimeStamp));
    }
  }

  virtual void OnTraceEventEnd(EVENT_TRACE* event) {
    if (IsMatchingEvent(event, event_)) {
      EXPECT_FALSE(start_.is_null());
      EXPECT_TRUE(end_.is_null());
      end_ = base::Time::FromFileTime(
        reinterpret_cast<FILETIME&>(event->Header.TimeStamp));
    }
  }

  base::StringPiece event_;
};

// This class measures the time between two events.
class MonitorTraceBetweenEventPair : public MonitorTraceBase,
                                     public TracedEvents {
 public:
  MonitorTraceBetweenEventPair() : event_end_(NULL),
                                   event_start_(NULL) {
  }

  void set_start_event(const char* event) {
    event_start_ = event;
  }

  void set_end_event(const char* event) {
    event_end_ = event;
  }

  virtual void OnTraceEventBegin(EVENT_TRACE* event) {
    if (IsMatchingEvent(event, event_start_)) {
      EXPECT_TRUE(start_.is_null());
      EXPECT_TRUE(end_.is_null());
      start_ = base::Time::FromFileTime(
        reinterpret_cast<FILETIME&>(event->Header.TimeStamp));
    } else if (IsMatchingEvent(event, event_end_)) {
      EXPECT_FALSE(start_.is_null());
      EXPECT_TRUE(end_.is_null());
      end_ = base::Time::FromFileTime(
        reinterpret_cast<FILETIME&>(event->Header.TimeStamp));
    }
  }

  virtual void OnTraceEventEnd(EVENT_TRACE* event) {}

  base::StringPiece event_start_;
  base::StringPiece event_end_;
};

// The very same as UIPerfTest::PrintResultXXXX without the need to
// create an UIPerfTest instance.
void PrintResultsImpl(const std::string& measurement,
                      const std::string& modifier,
                      const std::string& trace,
                      const std::string& values,
                      const std::string& prefix,
                      const std::string& suffix,
                      const std::string& units,
                      bool important) {
  // <*>RESULT <graph_name>: <trace_name>= <value> <units>
  // <*>RESULT <graph_name>: <trace_name>= {<mean>, <std deviation>} <units>
  // <*>RESULT <graph_name>: <trace_name>= [<value>,value,value,...,] <units>
  printf("%sRESULT %s%s: %s= %s%s%s %s\n",
         important ? "*" : "", measurement.c_str(), modifier.c_str(),
         trace.c_str(), prefix.c_str(), values.c_str(), suffix.c_str(),
         units.c_str());
}

void PrintResultList(const std::string& measurement,
                     const std::string& modifier,
                     const std::string& trace,
                     const std::string& values,
                     const std::string& units,
                     bool important) {
  PrintResultsImpl(measurement, modifier, trace, values,
      "[", "]", units, important);
}

bool RunSingleTestOutOfProc(const std::string& test_name) {
  FilePath path;
  PathService::Get(base::DIR_EXE, &path);
  path = path.Append(L"chrome_frame_tests.exe");

  CommandLine cmd_line(path);
  // Always enable disabled tests.  This method is not called with disabled
  // tests unless this flag was specified to the browser test executable.
  cmd_line.AppendSwitch("gtest_also_run_disabled_tests");
  cmd_line.AppendSwitchASCII("gtest_filter", test_name);

  base::ProcessHandle process_handle;
  if (!base::LaunchApp(cmd_line, false, false, &process_handle))
    return false;

  int test_terminate_timeout_ms = 60 * 1000;
  int exit_code = 0;
  if (!base::WaitForExitCodeWithTimeout(process_handle, &exit_code,
    test_terminate_timeout_ms)) {
      LOG(ERROR) << "Test timeout (" << test_terminate_timeout_ms
        << " ms) exceeded for " << test_name;

      exit_code = -1;  // Set a non-zero exit code to signal a failure.

      // Ensure that the process terminates.
      base::KillProcess(process_handle, -1, true);
  }

  return exit_code == 0;
}

template <class Monitor>
void PrintPerfTestResults(const Monitor* monitor,
                          int num_cycles,
                          const char* result_name) {
  std::string times;

  for (int i = 0; i < num_cycles; ++i) {
    ASSERT_TRUE(monitor[i].is_valid());
    base::StringAppendF(&times,
                        "%.2f,",
                        monitor[i].duration().InMillisecondsF());
  }

  PrintResultList(result_name, "", "t", times, "ms", false);
}

TEST(TestAsPerfTest, MetaTag_createproxy) {
  const int kNumCycles = 10;

  MonitorTracePair create_proxy_monitor[kNumCycles];
  MonitorTraceBetweenEventPair browser_main_start_monitor[kNumCycles];
  MonitorTraceBetweenEventPair browser_main_loop_monitor[kNumCycles];
  MonitorTraceBetweenEventPair automation_provider_start_monitor[kNumCycles];
  MonitorTraceBetweenEventPair automation_provider_connect_monitor[kNumCycles];
  MonitorTracePair external_tab_navigate_monitor[kNumCycles];
  MonitorTracePair pre_read_chrome_monitor[kNumCycles];
  MonitorTraceBetweenEventPair renderer_main_monitor[kNumCycles];

  for (int i = 0; i < kNumCycles; ++i) {
    EtwPerfSession perf_session;
    ASSERT_NO_FATAL_FAILURE(perf_session.Start());
    ASSERT_TRUE(RunSingleTestOutOfProc(
        "ChromeFrameTestWithWebServer.FullTabModeIE_MetaTag"));
    // Since we cannot have array of objects with a non-default constructor,
    // dedicated method is used to initialize watched event.
    create_proxy_monitor[i].set_interesting_event("chromeframe.createproxy");

    browser_main_start_monitor[i].set_start_event("chromeframe.createproxy");
    browser_main_start_monitor[i].set_end_event("BrowserMain");

    browser_main_loop_monitor[i].set_start_event("BrowserMain");
    browser_main_loop_monitor[i].set_end_event("BrowserMain:MESSAGE_LOOP");

    automation_provider_start_monitor[i].set_start_event("BrowserMain");
    automation_provider_start_monitor[i].set_end_event(
        "AutomationProvider::AutomationProvider");

    automation_provider_connect_monitor[i].set_start_event(
        "AutomationProvider::AutomationProvider");
    automation_provider_connect_monitor[i].set_end_event(
        "AutomationProvider::InitializeChannel");

    external_tab_navigate_monitor[i].set_interesting_event(
        "ExternalTabContainer::Navigate");

    renderer_main_monitor[i].set_start_event("ExternalTabContainer::Navigate");
    renderer_main_monitor[i].set_end_event("RendererMain");

    pre_read_chrome_monitor[i].set_interesting_event("PreReadImage");

    ASSERT_HRESULT_SUCCEEDED(perf_session.Stop());

    perf_session.AnalyzeOutput(&create_proxy_monitor[i]);
    perf_session.AnalyzeOutput(&browser_main_start_monitor[i]);
    perf_session.AnalyzeOutput(&browser_main_loop_monitor[i]);
    perf_session.AnalyzeOutput(&automation_provider_start_monitor[i]);
    perf_session.AnalyzeOutput(&automation_provider_connect_monitor[i]);
    perf_session.AnalyzeOutput(&external_tab_navigate_monitor[i]);
    perf_session.AnalyzeOutput(&pre_read_chrome_monitor[i]);
    perf_session.AnalyzeOutput(&renderer_main_monitor[i]);
  }

  // Print results
  PrintPerfTestResults(create_proxy_monitor, kNumCycles, "createproxy");
  PrintPerfTestResults(browser_main_start_monitor, kNumCycles,
                       "browserstart");
  PrintPerfTestResults(browser_main_loop_monitor, kNumCycles,
                       "browserloop");
  PrintPerfTestResults(automation_provider_start_monitor, kNumCycles,
                       "automationproviderstart");
  PrintPerfTestResults(automation_provider_connect_monitor, kNumCycles,
                       "automationproviderconnect");
  PrintPerfTestResults(external_tab_navigate_monitor, kNumCycles,
                       "externaltabnavigate");
  PrintPerfTestResults(renderer_main_monitor, kNumCycles,
                       "beginrenderermain");
#ifdef NDEBUG
  PrintPerfTestResults(pre_read_chrome_monitor, kNumCycles, "PreReadImage");
#endif  // NDEBUG
}
