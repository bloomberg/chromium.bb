// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// IE ChromeFrameHost implementation.
#ifndef CEEE_IE_COMMON_CHROME_FRAME_HOST_H_
#define CEEE_IE_COMMON_CHROME_FRAME_HOST_H_

#include <atlbase.h>
#include <atlwin.h>
#include <atlcrack.h>
#include <list>

#include "base/basictypes.h"
#include "base/scoped_handle.h"
#include "ceee/common/initializing_coclass.h"
#include "chrome_tab.h"  // NOLINT

// fwd.
struct IAxWinHostWindow;

class IChromeFrameHostEvents: public IUnknown {
 public:
  virtual HRESULT OnCfReadyStateChanged(LONG state) = 0;
  virtual HRESULT OnCfPrivateMessage(BSTR msg, BSTR origin, BSTR target) = 0;
  virtual HRESULT OnCfExtensionReady(BSTR path, int response) = 0;
  virtual HRESULT OnCfGetEnabledExtensionsComplete(SAFEARRAY* base_dirs) = 0;
  virtual HRESULT OnCfGetExtensionApisToAutomate(BSTR* functions_enabled) = 0;
  virtual HRESULT OnCfChannelError() = 0;
};

// This is the interface the chrome frame host presents to its consumers.
extern const GUID IID_IChromeFrameHost;
class IChromeFrameHost: public IUnknown {
 public:
  // Set the name of the profile we want Chrome Frame to use.
  // @param chrome_profile_name The name of the profile to use.
  STDMETHOD_(void, SetChromeProfileName)(
      const wchar_t* chrome_profile_name) = 0;

  // Set the URL we want to navigate Chrome Frame to once it is ready.
  // @param url The URL to navigate to.
  STDMETHOD(SetUrl)(BSTR url) = 0;

  // Creates and initializes our Chrome Frame instance.
  STDMETHOD(StartChromeFrame)() = 0;

  // Posts a message through Chrome Frame, or enqueues it if
  // Chrome Frame is not yet ready and @p queueable is true.
  // @param message The message to post to Chrome Frame.
  // @param target Where we want the message to be posted within Chrome.
  STDMETHOD(PostMessage)(BSTR message,  BSTR target) = 0;

  // Tears down an initialized ChromeFrameHost.
  STDMETHOD(TearDown)() = 0;

  // Sets the event sink for this ChromeFrameHost.
  STDMETHOD_(void, SetEventSink)(IChromeFrameHostEvents* event_sink) = 0;

  // Installs the given extension.  Results come back via
  // IChromeFrameHostEvents::OnCfExtensionReady.
  STDMETHOD(InstallExtension)(BSTR crx_path) = 0;

  // Loads the given exploded extension directory.  Results come back via
  // IChromeFrameHostEvents::OnCfExtensionReady.
  STDMETHOD(LoadExtension)(BSTR extension_dir) = 0;

  // Initiates a request for installed extensions. Results come back via
  // IChromeFrameHostEvents::OnCfGetEnabledExtensionsComplete.
  STDMETHOD(GetEnabledExtensions)() = 0;

  // Retrieves the session_id used by Chrome for the CF tab. Will return S_FALSE
  // if the session id is not yet available.
  // The session_id is the id used for the Tab javascript object.
  STDMETHOD(GetSessionId)(int* session_id) = 0;
};

