// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// @file
// CeeeExecutor & CeeeExecutorCreator implementation, interfaces to
// execute code in other threads which can be running in other another process.

#ifndef CEEE_IE_PLUGIN_BHO_EXECUTOR_H_
#define CEEE_IE_PLUGIN_BHO_EXECUTOR_H_

#include <atlbase.h>
#include <atlcom.h>
#include <string.h>

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/win/rgs_helper.h"
#include "ceee/ie/plugin/bho/infobar_manager.h"
#include "ceee/ie/plugin/toolband/resource.h"

#include "toolband.h"  // NOLINT

struct IWebBrowser2;
namespace infobar_api {
  class InfobarManager;
};

// The executor creator hooks itself in the destination thread where
// the executor will then be created and register in the CeeeBroker.

// The creator of CeeeExecutors.
class ATL_NO_VTABLE CeeeExecutorCreator
    : public CComObjectRootEx<CComSingleThreadModel>,
      public CComCoClass<CeeeExecutorCreator,
                         &CLSID_CeeeExecutorCreator>,
      public ICeeeExecutorCreator {
 public:
  CeeeExecutorCreator();
  void FinalRelease();

  DECLARE_REGISTRY_RESOURCEID_EX(IDR_EXECUTOR)
  BEGIN_REGISTRY_MAP(CeeeExecutorCreator)
    REGMAP_UUID("CLSID", CLSID_CeeeExecutorCreator)
    REGMAP_RESOURCE("NAME", IDS_CEEE_EXECUTOR_CREATOR_NAME)
    REGMAP_ENTRY("THREADING_MODEL", "Free")
  END_REGISTRY_MAP()

  DECLARE_NOT_AGGREGATABLE(CeeeExecutorCreator)
  BEGIN_COM_MAP(CeeeExecutorCreator)
    COM_INTERFACE_ENTRY(ICeeeExecutorCreator)
  END_COM_MAP()
  DECLARE_PROTECT_FINAL_CONSTRUCT()

  // @name ICeeeExecutorCreator implementation.
  // @{
  STDMETHOD(CreateWindowExecutor)(long thread_id, CeeeWindowHandle window);
  STDMETHOD(Teardown)(long thread_id);
  // @}

 protected:
  // The registered message we use to communicate with the destination thread.
  static const UINT kCreateWindowExecutorMessage;

  // The function that will be hooked in the destination thread.
  // See http://msdn.microsoft.com/en-us/library/ms644981(VS.85).aspx
  // for more details.
  static LRESULT CALLBACK GetMsgProc(int code, WPARAM wparam, LPARAM lparam);

  // We must remember the hook so that we can unhook when we are done.
  HHOOK hook_;

  // We can only work for one thread at a time. Used to validate that the
  // call to ICeeeExecutorCreator::Teardown are balanced to a previous call
  // to ICeeeExecutorCreator::CreateExecutor.
  long current_thread_id_;
};

