// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CeeeExecutor implementation
//
// We use interfaces named ITabWindowManager and ITabWindow
// (documented at
// http://www.geoffchappell.com/viewer.htm?doc=studies/windows/ie/ieframe/interfaces/itabwindowmanager.htm
// and
// http://www.geoffchappell.com/viewer.htm?doc=studies/windows/ie/ieframe/interfaces/itabwindow.htm)
// to help implement this API.  These are available in IE7, IE8 and
// IE9 (with minor differences between browser versions), so we use a
// wrapper class that takes care of delegating to the available
// interface.
//
// Alternate approach considered: Using the IAccessible interface to find out
// about the order (indexes) of tabs, create new tabs and close tabs in a
// reliable way.  The main drawback was that currently, the only way we've
// found to go from the IAccessible interface to the tab window itself (and
// hence the IWebBrowser2 object) is to match the description
// fetched using IAccessible::get_accDescription(), which contains the title
// and URL of the tab, to the title and URL retrieved via the IWebBrowser2
// object.  This limitation would mean that tab indexes could be incorrect
// when two or more tabs are navigated to the same page (and have the same
// title).

#include "ceee/ie/plugin/bho/executor.h"

#include <atlcomcli.h>
#include <mshtml.h>
#include <wininet.h>

#include <vector>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/window_utils.h"
#include "ceee/common/windows_constants.h"
#include "ceee/ie/common/ie_util.h"
#include "ceee/ie/plugin/bho/frame_event_handler.h"
#include "ceee/ie/plugin/bho/infobar_manager.h"
#include "ceee/ie/plugin/bho/tab_window_manager.h"
#include "ceee/ie/plugin/toolband/toolband_proxy.h"
#include "chrome_frame/utils.h"

#include "broker_lib.h"  // NOLINT

namespace {

// Static per-process variable to indicate whether the process has been
// registered as a cookie store yet.
static bool g_cookie_store_is_registered = false;

// INTERNET_COOKIE_HTTPONLY is only available for IE8 or later, which allows
// Wininet API to read cookies that are marked as HTTPOnly.
#ifndef INTERNET_COOKIE_HTTPONLY
#define INTERNET_COOKIE_HTTPONLY      0x00002000
#endif

// Default maximum height of the infobar. From the experience with the design of
// infobars this value is found to provide enough space and not to be too
// restrictive - for example this is approximately the height of Chrome infobar.
const int kMaxInfobarHeight = 39;
}  // namespace

// The message which will be posted to the destination thread.
const UINT CeeeExecutorCreator::kCreateWindowExecutorMessage =
    ::RegisterWindowMessage(
        L"CeeeExecutor{D91E23A6-1C2E-4984-8528-1F1771004F37}");

CeeeExecutorCreator::CeeeExecutorCreator()
    : current_thread_id_(0), hook_(NULL) {
}

void CeeeExecutorCreator::FinalRelease() {
  if (hook_ != NULL) {
    HRESULT hr = Teardown(current_thread_id_);
    DCHECK(SUCCEEDED(hr)) << "Self-Tearing down. " << com::LogHr(hr);
  }
}

HRESULT CeeeExecutorCreator::CreateWindowExecutor(long thread_id,
                                                  CeeeWindowHandle window) {
  DCHECK_EQ(0, current_thread_id_);
  current_thread_id_ = thread_id;
  // Verify we're a window, not just a tab.
  DCHECK_EQ(window_utils::GetTopLevelParent(reinterpret_cast<HWND>(window)),
            reinterpret_cast<HWND>(window));

  hook_ = ::SetWindowsHookEx(WH_GETMESSAGE, GetMsgProc,
      static_cast<HINSTANCE>(_AtlBaseModule.GetModuleInstance()), thread_id);
  if (hook_ == NULL) {
    LOG(ERROR) << "Couldn't hook into thread: " << thread_id << " " <<
        com::LogWe();
    current_thread_id_ = 0;
    return E_FAIL;
  }

  // We unfortunately can't Send a synchronous message here.
  // If we do, any calls back to the broker fail with the following error:
  // "An outgoing call cannot be made since the application is dispatching an
  //  input synchronous call."
  BOOL success = ::PostThreadMessage(thread_id, kCreateWindowExecutorMessage,
                                     0, static_cast<LPARAM>(window));
  if (success)
    return S_OK;
  else
    return HRESULT_FROM_WIN32(::GetLastError());
}

HRESULT CeeeExecutorCreator::Teardown(long thread_id) {
  if (hook_ != NULL) {
    DCHECK(current_thread_id_ == thread_id);
    current_thread_id_ = 0;
    // Don't return the failure since it may fail when we get called after
    // the destination thread/module we hooked to vanished into thin air.
    BOOL success = ::UnhookWindowsHookEx(hook_);
    LOG_IF(INFO, !success) << "Failed to unhook. " << com::LogWe();
    hook_ = NULL;
  }
  return S_OK;
}