class ATL_NO_VTABLE ChromeFrameHost
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<ChromeFrameHost>,
      public IServiceProviderImpl<ChromeFrameHost>,
      public IChromeFrameHost,
      public IChromeFramePrivileged,
      public IDispEventSimpleImpl<0,
                                  ChromeFrameHost,
                                  &DIID_DIChromeFrameEvents>,
      public CWindowImpl<ChromeFrameHost> {
 public:
  typedef IDispEventSimpleImpl<0,
                               ChromeFrameHost,
                               &DIID_DIChromeFrameEvents> ChromeFrameEvents;
  ChromeFrameHost();
  ~ChromeFrameHost();

  BEGIN_COM_MAP(ChromeFrameHost)
    COM_INTERFACE_ENTRY(IServiceProvider)
    COM_INTERFACE_ENTRY_IID(IID_IChromeFrameHost, IChromeFrameHost)
    COM_INTERFACE_ENTRY(IChromeFramePrivileged)
  END_COM_MAP()

  BEGIN_SERVICE_MAP(ChromeFrameHost)
    SERVICE_ENTRY(SID_ChromeFramePrivileged)
  END_SERVICE_MAP()

  BEGIN_SINK_MAP(ChromeFrameHost)
    SINK_ENTRY_INFO(0, DIID_DIChromeFrameEvents, CF_EVENT_DISPID_ONLOAD,
                    OnCfLoad, &handler_type_idispatch_)
    SINK_ENTRY_INFO(0, DIID_DIChromeFrameEvents, CF_EVENT_DISPID_ONLOADERROR,
                    OnCfLoadError, &handler_type_idispatch_)
    SINK_ENTRY_INFO(0, DIID_DIChromeFrameEvents, CF_EVENT_DISPID_ONMESSAGE,
                    OnCfMessage, &handler_type_idispatch_)
    SINK_ENTRY_INFO(0, DIID_DIChromeFrameEvents,
                    CF_EVENT_DISPID_ONREADYSTATECHANGED,
                    OnCfReadyStateChanged, &handler_type_long_)
    SINK_ENTRY_INFO(0, DIID_DIChromeFrameEvents,
                    CF_EVENT_DISPID_ONPRIVATEMESSAGE,
                    OnCfPrivateMessage, &handler_type_idispatch_bstr_)
    SINK_ENTRY_INFO(0, DIID_DIChromeFrameEvents,
                    CF_EVENT_DISPID_ONEXTENSIONREADY,
                    OnCfExtensionReady, &handler_type_bstr_i4_)
    SINK_ENTRY_INFO(0, DIID_DIChromeFrameEvents,
                    CF_EVENT_DISPID_ONGETENABLEDEXTENSIONSCOMPLETE,
                    OnCfGetEnabledExtensionsComplete, &handler_type_bstrarray_)
    SINK_ENTRY_INFO(0, DIID_DIChromeFrameEvents,
                    CF_EVENT_DISPID_ONCHANNELERROR,
                    OnCfChannelError, &handler_type_void_)
  END_SINK_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  HRESULT Initialize();

  HRESULT FinalConstruct();
  void FinalRelease();

  BEGIN_MSG_MAP(ChromeFrameHost)
    MSG_WM_CREATE(OnCreate)
  END_MSG_MAP()

  // @name IChromeFramePrivileged implementation.
  // @{
  STDMETHOD(GetWantsPrivileged)(boolean* wants_privileged);
  STDMETHOD(GetChromeProfileName)(BSTR* args);
  STDMETHOD(GetExtensionApisToAutomate)(BSTR* functions_enabled);
  STDMETHOD(ShouldShowVersionMismatchDialog)();
  // @}

  // @name ChromeFrame event handlers
  // @{
  STDMETHOD_(void, OnCfLoad)(IDispatch* event);
  STDMETHOD_(void, OnCfLoadError)(IDispatch* event);
  STDMETHOD_(void, OnCfMessage)(IDispatch* event);
  STDMETHOD_(void, OnCfReadyStateChanged)(LONG state);
  STDMETHOD_(void, OnCfPrivateMessage)(IDispatch *event, BSTR target);
  STDMETHOD_(void, OnCfExtensionReady)(BSTR path, int response);
  STDMETHOD_(void, OnCfGetEnabledExtensionsComplete)(
      SAFEARRAY* extension_directories);
  STDMETHOD_(void, OnCfChannelError)(void);
  // @}

  // @name IChromeFrameHost implementation.
  // @{
  STDMETHOD_(void, SetChromeProfileName)(const wchar_t* chrome_profile_name);
  STDMETHOD(SetUrl)(BSTR url);
  STDMETHOD(StartChromeFrame)();
  STDMETHOD(PostMessage)(BSTR message, BSTR target);
  STDMETHOD(TearDown)();
  STDMETHOD_(void, SetEventSink)(IChromeFrameHostEvents* event_sink);
  STDMETHOD(InstallExtension)(BSTR crx_path);
  STDMETHOD(LoadExtension)(BSTR extension_dir);
  STDMETHOD(GetEnabledExtensions)();
  STDMETHOD(GetSessionId)(int* session_id);
  // @}

 protected:
  virtual HRESULT CreateActiveXHost(IAxWinHostWindow** host);
  virtual HRESULT CreateChromeFrame(IChromeFrame** chrome_frame);

  // Our window maintains a refcount on us for the duration of its lifetime.
  // The self-reference is managed with those two methods.
  virtual void OnFinalMessage(HWND window);
  LRESULT OnCreate(LPCREATESTRUCT lpCreateStruct);

 private:
  struct PostedMessage {
    CComBSTR message;
    CComBSTR target;
  };
  typedef std::list<PostedMessage> PostedMessageList;

  // Set us as site for child.
  HRESULT SetChildSite(IUnknown* child);

  // Our Chrome Frame instance.
  CComPtr<IChromeFrame> chrome_frame_;

  // The Chrome profile we ask to connect with.
  CComBSTR chrome_profile_name_;

  // Messages posted before Chrome Frame has loaded have to be enqueued,
  // to ensure that the toolband extension has propertly loaded and
  // initialized before we attempt to post messages at it.
  // This list stores such messages until Chrome Frame reports the
  // document loaded.
  PostedMessageList posted_messages_;

  // True iff Chrome Frame has reported a document loaded event.
  // TODO(mad@chromium.org): Use a three states variable to take the
  // error case into account.
  bool document_loaded_;

#ifndef NDEBUG
  // We use a cross process event to make sure there is only one chrome frame
  // host that returns ExtensionApisToAutomate... But only needed for a DCHECK.
  ScopedHandle automating_extension_api_;
#endif

  // A cached BSTR for the posted messages origin (which is kAutomationOrigin).
  CComBSTR origin_;

  // Our event sink.
  CComPtr<IChromeFrameHostEvents> event_sink_;

  // Function info objects describing our message handlers.
  // Effectively const but can't make const because of silly ATL macro problem.
  static _ATL_FUNC_INFO handler_type_idispatch_;
  static _ATL_FUNC_INFO handler_type_long_;
  static _ATL_FUNC_INFO handler_type_idispatch_bstr_;
  static _ATL_FUNC_INFO handler_type_idispatch_variantptr_;
  static _ATL_FUNC_INFO handler_type_bstr_i4_;
  static _ATL_FUNC_INFO handler_type_bstrarray_;
  static _ATL_FUNC_INFO handler_type_void_;

  DISALLOW_COPY_AND_ASSIGN(ChromeFrameHost);
};

#endif  // CEEE_IE_COMMON_CHROME_FRAME_HOST_H_