// Implements an asynchronous call object for ICeeeTabExecutor.
// This is instantiated through the executor's ICallFactory::CreateCall method.
// For asynchronous methods, it posts a window message and processes the
// method completion when the message is dispatched. This is done to avoid
// performing significant operations inside IE, reentrantly during an
// outgoing out-of-apartment call. During those times, IE and the various
// third-party addons performing such outcalls tend to be in a fragile state.
// @note the COM marshaling machinery will instantiate a new one of these for
//      each call it handles, so these are not implemented to cope with reuse.
class AsyncTabCall
    : public CComObjectRootEx<CComSingleThreadModel>,
      public CComCoClass<AsyncTabCall>,
      public CWindowImpl<AsyncTabCall>,
      public AsyncICeeeTabExecutor {
 public:
  DECLARE_GET_CONTROLLING_UNKNOWN()

  BEGIN_COM_MAP(AsyncTabCall)
    COM_INTERFACE_ENTRY(AsyncICeeeTabExecutor)
    COM_INTERFACE_ENTRY_AUTOAGGREGATE(IID_ISynchronize,
                                      synchronize_.p,
                                      CLSID_ManualResetEvent)
  END_COM_MAP()

  // The window message we post to ourselves.
  static const uint32 kExecuteTaskMessage = WM_USER;

  BEGIN_MSG_MAP(AsyncTabCall)
    MSG_WM_CREATE(OnCreate)
    MESSAGE_HANDLER(kExecuteTaskMessage, OnExecuteTask)
  END_MSG_MAP()

  AsyncTabCall();
  ~AsyncTabCall();

  // Creates an initialized instance. Aggregated if outer is non-NULL.
  // @param executor the executor instance we invoke on.
  // @param outer the controlling outer or NULL.
  // @param async_call on success, returns the new AsyncTabCall.
  static HRESULT CreateInitialized(ICeeeTabExecutor* executor,
                                   IUnknown* outer,
                                   IUnknown** async_call);

  static bool ImplementsThreadSafeReferenceCounting() {
    return false;
  }

  // @name AsyncICeeeTabExecutor implementation.
  // @{
  STDMETHOD(Begin_Initialize)(CeeeWindowHandle hwnd);
  STDMETHOD(Finish_Initialize)();
  STDMETHOD(Begin_GetTabInfo)();
  STDMETHOD(Finish_GetTabInfo)(CeeeTabInfo *tab_info);
  STDMETHOD(Begin_Navigate)(BSTR url, long flags, BSTR target);
  STDMETHOD(Finish_Navigate)();
  STDMETHOD(Begin_InsertCode)(BSTR code, BSTR file, BOOL all_frames,
      CeeeTabCodeType type);
  STDMETHOD(Finish_InsertCode)();
  // @}

  // Initialize a newly constructed async tab call.
  // This is public only for unittesting.
  HRESULT Initialize(ICeeeTabExecutor* executor);

 private:
  void NavigateImpl(const CComBSTR& url, long flags, const CComBSTR& target);
  void InsertCodeImpl(const CComBSTR& code,
                      const CComBSTR& file,
                      BOOL all_frames,
                      CeeeTabCodeType type);

  int OnCreate(LPCREATESTRUCT lpCreateStruct);
  LRESULT OnExecuteTask(UINT msg, WPARAM wparam, LPARAM lparam, BOOL& handled);
  virtual void OnFinalMessage(HWND window);
  bool ScheduleTask(Task* task);
  HRESULT Signal();

  CComPtr<IUnknown> synchronize_;
  CComPtr<ICeeeTabExecutor> executor_;

  // The task we've scheduled for execution.
  scoped_ptr<Task> task_;
  HRESULT task_hr_;
};