LRESULT CeeeExecutorCreator::GetMsgProc(int code, WPARAM wparam,
                                        LPARAM lparam) {
  if (code == HC_ACTION) {
    MSG* message_data = reinterpret_cast<MSG*>(lparam);
    if (message_data != NULL &&
        message_data->message == kCreateWindowExecutorMessage) {
      // Remove the message from the queue so that we don't get called again
      // while we wait for CoCreateInstance to complete, since COM will run
      // a message loop in there. And some loop don't PM_REMOVE us (some do).
      if (wparam != PM_REMOVE) {
        MSG dummy;
        BOOL success = ::PeekMessage(&dummy, NULL, kCreateWindowExecutorMessage,
                                     kCreateWindowExecutorMessage, PM_REMOVE);
        DCHECK(success) << "Peeking Hook Message. " << com::LogWe();
        // We must return here since we will get called again from within
        // PeekMessage, and with PM_REMOVE this time (so no, we won't
        // infinitely recurse :-), so this ensure that we get called just once.
        return 0;
      }

      // Register the proxy/stubs for the executor.
      // We don't make arrangements to unregister them here as we don't
      // expect we'll ever unload from the process.
      // TODO(siggi@chromium.org): Is it worth arranging for unregistration
      //    in this case?
      if (!RegisterProxyStubs(NULL))
        LOG(ERROR) << "Executor Creator failed to register proxy/stubs";

      CComPtr<ICeeeWindowExecutor> executor;
      HRESULT hr = executor.CoCreateInstance(CLSID_CeeeExecutor);
      LOG_IF(ERROR, FAILED(hr)) << "Failed to create Executor, hr=" <<
          com::LogHr(hr);
      DCHECK(SUCCEEDED(hr)) << "CoCreating Executor. " << com::LogHr(hr);

      if (SUCCEEDED(hr)) {
        CeeeWindowHandle window = static_cast<CeeeWindowHandle>(
            message_data->lParam);
        if (window) {
          hr = executor->Initialize(window);
          LOG_IF(ERROR, FAILED(hr)) << "Failed to create Executor, hr=" <<
              com::LogHr(hr);
          DCHECK(SUCCEEDED(hr)) << "CoCreating Executor. " << com::LogHr(hr);
        }

        CComPtr<ICeeeBrokerRegistrar> broker;
        hr = broker.CoCreateInstance(CLSID_CeeeBroker);
        LOG_IF(ERROR, FAILED(hr)) << "Failed to create broker, hr=" <<
            com::LogHr(hr);
        DCHECK(SUCCEEDED(hr)) << "CoCreating Broker. " << com::LogHr(hr);

        if (SUCCEEDED(hr)) {
          hr = broker->RegisterWindowExecutor(::GetCurrentThreadId(), executor);
          DCHECK(SUCCEEDED(hr)) << "Registering Executor. " << com::LogHr(hr);
        }
      }
      return 0;
    }
  }
  return ::CallNextHookEx(NULL, code, wparam, lparam);
}

AsyncTabCall::AsyncTabCall() : task_hr_(E_PENDING), task_(NULL) {
  VLOG(1) << "AsyncTabCall " << this << " created";
}

AsyncTabCall::~AsyncTabCall() {
  VLOG(1) << "AsyncTabCall " << this << " deleted";
}

HRESULT AsyncTabCall::CreateInitialized(ICeeeTabExecutor* executor,
                                        IUnknown* outer,
                                        IUnknown** async_call) {
  CComPolyObject<AsyncTabCall>* async_tab_call = NULL;
  HRESULT hr = CComPolyObject<AsyncTabCall>::CreateInstance(
      outer, &async_tab_call);

  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create async tab call " << com::LogHr(hr);
    return hr;
  }
  DCHECK(async_tab_call != NULL);

  hr = async_tab_call->m_contained.Initialize(executor);
  if (FAILED(hr)) {
    delete async_tab_call;
    LOG(ERROR) << "Async tab call initialization failed " << com::LogHr(hr);
  }

  return async_tab_call->QueryInterface(async_call);
}

HRESULT AsyncTabCall::Initialize(ICeeeTabExecutor* executor) {
  DCHECK(executor != NULL);
  executor_ = executor;
  return S_OK;
}

STDMETHODIMP AsyncTabCall::Begin_Initialize(CeeeWindowHandle hwnd) {
  DCHECK(task_hr_ == E_PENDING && task_ == NULL);
  task_hr_ = executor_->Initialize(hwnd);
  return Signal();
}

STDMETHODIMP AsyncTabCall::Finish_Initialize() {
  return task_hr_;
}

// A noop function.
static void Noop() {
}

STDMETHODIMP AsyncTabCall::Begin_GetTabInfo() {
  DCHECK(task_hr_ == E_PENDING && task_ == NULL);

  // We do all the work on Finish_GetTabInfo, so schedule only a noop
  // invocation. Alternatively we could schedule NULL here, though that
  // would make it more difficult to distinguish error cases.
  return ScheduleTask(NewRunnableFunction(Noop));
}

STDMETHODIMP AsyncTabCall::Finish_GetTabInfo(CeeeTabInfo *tab_info) {
  return executor_->GetTabInfo(tab_info);
}

STDMETHODIMP AsyncTabCall::Begin_Navigate(BSTR url, long flags, BSTR target) {
  DCHECK(task_hr_ == E_PENDING && task_ == NULL);

  if (!ScheduleTask(NewRunnableMethod(this,
                                      &AsyncTabCall::NavigateImpl,
                                      CComBSTR(url),
                                      flags,
                                      CComBSTR(target)))) {
    LOG(ERROR) << "Failed to schedule navigation task";
    return E_OUTOFMEMORY;
  }

  return S_OK;
}

STDMETHODIMP AsyncTabCall::Finish_Navigate() {
  return task_hr_;
}

STDMETHODIMP AsyncTabCall::Begin_InsertCode(BSTR code,
                                            BSTR file,
                                            BOOL all_frames,
                                            CeeeTabCodeType type) {
  DCHECK(task_hr_ == E_PENDING && task_ == NULL);

  if (!ScheduleTask(NewRunnableMethod(this,
                                      &AsyncTabCall::InsertCodeImpl,
                                      CComBSTR(code),
                                      CComBSTR(file),
                                      all_frames,
                                      type))) {
    LOG(ERROR) << "Failed to schedule insert code task";
    return E_OUTOFMEMORY;
  }

  return S_OK;
}

STDMETHODIMP AsyncTabCall::Finish_InsertCode() {
  return task_hr_;
}

void AsyncTabCall::NavigateImpl(const CComBSTR& url,
                                long flags,
                                const CComBSTR& target) {
  task_hr_ = executor_->Navigate(url, flags, target);
}

void AsyncTabCall::InsertCodeImpl(const CComBSTR& code,
                                  const CComBSTR& file,
                                  BOOL all_frames,
                                  CeeeTabCodeType type) {
  task_hr_ = executor_->InsertCode(code, file, all_frames, type);
}

int AsyncTabCall::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  // Our window maintains a self-reference to our object.
  GetUnknown()->AddRef();
  return 1;
}

LRESULT AsyncTabCall::OnExecuteTask(
    UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled) {
  VLOG(1) << "OnExecuteTask for " << this;

  if (task_.get() != NULL) {
    task_->Run();
    task_.reset();
  }

  // Signal the call done, this'll cause the Finish_ call to execute.
  Signal();

  DestroyWindow();
  return 1;
}

void AsyncTabCall::OnFinalMessage(HWND window) {
  // Release our window's self-reference.
  GetUnknown()->Release();
}

bool AsyncTabCall::ScheduleTask(Task* task) {
  DCHECK(task != NULL);
  DCHECK(m_hWnd == NULL);

  if (Create(HWND_MESSAGE) == NULL) {
    LOG(ERROR) << "Failed to create window";

    delete task;
    return false;
  }
  DCHECK(m_hWnd != NULL);

  // Schedule the task for later by posting a message.
  task_.reset(task);
  PostMessage(kExecuteTaskMessage);

  return true;
}

HRESULT AsyncTabCall::Signal() {
  // We need to explicitly query for ISynchronize, because we're
  // most likely aggregated, and the implementation is on our
  // controlling outer.
  CComPtr<ISynchronize> sync;
  HRESULT hr = GetUnknown()->QueryInterface(&sync);
  if (sync == NULL) {
    LOG(ERROR) << "Failed to retrieve ISynchronize " << com::LogHr(hr);
    return hr;
  }
  DCHECK(sync != NULL);

  hr = sync->Signal();
  if (FAILED(hr))
    LOG(ERROR) << "Failed to signal " << com::LogHr(hr);

  return hr;
}

CeeeExecutor::CeeeExecutor() : hwnd_(NULL) {
}

CeeeExecutor::~CeeeExecutor() {
}