// The executor object that is instantiated in the destination thread and
// then called to... execute stuff...
class ATL_NO_VTABLE CeeeExecutor
    : public CComObjectRootEx<CComSingleThreadModel>,
      public CComCoClass<CeeeExecutor, &CLSID_CeeeExecutor>,
      public IObjectWithSiteImpl<CeeeExecutor>,
      public ICallFactory,
      public ICeeeWindowExecutor,
      public ICeeeTabExecutor,
      public ICeeeCookieExecutor,
      public ICeeeInfobarExecutor {
 public:
  DECLARE_REGISTRY_RESOURCEID_EX(IDR_EXECUTOR)
  BEGIN_REGISTRY_MAP(CeeeExecutor)
    REGMAP_UUID("CLSID", CLSID_CeeeExecutor)
    REGMAP_RESOURCE("NAME", IDS_CEEE_EXECUTOR_NAME)
    REGMAP_ENTRY("THREADING_MODEL", "Apartment")
  END_REGISTRY_MAP()

  DECLARE_NOT_AGGREGATABLE(CeeeExecutor)
  BEGIN_COM_MAP(CeeeExecutor)
    COM_INTERFACE_ENTRY(IObjectWithSite)
    COM_INTERFACE_ENTRY(ICallFactory)
    COM_INTERFACE_ENTRY(ICeeeWindowExecutor)
    COM_INTERFACE_ENTRY(ICeeeTabExecutor)
    COM_INTERFACE_ENTRY(ICeeeCookieExecutor)
    COM_INTERFACE_ENTRY(ICeeeInfobarExecutor)
  END_COM_MAP()
  DECLARE_PROTECT_FINAL_CONSTRUCT()
  DECLARE_CLASSFACTORY()

  CeeeExecutor();
  ~CeeeExecutor();

  // @name ICallFactory implementation.
  // @{
  STDMETHOD(CreateCall)(REFIID async_iid,
                        IUnknown *outer,
                        REFIID requested_iid,
                        IUnknown **out);
  // @}

  // @name ICeeeWindowExecutor implementation.
  // @{
  STDMETHOD(Initialize)(CeeeWindowHandle hwnd);
  STDMETHOD(GetWindow)(BOOL populate_tabs, CeeeWindowInfo* window_info);
  STDMETHOD(GetTabs)(BSTR* tab_list);
  STDMETHOD(UpdateWindow)(long left, long top, long width, long height,
      CeeeWindowInfo* window_info);
  STDMETHOD(RemoveWindow)();
  STDMETHOD(GetTabIndex)(CeeeWindowHandle tab, long* index);
  STDMETHOD(MoveTab)(CeeeWindowHandle tab, long index);
  STDMETHOD(RemoveTab)(CeeeWindowHandle tab);
  STDMETHOD(SelectTab)(CeeeWindowHandle tab);
  // @}

  // @name ICeeeTabExecutor implementation.
  // @{
  // Initialize was already declared in ICeeeWindowExecutor, so we don't
  // add it here, even if it's part of the interface.
  STDMETHOD(GetTabInfo)(CeeeTabInfo* tab_info);
  STDMETHOD(Navigate)(BSTR url, long flags, BSTR target);
  STDMETHOD(InsertCode)(BSTR code, BSTR file, BOOL all_frames,
                        CeeeTabCodeType type);
  // @}

  // @name ICeeeCookieExecutor implementation.
  // @{
  STDMETHOD(GetCookie)(BSTR url, BSTR name, CeeeCookieInfo* cookie_info);
  STDMETHOD(RegisterCookieStore)();
  STDMETHOD(CookieStoreIsRegistered)();
  // @}

  // @name ICeeeInfobarExecutor implementation.
  // @{
  STDMETHOD(SetExtensionId)(BSTR extension_id);
  STDMETHOD(ShowInfobar)(BSTR url, CeeeWindowHandle* window_handle);
  STDMETHOD(OnTopFrameBeforeNavigate)(BSTR url);
  // @}

 protected:
  // Unittest seam.
  virtual HRESULT CreateTabCall(ICeeeTabExecutor* executor,
                                IUnknown *outer,
                                REFIID riid2,
                                IUnknown **out);

  // Get the IWebBrowser2 interface of the
  // frame event host that was set as our site.
  virtual HRESULT GetWebBrowser(IWebBrowser2** browser);

  // Used via EnumChildWindows to get all tabs.
  static BOOL CALLBACK GetTabsEnumProc(HWND window, LPARAM param);

  // Ensure we're running inside the right thread.
  HRESULT EnsureWindowThread();

  // The HWND of the tab/window we are associated to.
  HWND hwnd_;

  // Extension id.
  std::string extension_id_;

  // Get the value of the cookie with the given name, associated with the given
  // URL. Returns S_FALSE if the cookie does not exist, and returns an error
  // code if something unexpected occurs.
  virtual HRESULT GetCookieValue(BSTR url, BSTR name, BSTR* value);

  // Mainly for unit testing purposes.
  void set_cookie_store_is_registered(bool is_registered);

  // Instance of InfobarManager for the tab associated with the thread to which
  // the executor is attached.
  scoped_ptr<infobar_api::InfobarManager> infobar_manager_;
};

#endif  // CEEE_IE_PLUGIN_BHO_EXECUTOR_H_