HRESULT CeeeExecutor::CreateTabCall(ICeeeTabExecutor* executor,
                                    IUnknown *outer,
                                    REFIID riid2,
                                    IUnknown **out) {
  CComPtr<IUnknown> tab_call;
  HRESULT hr = AsyncTabCall::CreateInitialized(executor, outer, &tab_call);
  if (SUCCEEDED(hr))
    hr = tab_call->QueryInterface(riid2, reinterpret_cast<void**>(out));

  if (FAILED(hr)) {
    delete tab_call;
    LOG(ERROR) << "Async executor initialization failed " << com::LogHr(hr);
  }

  return hr;
}

STDMETHODIMP CeeeExecutor::CreateCall(REFIID async_iid,
                                      IUnknown *outer,
                                      REFIID requested_iid,
                                      IUnknown **out) {
  DCHECK(outer != NULL);
  DCHECK(out != NULL);
  // Null for safety.
  *out = NULL;

  if (async_iid == IID_AsyncICeeeTabExecutor) {
    return CreateTabCall(this, outer, requested_iid, out);
  } else {
    LOG(ERROR) << "Unexpected IID to CreateCall";
    return E_NOINTERFACE;
  }
}

HRESULT CeeeExecutor::Initialize(CeeeWindowHandle hwnd) {
  DCHECK(hwnd);
  hwnd_ = reinterpret_cast<HWND>(hwnd);

  HRESULT hr = EnsureWindowThread();
  if (FAILED(hr)) {
    return hr;
  }

  // If this is tab window then create the infobar manager.
  // TODO(mad@chromium.org): We are starting to need to have different classes
  // for the different executors.
  // TODO(hansl@chromium.org): We might not need to have an Executor for
  // Infobar. In any case, the construction below should have a reference to
  // a BHO and its EventSender so we don't create Infobars before the tab_id
  // is ready.
  if (window_utils::GetTopLevelParent(hwnd_) != hwnd_)
    infobar_manager_.reset(
        new infobar_api::InfobarManager(hwnd_, new BrokerRpcClient));

  return S_OK;
}

HRESULT CeeeExecutor::GetWebBrowser(IWebBrowser2** browser) {
  DCHECK(browser);
  CComPtr<IFrameEventHandlerHost> frame_handler_host;
  HRESULT hr = GetSite(IID_IFrameEventHandlerHost,
                       reinterpret_cast<void**>(&frame_handler_host));
  if (FAILED(hr)) {
    NOTREACHED() << "No frame event handler host for executor. " <<
        com::LogHr(hr);
    return hr;
  }
  return frame_handler_host->GetTopLevelBrowser(browser);
}

STDMETHODIMP CeeeExecutor::GetWindow(BOOL populate_tabs,
                                     CeeeWindowInfo* window_info) {
  DCHECK(window_info);
  HRESULT hr = EnsureWindowThread();
  if (FAILED(hr)) {
    return hr;
  }

  // Zero the window info.
  ZeroMemory(window_info, sizeof(CeeeWindowInfo));

  // The window we use to represent IE windows is the top-level (parentless)
  // frame for the collection of windows that make up a logical "window" in the
  // sense of the chrome.window.* API. Therefore, compare the provided window
  // with the top-level parent of the current foreground (focused) window to
  // see if the logical window has focus.
  HWND top_level = window_utils::GetTopLevelParent(::GetForegroundWindow());
  window_info->focused = (top_level == hwnd_);

  if (!::GetWindowRect(hwnd_, &window_info->rect)) {
    DWORD we = ::GetLastError();
    DCHECK(false) << "GetWindowRect failed " << com::LogWe(we);
    return HRESULT_FROM_WIN32(we);
  }

  if (populate_tabs) {
    return GetTabs(&window_info->tab_list);
  }

  return S_OK;
}

BOOL CALLBACK CeeeExecutor::GetTabsEnumProc(HWND window, LPARAM param) {
  if (window_utils::IsWindowClass(window, windows::kIeTabWindowClass)) {
    std::vector<HWND>* tab_windows =
        reinterpret_cast<std::vector<HWND>*>(param);
    DCHECK(tab_windows);
    tab_windows->push_back(window);
  }
  return TRUE;
}

STDMETHODIMP CeeeExecutor::GetTabs(BSTR* tab_list) {
  DCHECK(tab_list);
  HRESULT hr = EnsureWindowThread();
  if (FAILED(hr)) {
    return hr;
  }

  scoped_ptr<TabWindowManager> manager;
  hr = CreateTabWindowManager(hwnd_, &manager);
  DCHECK(SUCCEEDED(hr)) << "Failed to initialize TabWindowManager.";
  if (FAILED(hr)) {
    return hr;
  }

  std::vector<HWND> tab_windows;
  ::EnumChildWindows(hwnd_, GetTabsEnumProc,
                     reinterpret_cast<LPARAM>(&tab_windows));
  // We don't DCHECK that we found as many windows as the tab window manager
  // GetCount(), because there are cases where it sees more than we do... :-(
  // When we navigate to a new page, IE8 actually creates a new temporary page
  // (not sure if it always do it, or just in some cases), and the
  // TabWindowManager actually count this temporary tab, even though we can't
  // see it when we enumerate the kIeTabWindowClass windows.
  ListValue tabs_list;
  for (size_t index = 0; index < tab_windows.size(); ++index) {
    HWND tab_window = tab_windows[index];
    long tab_index = -1;
    hr = manager->IndexFromHWND(tab_window, &tab_index);
    if (SUCCEEDED(hr)) {
      tabs_list.Append(Value::CreateIntegerValue(
          reinterpret_cast<int>(tab_window)));
      tabs_list.Append(Value::CreateIntegerValue(static_cast<int>(tab_index)));
    // The tab window may have died by the time we get here.
    // Simply ignore that tab in this case.
    } else if (::IsWindow(tab_window)) {
      // But if it's still alive, then something wrong happened.
      return hr;
    }
  }
  std::string tabs_json;
  base::JSONWriter::Write(&tabs_list, false, &tabs_json);
  *tab_list = ::SysAllocString(CA2W(tabs_json.c_str()));
  if (*tab_list == NULL) {
    return E_OUTOFMEMORY;
  }
  return S_OK;
}

STDMETHODIMP CeeeExecutor::UpdateWindow(
    long left, long top, long width, long height,
    CeeeWindowInfo* window_info) {
  DCHECK(window_info);
  HRESULT hr = EnsureWindowThread();
  if (FAILED(hr)) {
    return hr;
  }

  // Any part of the window rect can optionally be set by the caller.
  // We need the original rect if only some dimensions are set by caller.
  RECT rect = { 0 };
  BOOL success = ::GetWindowRect(hwnd_, &rect);
  DCHECK(success);

  if (left == -1) {
    left = rect.left;
  }
  if (top == -1) {
    top = rect.top;
  }
  if (width == -1) {
    width = rect.right - left;
  }
  if (height == -1) {
    height = rect.bottom - top;
  }

  // In IE8 this would yield ERROR_ACCESS_DENIED when called from another
  // thread/process and protected mode is enabled, because the process owning
  // the frame window is medium integrity. See UIPI in MSDN.
  // So this is why we must do this via an injected executor.
  success = ::MoveWindow(hwnd_, left, top, width, height, TRUE);
  DCHECK(success) << "Failed to move the window to the update rect. " <<
      com::LogWe();
  return GetWindow(FALSE, window_info);
}

STDMETHODIMP CeeeExecutor::RemoveWindow() {
  HRESULT hr = EnsureWindowThread();
  if (FAILED(hr)) {
    return hr;
  }

  scoped_ptr<TabWindowManager> manager;
  hr = CreateTabWindowManager(hwnd_, &manager);
  DCHECK(SUCCEEDED(hr)) << "Failed to initialize TabWindowManager.";
  if (FAILED(hr)) {
    return hr;
  }
  return manager->CloseAllTabs();
}

STDMETHODIMP CeeeExecutor::GetTabInfo(CeeeTabInfo* tab_info) {
  DCHECK(tab_info);
  HRESULT hr = EnsureWindowThread();
  if (FAILED(hr)) {
    return hr;
  }

  // Zero the info.
  ZeroMemory(tab_info, sizeof(CeeeTabInfo));

  CComPtr<IFrameEventHandlerHost> frame_handler_host;
  hr = GetSite(IID_IFrameEventHandlerHost,
               reinterpret_cast<void**>(&frame_handler_host));
  if (FAILED(hr)) {
    NOTREACHED() << "No frame event handler host for executor. " <<
      com::LogHr(hr);
    return hr;
  }
  READYSTATE ready_state = READYSTATE_UNINITIALIZED;
  hr = frame_handler_host->GetReadyState(&ready_state);
  if (FAILED(hr)) {
    NOTREACHED() << "Can't get ReadyState, Wazzup???. " << com::LogHr(hr);
    return hr;
  }

  tab_info->status = kCeeeTabStatusComplete;
  if (ready_state != READYSTATE_COMPLETE) {
    // Chrome only has two states, so all incomplete states are "loading".
    tab_info->status = kCeeeTabStatusLoading;
  }

  CComPtr<IWebBrowser2> browser;
  hr = frame_handler_host->GetTopLevelBrowser(&browser);
  if (FAILED(hr)) {
    NOTREACHED();
    return hr;
  }

  hr = browser->get_LocationURL(&tab_info->url);
  DCHECK(SUCCEEDED(hr)) << "get_LocationURL()" << com::LogHr(hr);

  hr = browser->get_LocationName(&tab_info->title);
  DCHECK(SUCCEEDED(hr)) << "get_LocationName()" << com::LogHr(hr);

  // TODO(mad@chromium.org): Favicon support (see Chrome
  // implementation, kFavIconUrlKey).  AFAJoiCT, this is only set if
  // there is a <link rel="icon" ...> tag, so we could parse this out
  // of the IHTMLDocument2::get_links() collection.
  tab_info->fav_icon_url = NULL;
  bool is_protected_mode = false;
  HRESULT protected_mode_hr =
      ie_util::GetIEIsInProtectedMode(&is_protected_mode);
  if (SUCCEEDED(protected_mode_hr)) {
    tab_info->protected_mode = is_protected_mode ? TRUE : FALSE;
  }
  return S_OK;
}

STDMETHODIMP CeeeExecutor::GetTabIndex(CeeeWindowHandle tab, long* index) {
  DCHECK(index);
  HRESULT hr = EnsureWindowThread();
  if (FAILED(hr)) {
    return hr;
  }

  scoped_ptr<TabWindowManager> manager;
  hr = CreateTabWindowManager(hwnd_, &manager);
  DCHECK(SUCCEEDED(hr)) << "Failed to initialize TabWindowManager.";
  if (FAILED(hr)) {
    return hr;
  }

  hr = manager->IndexFromHWND(reinterpret_cast<HWND>(tab), index);
  DCHECK(SUCCEEDED(hr)) << "Couldn't get index for tab: " <<
      tab << ", " << com::LogHr(hr);
  return hr;
}

STDMETHODIMP CeeeExecutor::MoveTab(CeeeWindowHandle tab, long index) {
  HRESULT hr = EnsureWindowThread();
  if (FAILED(hr)) {
    return hr;
  }

  scoped_ptr<TabWindowManager> manager;
  hr = CreateTabWindowManager(hwnd_, &manager);
  DCHECK(SUCCEEDED(hr)) << "Failed to initialize TabWindowManager.";
  if (FAILED(hr)) {
    return hr;
  }

  long src_index = 0;
  hr = manager->IndexFromHWND(reinterpret_cast<HWND>(tab), &src_index);
  if (FAILED(hr)) {
    NOTREACHED() << "Failed IndexFromHWND " << com::LogHr(hr);
    return hr;
  }

  LONG num_tabs = -1;
  hr = manager->GetCount(&num_tabs);
  if (FAILED(hr)) {
    NOTREACHED() << "Failed to GetCount " << com::LogHr(hr);
    return hr;
  }

  // Clamp new index (as per Chrome implementation) so that extension authors
  // can for convenience sakes use index=999 (or some such) to move the tab
  // to the far right.
  if (index >= num_tabs) {
    index = num_tabs - 1;
  }

  // Clamp current index so we can move newly-created tabs easily.
  if (src_index >= num_tabs) {
    src_index = num_tabs - 1;
  }

  if (index == src_index)
    return S_FALSE;  // nothing to be done

  scoped_ptr<TabWindow> dest_tab;
  hr = manager->GetItemWrapper(index, &dest_tab);
  if (FAILED(hr)) {
    NOTREACHED() << "Failed GetItem or QI on dest tab " << com::LogHr(hr);
    return hr;
  }

  long dest_id = -1;
  hr = dest_tab->GetID(&dest_id);
  if (FAILED(hr)) {
    NOTREACHED() << "Failed GetID on dest tab " << com::LogHr(hr);
    return hr;
  }

  scoped_ptr<TabWindow> moving_tab;
  hr = manager->GetItemWrapper(src_index, &moving_tab);
  if (FAILED(hr)) {
    NOTREACHED() << "Failed GetItem or QI on moving tab " << com::LogHr(hr);
    return hr;
  }

  long moving_id = -1;
  hr = moving_tab->GetID(&moving_id);
  if (FAILED(hr)) {
    NOTREACHED() << "Failed GetID on moving tab " << com::LogHr(hr);
    return hr;
  }

  hr = manager->RepositionTab(moving_id, dest_id, 0);
  if (FAILED(hr)) {
    NOTREACHED() << "Failed to reposition tab " << com::LogHr(hr);
    return hr;
  }

  return hr;
}

STDMETHODIMP CeeeExecutor::Navigate(BSTR url, long flags, BSTR target) {
  DCHECK(url);
  HRESULT hr = EnsureWindowThread();
  if (FAILED(hr)) {
    return hr;
  }

  CComPtr<IWebBrowser2> tab_browser;
  hr = GetWebBrowser(&tab_browser);
  if (FAILED(hr)) {
    NOTREACHED() << "Failed to get browser " << com::LogHr(hr);
    return hr;
  }

  CComBSTR current_url;
  hr = tab_browser->get_LocationURL(&current_url);
  if (FAILED(hr)) {
    NOTREACHED() << "Failed to get URL " << com::LogHr(hr);
    return hr;
  }

  if (current_url == url &&
      0 != lstrcmpW(L"_blank", com::ToString(target))) {
    LOG(INFO) << "Got update request, but URL & target is unchanged: " << url;
    return S_FALSE;
  }

  hr = tab_browser->Navigate(url, &CComVariant(flags), &CComVariant(target),
                             &CComVariant(), &CComVariant());
  // We don't DCHECK here since there are cases where we get an error
  // 0x800700aa "The requested resource is in use." if the main UI
  // thread is currently blocked... and sometimes... it is blocked by
  // us... if we are too slow to respond (e.g. because too busy
  // navigating when the user performs an extension action that causes
  // navigation again and again and again)... So we might as well
  // abandon ship and let the UI thread be happy...
  LOG_IF(ERROR, FAILED(hr)) << "Failed to navigate tab: " << hwnd_ <<
       " to " << url << ". " << com::LogHr(hr);
  return hr;
}

STDMETHODIMP CeeeExecutor::RemoveTab(CeeeWindowHandle tab) {
  HRESULT hr = EnsureWindowThread();
  if (FAILED(hr)) {
    return hr;
  }

  scoped_ptr<TabWindowManager> manager;
  hr = CreateTabWindowManager(hwnd_, &manager);
  DCHECK(SUCCEEDED(hr)) << "Failed to initialize TabWindowManager.";
  if (FAILED(hr)) {
    return hr;
  }

  long index = -1;
  hr = manager->IndexFromHWND(reinterpret_cast<HWND>(tab), &index);
  if (FAILED(hr)) {
    NOTREACHED() << "Failed to get index of tab " << com::LogHr(hr);
    return hr;
  }

  scoped_ptr<TabWindow> tab_item;
  hr = manager->GetItemWrapper(index, &tab_item);
  if (FAILED(hr)) {
    NOTREACHED() << "Failed to get tab object " << com::LogHr(hr);
    return hr;
  }

  hr = tab_item->Close();
  DCHECK(SUCCEEDED(hr)) << "Failed to close tab " << com::LogHr(hr);
  return hr;
}

STDMETHODIMP CeeeExecutor::SelectTab(CeeeWindowHandle tab) {
  HRESULT hr = EnsureWindowThread();
  if (FAILED(hr)) {
    return hr;
  }

  scoped_ptr<TabWindowManager> manager;
  hr = CreateTabWindowManager(hwnd_, &manager);
  DCHECK(SUCCEEDED(hr)) << "Failed to initialize TabWindowManager.";
  if (FAILED(hr)) {
    return hr;
  }

  long index = -1;
  hr = manager->IndexFromHWND(reinterpret_cast<HWND>(tab), &index);
  if (FAILED(hr)) {
    NOTREACHED() << "Failed to get index of tab, wnd=" <<
        std::hex << hwnd_ << " " << com::LogHr(hr);
    return hr;
  }

  hr = manager->SelectTab(index);
  DCHECK(SUCCEEDED(hr)) << "Failed to select window, wnd=" <<
      std::hex << hwnd_ << " " << com::LogHr(hr);
  return hr;
}

STDMETHODIMP CeeeExecutor::InsertCode(BSTR code, BSTR file, BOOL all_frames,
                                      CeeeTabCodeType type) {
  HRESULT hr = EnsureWindowThread();
  if (FAILED(hr)) {
    return hr;
  }

  CComPtr<IFrameEventHandlerHost> frame_handler_host;
  hr = GetSite(IID_IFrameEventHandlerHost,
               reinterpret_cast<void**>(&frame_handler_host));
  if (FAILED(hr)) {
    NOTREACHED() << "No frame event handler host for executor. "
                 << com::LogHr(hr);
    return hr;
  }

  hr = frame_handler_host->InsertCode(code, file, all_frames, type);
  if (FAILED(hr)) {
    NOTREACHED() << "Failed to insert code. " << com::LogHr(hr);
    return hr;
  }

  return S_OK;
}

HRESULT CeeeExecutor::GetCookieValue(BSTR url, BSTR name, BSTR* value) {
  DCHECK(value);
  if (!value)
    return E_POINTER;

  // INTERNET_COOKIE_HTTPONLY only works for IE8+.
  DWORD flags = 0;
  if (ie_util::GetIeVersion() > ie_util::IEVERSION_IE7)
    flags |= INTERNET_COOKIE_HTTPONLY;

  // First find out the size of the cookie data.
  DWORD size = 0;
  BOOL cookie_found = ::InternetGetCookieExW(
      url, name, NULL, &size, flags, NULL);
  if (!cookie_found) {
    if (::GetLastError() == ERROR_NO_MORE_ITEMS)
      return S_FALSE;
    else
      return E_FAIL;
  } else if (size == 0) {
    return E_FAIL;
  }

  // Now retrieve the data.
  std::vector<wchar_t> cookie_data(size + 1);
  cookie_found = ::InternetGetCookieExW(
      url, name, &cookie_data[0], &size, flags, NULL);
  DCHECK(cookie_found);
  if (!cookie_found)
    return E_FAIL;

  // Copy the data to the output parameter.
  cookie_data[size] = 0;
  std::wstring cookie_data_string(&cookie_data[0], size);
  std::wstring data_prefix(name);
  data_prefix.append(L"=");
  if (cookie_data_string.find(data_prefix) != 0) {
    DCHECK(false) << "The cookie name or data format does not match the "
                  << "expected 'name=value'. Name: " << name << ", Data: "
                  << cookie_data_string;
    return E_FAIL;
  }
  *value = ::SysAllocString(
      cookie_data_string.substr(data_prefix.size()).c_str());
  return S_OK;
}

STDMETHODIMP CeeeExecutor::GetCookie(BSTR url, BSTR name,
                                     CeeeCookieInfo* cookie_info) {
  DCHECK(cookie_info);
  if (!cookie_info)
    return E_POINTER;
  HRESULT hr = GetCookieValue(url, name, &cookie_info->value);
  if (hr == S_OK) {
    cookie_info->name = ::SysAllocString(name);
  }
  DCHECK(hr == S_OK || cookie_info->value == NULL);
  return hr;
}

void CeeeExecutor::set_cookie_store_is_registered(bool is_registered) {
  g_cookie_store_is_registered = is_registered;
}

STDMETHODIMP CeeeExecutor::RegisterCookieStore() {
  set_cookie_store_is_registered(true);
  return S_OK;
}

HRESULT CeeeExecutor::EnsureWindowThread() {
  if (!window_utils::IsWindowThread(hwnd_)) {
    LOG(ERROR) << "Executor not running in appropriate thread for window: " <<
        hwnd_;
    return E_UNEXPECTED;
  }

  return S_OK;
}

STDMETHODIMP CeeeExecutor::CookieStoreIsRegistered() {
  return g_cookie_store_is_registered ? S_OK : S_FALSE;
}

STDMETHODIMP CeeeExecutor::SetExtensionId(BSTR extension_id) {
  DCHECK(extension_id);
  if (extension_id == NULL)
    return E_FAIL;

  WideToUTF8(extension_id, SysStringLen(extension_id), &extension_id_);
  return S_OK;
}

STDMETHODIMP CeeeExecutor::ShowInfobar(BSTR url,
                                       CeeeWindowHandle* window_handle) {
  DCHECK(infobar_manager_ != NULL) << "infobar_manager_ is not initialized";
  if (infobar_manager_ == NULL)
    return E_FAIL;

  // Consider infobar navigation to an empty url as the request to hide it.
  // Note that this is not a part of the spec so it is up to the implementation
  // how to treat this.
  size_t url_string_length = SysStringLen(url);
  if (0 == url_string_length) {
    if (window_handle != NULL)
      *window_handle = 0;
    infobar_manager_->HideAll();
    return S_OK;
  }

  // Translate relative path to the absolute path using our extension URL
  // as the root.
  std::string url_utf8;
  WideToUTF8(url, url_string_length, &url_utf8);
  if (extension_id_.empty()) {
    LOG(ERROR) << "Extension id is not set before the request to show infobar.";
  } else {
    url_utf8 = ResolveURL(
        StringPrintf("chrome-extension://%s", extension_id_.c_str()), url_utf8);
  }
  std::wstring full_url;
  UTF8ToWide(url_utf8.c_str(), url_utf8.size(), &full_url);

  // Show and navigate the infobar window.
  HRESULT hr = infobar_manager_->Show(infobar_api::TOP_INFOBAR,
                                      kMaxInfobarHeight, full_url, true);
  if (SUCCEEDED(hr) && window_handle != NULL) {
    *window_handle = reinterpret_cast<CeeeWindowHandle>(
        window_utils::GetTopLevelParent(hwnd_));
  }

  return hr;
}

STDMETHODIMP CeeeExecutor::OnTopFrameBeforeNavigate(BSTR url) {
  DCHECK(infobar_manager_ != NULL) << "infobar_manager_ is not initialized";
  if (infobar_manager_ == NULL)
    return E_FAIL;

  // According to the specification, tab navigation closes the infobar.
  infobar_manager_->HideAll();
  return S_OK;
}
