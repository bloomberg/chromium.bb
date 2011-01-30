// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_FRAME_ACTIVEX_BASE_H_
#define CHROME_FRAME_CHROME_FRAME_ACTIVEX_BASE_H_

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <wininet.h>
#include <shdeprecated.h>  // for IBrowserService2
#include <shlguid.h>

#include <set>
#include <string>
#include <vector>

#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"
#include "grit/chrome_frame_resources.h"
#include "ceee/ie/common/ceee_util.h"
#include "chrome/common/url_constants.h"
#include "chrome_frame/chrome_frame_plugin.h"
#include "chrome_frame/com_message_event.h"
#include "chrome_frame/com_type_info_holder.h"
#include "chrome_frame/simple_resource_loader.h"
#include "chrome_frame/urlmon_url_request.h"
#include "chrome_frame/urlmon_url_request_private.h"
#include "chrome_frame/utils.h"
#include "grit/generated_resources.h"
#include "net/base/cookie_monster.h"

// Include without path to make GYP build see it.
#include "chrome_tab.h"  // NOLINT

// Connection point class to support firing IChromeFrameEvents (dispinterface).
template<class T>
class ATL_NO_VTABLE ProxyDIChromeFrameEvents
    : public IConnectionPointImpl<T, &DIID_DIChromeFrameEvents> {
 public:
  void FireMethodWithParams(ChromeFrameEventDispId dispid,
                            const VARIANT* params, size_t num_params) {
    T* me = static_cast<T*>(this);
    // We need to copy the whole vector and AddRef the sinks in case
    // some would get disconnected as we fire methods. Note that this is not
    // a threading issue, but a re-entrance issue, because the connection
    // can be affected by the implementation of the sinks receiving the event.
    me->Lock();
    std::vector< base::win::ScopedComPtr<IUnknown> > sink_array(
        m_vec.GetSize());
    for (int connection = 0; connection < m_vec.GetSize(); ++connection)
      sink_array[connection] = m_vec.GetAt(connection);
    me->Unlock();

    for (size_t sink = 0; sink < sink_array.size(); ++sink) {
      DIChromeFrameEvents* events =
          static_cast<DIChromeFrameEvents*>(sink_array[sink].get());
      if (events) {
        DISPPARAMS disp_params = {
            const_cast<VARIANT*>(params),
            NULL,
            num_params,
            0};
        HRESULT hr = events->Invoke(static_cast<DISPID>(dispid),
                                    DIID_DIChromeFrameEvents,
                                    LOCALE_USER_DEFAULT, DISPATCH_METHOD,
                                    &disp_params, NULL, NULL, NULL);
        DLOG_IF(ERROR, FAILED(hr)) << "invoke(" << dispid << ") failed" <<
            base::StringPrintf("0x%08X", hr);
      }
    }
  }

  void FireMethodWithParam(ChromeFrameEventDispId dispid,
                           const VARIANT& param) {
    FireMethodWithParams(dispid, &param, 1);
  }

  void Fire_onload(IDispatch* event) {
    VARIANT var = { VT_DISPATCH };
    var.pdispVal = event;
    FireMethodWithParam(CF_EVENT_DISPID_ONLOAD, var);
  }

  void Fire_onloaderror(IDispatch* event) {
    VARIANT var = { VT_DISPATCH };
    var.pdispVal = event;
    FireMethodWithParam(CF_EVENT_DISPID_ONLOADERROR, var);
  }

  void Fire_onmessage(IDispatch* event) {
    VARIANT var = { VT_DISPATCH };
    var.pdispVal = event;
    FireMethodWithParam(CF_EVENT_DISPID_ONMESSAGE, var);
  }

  void Fire_onreadystatechanged(long readystate) {  // NOLINT
    VARIANT var = { VT_I4 };
    var.lVal = readystate;
    FireMethodWithParam(CF_EVENT_DISPID_ONREADYSTATECHANGED, var);
  }

  void Fire_onprivatemessage(IDispatch* event, BSTR target) {
    // Arguments in reverse order to the function declaration, because
    // that's what DISPPARAMS requires.
    VARIANT args[2] = { { VT_BSTR, }, {VT_DISPATCH, } };
    args[0].bstrVal = target;
    args[1].pdispVal = event;

    FireMethodWithParams(CF_EVENT_DISPID_ONPRIVATEMESSAGE,
                         args,
                         arraysize(args));
  }

  void Fire_onextensionready(BSTR path, long response) {  // NOLINT
    // Arguments in reverse order to the function declaration, because
    // that's what DISPPARAMS requires.
    VARIANT args[2] = { { VT_I4, }, { VT_BSTR, } };
    args[0].lVal = response;
    args[1].bstrVal = path;

    FireMethodWithParams(CF_EVENT_DISPID_ONEXTENSIONREADY,
                         args,
                         arraysize(args));
  }

  void Fire_ongetenabledextensionscomplete(SAFEARRAY* extension_dirs) {
    VARIANT args[1] = { { VT_ARRAY | VT_BSTR } };
    args[0].parray = extension_dirs;

    FireMethodWithParams(CF_EVENT_DISPID_ONGETENABLEDEXTENSIONSCOMPLETE,
                         args, arraysize(args));
  }

  void Fire_onchannelerror() {  // NOLINT
    FireMethodWithParams(CF_EVENT_DISPID_ONCHANNELERROR, NULL, 0);
  }

  void Fire_onclose() {  // NOLINT
    FireMethodWithParams(CF_EVENT_DISPID_ONCLOSE, NULL, 0);
  }
};

extern bool g_first_launch_by_process_;

// Common implementation for ActiveX and Active Document
template <class T, const CLSID& class_id>
class ATL_NO_VTABLE ChromeFrameActivexBase :  // NOLINT
  public CComObjectRootEx<CComMultiThreadModel>,
  public IOleControlImpl<T>,
  public IOleObjectImpl<T>,
  public IOleInPlaceActiveObjectImpl<T>,
  public IViewObjectExImpl<T>,
  public IOleInPlaceObjectWindowlessImpl<T>,
  public ISupportErrorInfo,
  public IQuickActivateImpl<T>,
  public com_util::IProvideClassInfo2Impl<class_id,
                                          DIID_DIChromeFrameEvents>,
  public com_util::IDispatchImpl<IChromeFrame>,
  public IChromeFrameInternal,
  public IConnectionPointContainerImpl<T>,
  public ProxyDIChromeFrameEvents<T>,
  public IPropertyNotifySinkCP<T>,
  public CComCoClass<T, &class_id>,
  public CComControl<T>,
  public ChromeFramePlugin<T> {
 protected:
  typedef std::set<base::win::ScopedComPtr<IDispatch> > EventHandlers;
  typedef ChromeFrameActivexBase<T, class_id> BasePlugin;

 public:
  ChromeFrameActivexBase()
      : ready_state_(READYSTATE_UNINITIALIZED),
      url_fetcher_(new UrlmonUrlRequestManager()),
      failed_to_fetch_in_place_frame_(false),
      draw_sad_tab_(false) {
    m_bWindowOnly = TRUE;
    url_fetcher_->set_container(static_cast<IDispatch*>(this));
  }

  ~ChromeFrameActivexBase() {
    url_fetcher_->set_container(NULL);
  }

DECLARE_OLEMISC_STATUS(OLEMISC_RECOMPOSEONRESIZE | OLEMISC_CANTLINKINSIDE |
                       OLEMISC_INSIDEOUT | OLEMISC_ACTIVATEWHENVISIBLE |
                       OLEMISC_SETCLIENTSITEFIRST)

DECLARE_NOT_AGGREGATABLE(T)

BEGIN_COM_MAP(ChromeFrameActivexBase)
  COM_INTERFACE_ENTRY(IChromeFrame)
  COM_INTERFACE_ENTRY(IDispatch)
  COM_INTERFACE_ENTRY(IChromeFrameInternal)
  COM_INTERFACE_ENTRY(IViewObjectEx)
  COM_INTERFACE_ENTRY(IViewObject2)
  COM_INTERFACE_ENTRY(IViewObject)
  COM_INTERFACE_ENTRY(IOleInPlaceObjectWindowless)
  COM_INTERFACE_ENTRY(IOleInPlaceObject)
  COM_INTERFACE_ENTRY2(IOleWindow, IOleInPlaceObjectWindowless)
  COM_INTERFACE_ENTRY(IOleInPlaceActiveObject)
  COM_INTERFACE_ENTRY(IOleControl)
  COM_INTERFACE_ENTRY(IOleObject)
  COM_INTERFACE_ENTRY(ISupportErrorInfo)
  COM_INTERFACE_ENTRY(IQuickActivate)
  COM_INTERFACE_ENTRY(IProvideClassInfo)
  COM_INTERFACE_ENTRY(IProvideClassInfo2)
  COM_INTERFACE_ENTRY(IConnectionPointContainer)
  COM_INTERFACE_ENTRY_FUNC_BLIND(0, InterfaceNotSupported)
END_COM_MAP()

BEGIN_CONNECTION_POINT_MAP(T)
  CONNECTION_POINT_ENTRY(IID_IPropertyNotifySink)
  CONNECTION_POINT_ENTRY(DIID_DIChromeFrameEvents)
END_CONNECTION_POINT_MAP()

BEGIN_MSG_MAP(ChromeFrameActivexBase)
  MESSAGE_HANDLER(WM_CREATE, OnCreate)
  MESSAGE_HANDLER(WM_DOWNLOAD_IN_HOST, OnDownloadRequestInHost)
  MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
  CHAIN_MSG_MAP(ChromeFramePlugin<T>)
  CHAIN_MSG_MAP(CComControl<T>)
  DEFAULT_REFLECTION_HANDLER()
END_MSG_MAP()

  // IViewObjectEx
  DECLARE_VIEW_STATUS(VIEWSTATUS_SOLIDBKGND | VIEWSTATUS_OPAQUE)

  inline HRESULT IViewObject_Draw(DWORD draw_aspect, LONG index,
      void* aspect_info, DVTARGETDEVICE* ptd, HDC info_dc, HDC dc,
      LPCRECTL bounds, LPCRECTL win_bounds) {
    // ATL ASSERTs if dwDrawAspect is DVASPECT_DOCPRINT, so we cheat.
    DWORD aspect = draw_aspect;
    if (aspect == DVASPECT_DOCPRINT)
      aspect = DVASPECT_CONTENT;

    return CComControl<T>::IViewObject_Draw(aspect, index, aspect_info, ptd,
        info_dc, dc, bounds, win_bounds);
  }

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  void SetResourceModule() {
    SimpleResourceLoader* loader_instance = SimpleResourceLoader::GetInstance();
    DCHECK(loader_instance);
    HMODULE res_dll = loader_instance->GetResourceModuleHandle();
    _AtlBaseModule.SetResourceInstance(res_dll);
  }

  HRESULT FinalConstruct() {
    SetResourceModule();

    if (!Initialize())
      return E_OUTOFMEMORY;

    // Set to true if this is the first launch by this process.
    // Used to perform one time tasks.
    if (g_first_launch_by_process_) {
      g_first_launch_by_process_ = false;
      THREAD_SAFE_UMA_HISTOGRAM_CUSTOM_COUNTS("ChromeFrame.IEVersion",
                                              GetIEVersion(),
                                              IE_INVALID,
                                              IE_9,
                                              IE_9 + 1);
    }

    return S_OK;
  }

  void FinalRelease() {
    Uninitialize();
  }

  void ResetUrlRequestManager() {
    url_fetcher_.reset(new UrlmonUrlRequestManager());
  }

  static HRESULT WINAPI InterfaceNotSupported(void* pv, REFIID riid, void** ppv,
                                              DWORD dw) {
#ifndef NDEBUG
    wchar_t buffer[64] = {0};
    ::StringFromGUID2(riid, buffer, arraysize(buffer));
    DVLOG(1) << "E_NOINTERFACE: " << buffer;
#endif
    return E_NOINTERFACE;
  }

  // ISupportsErrorInfo
  STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid) {
    static const IID* interfaces[] = {
      &IID_IChromeFrame,
      &IID_IDispatch
    };

    for (int i = 0; i < arraysize(interfaces); ++i) {
      if (InlineIsEqualGUID(*interfaces[i], riid))
        return S_OK;
    }
    return S_FALSE;
  }

  // Called to draw our control when chrome hasn't been initialized.
  virtual HRESULT OnDraw(ATL_DRAWINFO& draw_info) {  // NOLINT
    if (NULL == draw_info.prcBounds) {
      NOTREACHED();
      return E_FAIL;
    }

    if (draw_sad_tab_) {
      // TODO(tommi): Draw a proper sad tab.
      RECT rc = {0};
      if (draw_info.prcBounds) {
        rc.top = draw_info.prcBounds->top;
        rc.bottom = draw_info.prcBounds->bottom;
        rc.left = draw_info.prcBounds->left;
        rc.right = draw_info.prcBounds->right;
      } else {
        GetClientRect(&rc);
      }
      ::DrawTextA(draw_info.hdcDraw, ":-(", -1, &rc,
          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
      // Don't draw anything.
    }
    return S_OK;
  }

  // Used to setup the document_url_ member needed for completing navigation.
  // Create external tab (possibly in incognito mode).
  HRESULT IOleObject_SetClientSite(IOleClientSite* client_site) {
    // If we currently have a document site pointer, release it.
    doc_site_.Release();
    if (client_site) {
      doc_site_.QueryFrom(client_site);
    }

    if (client_site == NULL) {
      in_place_frame_.Release();
    }

    return CComControlBase::IOleObject_SetClientSite(client_site);
  }

  bool HandleContextMenuCommand(UINT cmd, const MiniContextMenuParams& params) {
    if (cmd == IDC_ABOUT_CHROME_FRAME) {
      int tab_handle = automation_client_->tab()->handle();
      HostNavigate(GURL("about:version"), GURL(), NEW_WINDOW);
      return true;
    } else {
      switch (cmd) {
        case IDS_CONTENT_CONTEXT_SAVEAUDIOAS:
        case IDS_CONTENT_CONTEXT_SAVEVIDEOAS:
        case IDS_CONTENT_CONTEXT_SAVEIMAGEAS:
        case IDS_CONTENT_CONTEXT_SAVELINKAS: {
          const GURL& referrer = params.frame_url.is_empty() ?
              params.page_url : params.frame_url;
          const GURL& url = (cmd == IDS_CONTENT_CONTEXT_SAVELINKAS ?
              params.link_url : params.src_url);
          DoFileDownloadInIE(UTF8ToWide(url.spec()).c_str());
          return true;
        }
      }
    }

    return false;
  }

  // Should connections initiated by this class try to block
  // responses served with the X-Frame-Options header?
  // ActiveX controls genereally will want to do this,
  // returning true, while true top-level documents
  // (ActiveDocument servers) will not. Your specialization
  // of this template should implement this method based on how
  // it "feels" from a security perspective. If it's hosted in another
  // scriptable document, return true, else false.
  //
  // The base implementation returns true unless we are in privileged
  // mode, in which case we always trust our container so we return false.
  bool is_frame_busting_enabled() const {
    return !is_privileged();
  }

  // Needed to support PostTask.
  static bool ImplementsThreadSafeReferenceCounting() {
    return true;
  }

 protected:
  virtual void GetProfilePath(const std::wstring& profile_name,
                              FilePath* profile_path) {
    bool is_IE = (lstrcmpi(profile_name.c_str(), kIexploreProfileName) == 0) ||
                 (lstrcmpi(profile_name.c_str(), kRundllProfileName) == 0);
    // Browsers without IDeleteBrowsingHistory in non-priv mode
    // have their profiles moved into "Temporary Internet Files".
    //
    // If CEEE is registered, we must have a persistent profile. We
    // considered checking if e.g. ceee_ie.dll is loaded in the process
    // but this gets into edge cases when the user enables the CEEE add-on
    // after CF is first loaded.
    if (is_IE && GetIEVersion() < IE_8 && !ceee_util::IsIeCeeeRegistered()) {
      *profile_path = GetIETemporaryFilesFolder();
      *profile_path = profile_path->Append(L"Google Chrome Frame");
    } else {
      ChromeFramePlugin::GetProfilePath(profile_name, profile_path);
    }
    DVLOG(1) << __FUNCTION__ << ": " << profile_path->value();
  }

  void OnLoad(const GURL& url) {
    if (ready_state_ < READYSTATE_COMPLETE) {
      ready_state_ = READYSTATE_COMPLETE;
      FireOnChanged(DISPID_READYSTATE);
    }

    HRESULT hr = InvokeScriptFunction(onload_handler_, url.spec());
  }

  void OnLoadFailed(int error_code, const std::string& url) {
    HRESULT hr = InvokeScriptFunction(onerror_handler_, url);
  }

  void OnMessageFromChromeFrame(const std::string& message,
                                const std::string& origin,
                                const std::string& target) {
    base::win::ScopedComPtr<IDispatch> message_event;
    if (SUCCEEDED(CreateDomEvent("message", message, origin,
                                 message_event.Receive()))) {
      base::win::ScopedVariant event_var;
      event_var.Set(static_cast<IDispatch*>(message_event));
      InvokeScriptFunction(onmessage_handler_, event_var.AsInput());
    }
  }

  virtual void OnTabbedOut(bool reverse) {
    DCHECK(m_bInPlaceActive);

    HWND parent = ::GetParent(m_hWnd);
    ::SetFocus(parent);
    base::win::ScopedComPtr<IOleControlSite> control_site;
    control_site.QueryFrom(m_spClientSite);
    if (control_site)
      control_site->OnFocus(FALSE);
  }

  virtual void OnOpenURL(const GURL& url_to_open,
                         const GURL& referrer, int open_disposition) {
    HostNavigate(url_to_open, referrer, open_disposition);
  }

  // Called when Chrome has decided that a request needs to be treated as a
  // download.  The caller will be the UrlRequest worker thread.
  // The worker thread will block while we process the request and take
  // ownership of the request object.
  // There's room for improvement here and also see todo below.
  LPARAM OnDownloadRequestInHost(UINT message, WPARAM wparam, LPARAM lparam,
                                 BOOL& handled) {
    base::win::ScopedComPtr<IMoniker> moniker(
        reinterpret_cast<IMoniker*>(lparam));
    DCHECK(moniker);
    base::win::ScopedComPtr<IBindCtx> bind_context(
        reinterpret_cast<IBindCtx*>(wparam));

    // TODO(tommi): It looks like we might have to switch the request object
    // into a pass-through request object and serve up any thus far received
    // content and headers to IE in order to prevent what can currently happen
    // which is reissuing requests and turning POST into GET.
    if (moniker) {
      NavigateBrowserToMoniker(doc_site_, moniker, NULL, bind_context, NULL);
    }

    return TRUE;
  }

  virtual void OnAttachExternalTab(const AttachExternalTabParams& params) {
    std::wstring wide_url = url_;
    GURL parsed_url(WideToUTF8(wide_url));

    // If Chrome-Frame is presently navigated to an extension page, navigating
    // the host to a url with scheme chrome-extension will fail, so we
    // point the host at http:local_host.  Note that this is NOT the URL
    // to which the host is directed.  It is only used as a temporary message
    // passing mechanism between this CF instance, and the BHO that will
    // be constructed in the new IE tab.
    if (parsed_url.SchemeIs("chrome-extension") &&
        is_privileged()) {
      const char kScheme[] = "http";
      const char kHost[] = "local_host";

      GURL::Replacements r;
      r.SetScheme(kScheme, url_parse::Component(0, sizeof(kScheme) -1));
      r.SetHost(kHost, url_parse::Component(0, sizeof(kHost) - 1));
      parsed_url = parsed_url.ReplaceComponents(r);
    }

    std::string url = base::StringPrintf(
        "%hs?attach_external_tab&%I64u&%d&%d&%d&%d&%d&%hs",
        parsed_url.GetOrigin().spec().c_str(),
        params.cookie,
        params.disposition,
        params.dimensions.x(),
        params.dimensions.y(),
        params.dimensions.width(),
        params.dimensions.height(),
        params.profile_name.c_str());
    HostNavigate(GURL(url), GURL(), params.disposition);
  }

  virtual void OnHandleContextMenu(HANDLE menu_handle,
                                   int align_flags,
                                   const MiniContextMenuParams& params) {
    scoped_refptr<BasePlugin> ref(this);
    ChromeFramePlugin<T>::OnHandleContextMenu(menu_handle, align_flags, params);
  }

  LRESULT OnCreate(UINT message, WPARAM wparam, LPARAM lparam,
                   BOOL& handled) {  // NO_LINT
    ModifyStyle(0, WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0);
    url_fetcher_->put_notification_window(m_hWnd);
    if (automation_client_.get()) {
      automation_client_->SetParentWindow(m_hWnd);
    } else {
      NOTREACHED() << "No automation server";
      return -1;
    }
    // Only fire the 'interactive' ready state if we aren't there already.
    if (ready_state_ < READYSTATE_INTERACTIVE) {
      ready_state_ = READYSTATE_INTERACTIVE;
      FireOnChanged(DISPID_READYSTATE);
    }
    return 0;
  }

  LRESULT OnDestroy(UINT message, WPARAM wparam, LPARAM lparam,
                    BOOL& handled) {  // NO_LINT
    DVLOG(1) << __FUNCTION__;
    return 0;
  }

  // ChromeFrameDelegate override
  virtual void OnAutomationServerReady() {
    draw_sad_tab_ = false;
    ChromeFramePlugin<T>::OnAutomationServerReady();

    ready_state_ = READYSTATE_COMPLETE;
    FireOnChanged(DISPID_READYSTATE);
  }

  // ChromeFrameDelegate override
  virtual void OnAutomationServerLaunchFailed(
      AutomationLaunchResult reason, const std::string& server_version) {
    DVLOG(1) << __FUNCTION__;
    if (reason == AUTOMATION_SERVER_CRASHED)
      draw_sad_tab_ = true;

    ready_state_ = READYSTATE_UNINITIALIZED;
    FireOnChanged(DISPID_READYSTATE);
  }

  virtual void OnCloseTab() {
    Fire_onclose();
  }

  // Overridden to take advantage of readystate prop changes and send those
  // to potential listeners.
  HRESULT FireOnChanged(DISPID dispid) {
    if (dispid == DISPID_READYSTATE) {
      Fire_onreadystatechanged(ready_state_);
    }
    return __super::FireOnChanged(dispid);
  }

  // IChromeFrame
  // Property getter/setters for the src attribute, which contains a URL.
  // The ChromeFrameActivex control initiates navigation to this URL
  // when instantiated.
  STDMETHOD(get_src)(BSTR* src) {
    if (NULL == src) {
      return E_POINTER;
    }

    *src = SysAllocString(url_);
    return S_OK;
  }

  STDMETHOD(put_src)(BSTR src) {
    if (src == NULL)
      return E_INVALIDARG;

    // Switch the src to UTF8 and try to expand to full URL
    std::string src_utf8;
    WideToUTF8(src, SysStringLen(src), &src_utf8);
    std::string full_url = ResolveURL(GetDocumentUrl(), src_utf8);

    // We can initiate navigation here even if ready_state is not complete.
    // We do not have to set proxy, and AutomationClient will take care
    // of navigation just after CreateExternalTab is done.
    if (!automation_client_->InitiateNavigation(full_url,
                                                GetDocumentUrl(),
                                                this)) {
      // TODO(robertshield): Make InitiateNavigation return more useful
      // error information.
      return E_INVALIDARG;
    }

    // Save full URL in BSTR member
    url_.Reset(::SysAllocString(UTF8ToWide(full_url).c_str()));

    return S_OK;
  }

  STDMETHOD(get_onload)(VARIANT* onload_handler) {
    if (NULL == onload_handler)
      return E_INVALIDARG;

    *onload_handler = onload_handler_.Copy();

    return S_OK;
  }

  // Property setter for the onload attribute, which contains a
  // javascript function to be invoked on successful navigation.
  STDMETHOD(put_onload)(VARIANT onload_handler) {
    if (V_VT(&onload_handler) != VT_DISPATCH) {
      DLOG(WARNING) << "Invalid onload handler type: "
                    << onload_handler.vt
                    << " specified";
      return E_INVALIDARG;
    }

    onload_handler_ = onload_handler;

    return S_OK;
  }

  // Property getter/setters for the onloaderror attribute, which contains a
  // javascript function to be invoked on navigation failure.
  STDMETHOD(get_onloaderror)(VARIANT* onerror_handler) {
    if (NULL == onerror_handler)
      return E_INVALIDARG;

    *onerror_handler = onerror_handler_.Copy();

    return S_OK;
  }

  STDMETHOD(put_onloaderror)(VARIANT onerror_handler) {
    if (V_VT(&onerror_handler) != VT_DISPATCH) {
      DLOG(WARNING) << "Invalid onloaderror handler type: "
                    << onerror_handler.vt
                    << " specified";
      return E_INVALIDARG;
    }

    onerror_handler_ = onerror_handler;

    return S_OK;
  }

  // Property getter/setters for the onmessage attribute, which contains a
  // javascript function to be invoked when we receive a message from the
  // chrome frame.
  STDMETHOD(put_onmessage)(VARIANT onmessage_handler) {
    if (V_VT(&onmessage_handler) != VT_DISPATCH) {
      DLOG(WARNING) << "Invalid onmessage handler type: "
                    << onmessage_handler.vt
                    << " specified";
      return E_INVALIDARG;
    }

    onmessage_handler_ = onmessage_handler;

    return S_OK;
  }

  STDMETHOD(get_onmessage)(VARIANT* onmessage_handler) {
    if (NULL == onmessage_handler)
      return E_INVALIDARG;

    *onmessage_handler = onmessage_handler_.Copy();

    return S_OK;
  }

  STDMETHOD(get_readyState)(long* ready_state) {  // NOLINT
    DVLOG(1) << __FUNCTION__;
    DCHECK(ready_state);

    if (!ready_state)
      return E_INVALIDARG;

    *ready_state = ready_state_;

    return S_OK;
  }

  // Property getter/setters for use_chrome_network flag. This flag
  // indicates if chrome network stack is to be used for fetching
  // network requests.
  STDMETHOD(get_useChromeNetwork)(VARIANT_BOOL* use_chrome_network) {
    if (!use_chrome_network)
      return E_INVALIDARG;

    *use_chrome_network =
        automation_client_->use_chrome_network() ? VARIANT_TRUE : VARIANT_FALSE;
    return S_OK;
  }

  STDMETHOD(put_useChromeNetwork)(VARIANT_BOOL use_chrome_network) {
    if (!is_privileged()) {
      DLOG(ERROR) << "Attempt to set useChromeNetwork in non-privileged mode";
      return E_ACCESSDENIED;
    }

    automation_client_->set_use_chrome_network(
        (VARIANT_FALSE != use_chrome_network));
    return S_OK;
  }

  // Posts a message to the chrome frame.
  STDMETHOD(postMessage)(BSTR message, VARIANT target) {
    if (NULL == message) {
      return E_INVALIDARG;
    }

    if (!automation_client_.get())
      return E_FAIL;

    std::string utf8_target;
    if (target.vt == VT_BSTR) {
      int len = ::SysStringLen(target.bstrVal);
      if (len == 1 && target.bstrVal[0] == L'*') {
        utf8_target = "*";
      } else {
        GURL resolved(target.bstrVal);
        if (!resolved.is_valid()) {
          Error(L"Unable to parse the specified target URL.");
          return E_INVALIDARG;
        }

        utf8_target = resolved.spec();
      }
    } else {
      utf8_target = "*";
    }

    std::string utf8_message;
    WideToUTF8(message, ::SysStringLen(message), &utf8_message);

    GURL url(GURL(document_url_).GetOrigin());
    std::string origin(url.is_empty() ? "null" : url.spec());
    if (!automation_client_->ForwardMessageFromExternalHost(utf8_message,
                                                            origin,
                                                            utf8_target)) {
      Error(L"Failed to post message to chrome frame");
      return E_FAIL;
    }

    return S_OK;
  }

  STDMETHOD(addEventListener)(BSTR event_type, IDispatch* listener,
                              VARIANT use_capture) {
    EventHandlers* handlers = NULL;
    HRESULT hr = GetHandlersForEvent(event_type, &handlers);
    if (FAILED(hr))
      return hr;

    DCHECK(handlers != NULL);

    handlers->insert(base::win::ScopedComPtr<IDispatch>(listener));

    return hr;
  }

  STDMETHOD(removeEventListener)(BSTR event_type, IDispatch* listener,
                                 VARIANT use_capture) {
    EventHandlers* handlers = NULL;
    HRESULT hr = GetHandlersForEvent(event_type, &handlers);
    if (FAILED(hr))
      return hr;

    DCHECK(handlers != NULL);
    handlers->erase(base::win::ScopedComPtr<IDispatch>(listener));

    return hr;
  }

  STDMETHOD(get_version)(BSTR* version) {
    if (!automation_client_.get()) {
      NOTREACHED();
      return E_FAIL;
    }

    if (version == NULL) {
      return E_INVALIDARG;
    }

    *version = SysAllocString(automation_client_->GetVersion().c_str());
    return S_OK;
  }

  STDMETHOD(postPrivateMessage)(BSTR message, BSTR origin, BSTR target) {
    if (NULL == message)
      return E_INVALIDARG;

    if (!is_privileged()) {
      DLOG(ERROR) << "Attempt to postPrivateMessage in non-privileged mode";
      return E_ACCESSDENIED;
    }

    DCHECK(automation_client_.get());
    std::string utf8_message, utf8_origin, utf8_target;
    WideToUTF8(message, ::SysStringLen(message), &utf8_message);
    WideToUTF8(origin, ::SysStringLen(origin), &utf8_origin);
    WideToUTF8(target, ::SysStringLen(target), &utf8_target);

    if (!automation_client_->ForwardMessageFromExternalHost(utf8_message,
                                                            utf8_origin,
                                                            utf8_target)) {
      Error(L"Failed to post message to chrome frame");
      return E_FAIL;
    }

    return S_OK;
  }

  STDMETHOD(installExtension)(BSTR crx_path) {
    DCHECK(automation_client_.get());

    if (NULL == crx_path) {
      NOTREACHED();
      return E_INVALIDARG;
    }

    if (!is_privileged()) {
      DLOG(ERROR) << "Attempt to installExtension in non-privileged mode";
      return E_ACCESSDENIED;
    }

    FilePath::StringType crx_path_str(crx_path);
    FilePath crx_file_path(crx_path_str);

    automation_client_->InstallExtension(crx_file_path, NULL);
    return S_OK;
  }

  STDMETHOD(loadExtension)(BSTR path) {
    DCHECK(automation_client_.get());

    if (NULL == path) {
      NOTREACHED();
      return E_INVALIDARG;
    }

    if (!is_privileged()) {
      DLOG(ERROR) << "Attempt to loadExtension in non-privileged mode";
      return E_ACCESSDENIED;
    }

    FilePath::StringType path_str(path);
    FilePath file_path(path_str);

    automation_client_->LoadExpandedExtension(file_path, NULL);
    return S_OK;
  }

  STDMETHOD(getEnabledExtensions)() {
    DCHECK(automation_client_.get());

    if (!is_privileged()) {
      DLOG(ERROR) << "Attempt to getEnabledExtensions in non-privileged mode";
      return E_ACCESSDENIED;
    }

    automation_client_->GetEnabledExtensions(NULL);
    return S_OK;
  }

  STDMETHOD(getSessionId)(int* session_id) {
    DCHECK(automation_client_.get());
    DCHECK(session_id);

    if (!is_privileged()) {
      DLOG(ERROR) << "Attempt to getSessionId in non-privileged mode";
      return E_ACCESSDENIED;
    }

    *session_id = automation_client_->GetSessionId();
    return (*session_id) == -1 ? S_FALSE : S_OK;
  }

  STDMETHOD(registerBhoIfNeeded)() {
    return E_NOTIMPL;
  }

  // Returns the vector of event handlers for a given event (e.g. "load").
  // If the event type isn't recognized, the function fills in a descriptive
  // error (IErrorInfo) and returns E_INVALIDARG.
  HRESULT GetHandlersForEvent(BSTR event_type, EventHandlers** handlers) {
    DCHECK(handlers != NULL);

    // TODO(tommi): make these if() statements data-driven.
    HRESULT hr = S_OK;
    const wchar_t* event_type_end = event_type + ::SysStringLen(event_type);
    if (LowerCaseEqualsASCII(event_type, event_type_end, "message")) {
      *handlers = &onmessage_;
    } else if (LowerCaseEqualsASCII(event_type, event_type_end, "load")) {
      *handlers = &onload_;
    } else if (LowerCaseEqualsASCII(event_type, event_type_end, "loaderror")) {
      *handlers = &onloaderror_;
    } else if (LowerCaseEqualsASCII(event_type, event_type_end,
                                    "readystatechanged")) {
      *handlers = &onreadystatechanged_;
    } else if (LowerCaseEqualsASCII(event_type, event_type_end,
                                    "privatemessage")) {
      // This event handler is only available in privileged mode.
      if (is_privileged()) {
        *handlers = &onprivatemessage_;
      } else {
        Error("Event type 'privatemessage' is privileged");
        hr = E_ACCESSDENIED;
      }
    } else if (LowerCaseEqualsASCII(event_type, event_type_end,
                                    "extensionready")) {
      // This event handler is only available in privileged mode.
      if (is_privileged()) {
        *handlers = &onextensionready_;
      } else {
        Error("Event type 'extensionready' is privileged");
        hr = E_ACCESSDENIED;
      }
    } else {
      Error(base::StringPrintf(
          "Event type '%ls' not found", event_type).c_str());
      hr = E_INVALIDARG;
    }

    return hr;
  }

  // Creates a new event object that supports the |data| property.
  // Note: you should supply an empty string for |origin| unless you're
  // creating a "message" event.
  HRESULT CreateDomEvent(const std::string& event_type, const std::string& data,
                         const std::string& origin, IDispatch** event) {
    DCHECK(event_type.length() > 0);  // NOLINT
    DCHECK(event != NULL);

    CComObject<ComMessageEvent>* ev = NULL;
    HRESULT hr = CComObject<ComMessageEvent>::CreateInstance(&ev);
    if (SUCCEEDED(hr)) {
      ev->AddRef();

      base::win::ScopedComPtr<IOleContainer> container;
      m_spClientSite->GetContainer(container.Receive());
      if (ev->Initialize(container, data, origin, event_type)) {
        *event = ev;
      } else {
        NOTREACHED() << "event->Initialize";
        ev->Release();
        hr = E_UNEXPECTED;
      }
    }

    return hr;
  }

  // Helper function to execute a function on a script IDispatch interface.
  HRESULT InvokeScriptFunction(const VARIANT& script_object,
                               const std::string& param) {
    base::win::ScopedVariant script_arg(UTF8ToWide(param.c_str()).c_str());
    return InvokeScriptFunction(script_object, script_arg.AsInput());
  }

  HRESULT InvokeScriptFunction(const VARIANT& script_object, VARIANT* param) {
    return InvokeScriptFunction(script_object, param, 1);
  }

  HRESULT InvokeScriptFunction(const VARIANT& script_object, VARIANT* params,
                               int param_count) {
    DCHECK_GE(param_count, 0);
    DCHECK(params);

    if (V_VT(&script_object) != VT_DISPATCH ||
        script_object.pdispVal == NULL) {
      return S_FALSE;
    }

    CComPtr<IDispatch> script(script_object.pdispVal);
    HRESULT hr = script.InvokeN(static_cast<DISPID>(DISPID_VALUE),
                                params,
                                param_count);
    // 0x80020101 == SCRIPT_E_REPORTED.
    // When the script we're invoking has an error, we get this error back.
    DLOG_IF(ERROR, FAILED(hr) && hr != 0x80020101) << "Failed to invoke script";
    return hr;
  }

  // Gives the browser a chance to handle an accelerator that was
  // sent to the out of proc chromium instance.
  // Returns S_OK iff the accelerator was handled by the browser.
  HRESULT AllowFrameToTranslateAccelerator(const MSG& msg) {
    static const int kMayTranslateAcceleratorOffset = 0x5c;
    // Although IBrowserService2 is officially deprecated, it's still alive
    // and well in IE7 and earlier.  We have to use it here to correctly give
    // the browser a chance to handle keyboard shortcuts.
    // This happens automatically for activex components that have windows that
    // belong to the current thread.  In that circumstance IE owns the message
    // loop and can walk the line of components allowing each participant the
    // chance to handle the keystroke and eventually falls back to
    // v_MayTranslateAccelerator.  However in our case, the message loop is
    // owned by the out-of-proc chromium instance so IE doesn't have a chance to
    // fall back on its default behavior.  Instead we give IE a chance to
    // handle the shortcut here.
    MSG accel_message = msg;
    accel_message.hwnd = ::GetParent(m_hWnd);
    HRESULT hr = S_FALSE;
    base::win::ScopedComPtr<IBrowserService2> bs2;

    // For non-IE containers, we use the standard IOleInPlaceFrame contract
    // (which IE does not support). For IE, we try to use IBrowserService2,
    // but need special handling for IE8 (see below).
    //
    // We try to cache an IOleInPlaceFrame for our site.  If we fail, we don't
    // retry, and we fall back to the IBrowserService2 and PostMessage
    // approaches below.
    if (!in_place_frame_ && !failed_to_fetch_in_place_frame_) {
      base::win::ScopedComPtr<IOleInPlaceUIWindow> dummy_ui_window;
      RECT dummy_pos_rect = {0};
      RECT dummy_clip_rect = {0};
      OLEINPLACEFRAMEINFO dummy_frame_info = {0};
      if (FAILED(m_spInPlaceSite->GetWindowContext(in_place_frame_.Receive(),
                                                   dummy_ui_window.Receive(),
                                                   &dummy_pos_rect,
                                                   &dummy_clip_rect,
                                                   &dummy_frame_info))) {
        failed_to_fetch_in_place_frame_ = true;
      }
    }

    // The IBrowserService2 code below (second conditional) explicitly checks
    // for whether the IBrowserService2::v_MayTranslateAccelerator function is
    // valid. On IE8 there is one vtable ieframe!c_ImpostorBrowserService2Vtbl
    // where this function entry is NULL which leads to a crash. We don't know
    // under what circumstances this vtable is actually used though.
    if (in_place_frame_) {
      hr = in_place_frame_->TranslateAccelerator(&accel_message, 0);
    } else if (S_OK == DoQueryService(
        SID_STopLevelBrowser, m_spInPlaceSite,
        bs2.Receive()) && bs2.get() &&
        *(*(reinterpret_cast<void***>(bs2.get())) +
        kMayTranslateAcceleratorOffset)) {
      hr = bs2->v_MayTranslateAccelerator(&accel_message);
    } else {
      // IE8 doesn't support IBrowserService2 unless you enable a special,
      // undocumented flag with CoInternetSetFeatureEnabled and even then,
      // the object you get back implements only a couple of methods of
      // that interface... all the other entries in the vtable are NULL.
      // In addition, the class that implements it is called
      // ImpostorBrowserService2 :)
      // IE8 does have a new interface though, presumably called
      // ITabBrowserService or something that can be abbreviated to TBS.
      // That interface has a method, TranslateAcceleratorTBS that does
      // call the root MayTranslateAccelerator function, but alas the
      // first argument to MayTranslateAccelerator is hard coded to FALSE
      // which means that global accelerators are not handled and we're
      // out of luck.
      // A third thing that's notable with regards to IE8 is that
      // none of the *MayTranslate* functions exist in a vtable inside
      // ieframe.dll.  I checked this by scanning for the address of
      // those functions inside the dll and found none, which means that
      // all calls to those functions are relative.
      // So, for IE8 in certain cases, and for other containers that may
      // support neither IOleInPlaceFrame or IBrowserService2 our approach
      // is very simple.  Just post the message to our parent window and IE
      // will pick it up if it's an accelerator. We won't know for sure if
      // the browser handled the keystroke or not.
      ::PostMessage(accel_message.hwnd, accel_message.message,
                    accel_message.wParam, accel_message.lParam);
    }

    return hr;
  }

  virtual void OnAcceleratorPressed(const MSG& accel_message) {
    DCHECK(m_spInPlaceSite != NULL);
    // Allow our host a chance to handle the accelerator.
    // This catches things like Ctrl+F, Ctrl+O etc, but not browser
    // accelerators such as F11, Ctrl+T etc.
    // (see AllowFrameToTranslateAccelerator for those).
    HRESULT hr = TranslateAccelerator(const_cast<MSG*>(&accel_message));
    if (hr != S_OK)
      hr = AllowFrameToTranslateAccelerator(accel_message);

    DVLOG(1) << __FUNCTION__ << " browser response: "
             << base::StringPrintf("0x%08x", hr);

    if (hr != S_OK) {
      // The WM_SYSKEYDOWN/WM_SYSKEYUP messages are not processed by the
      // IOleControlSite and IBrowserService2::v_MayTranslateAccelerator
      // implementations. We need to understand this better. That is for
      // another day. For now we just post these messages back to the parent
      // which forwards it off to the frame. This should not cause major
      // grief for Chrome as it does not need to handle WM_SYSKEY* messages in
      // in ChromeFrame mode.
      // TODO(iyengar)
      // Understand and fix WM_SYSCHAR handling
      // We should probably unify the accelerator handling for the active
      // document and the activex.
      if (accel_message.message == WM_SYSCHAR ||
          accel_message.message == WM_SYSKEYDOWN ||
          accel_message.message == WM_SYSKEYUP) {
        ::PostMessage(GetParent(), accel_message.message, accel_message.wParam,
                      accel_message.lParam);
        return;
      }
    }

    // Last chance to handle the keystroke is to pass it to chromium.
    // We do this last partially because there's no way for us to tell if
    // chromium actually handled the keystroke, but also since the browser
    // should have first dibs anyway.
    if (hr != S_OK && automation_client_.get()) {
      TabProxy* tab = automation_client_->tab();
      if (tab) {
        tab->ProcessUnhandledAccelerator(accel_message);
      }
    }
  }

 protected:
  void HostNavigate(const GURL& url_to_open,
                    const GURL& referrer, int open_disposition) {
    base::win::ScopedComPtr<IWebBrowser2> web_browser2;
    DoQueryService(SID_SWebBrowserApp, m_spClientSite, web_browser2.Receive());
    if (!web_browser2) {
      NOTREACHED() << "Failed to retrieve IWebBrowser2 interface";
      return;
    }
    base::win::ScopedVariant url;
    // Check to see if the URL uses a "view-source:" prefix, if so, open it
    // using chrome frame full tab mode by using 'cf:' protocol handler.
    // Also change the disposition to NEW_WINDOW since IE6 doesn't have tabs.
    if (url_to_open.has_scheme() &&
        (url_to_open.SchemeIs(chrome::kViewSourceScheme) ||
        url_to_open.SchemeIs(chrome::kAboutScheme))) {
      std::wstring chrome_url;
      chrome_url.append(kChromeProtocolPrefix);
      chrome_url.append(UTF8ToWide(url_to_open.spec()));
      url.Set(chrome_url.c_str());
      open_disposition = NEW_WINDOW;
    } else {
      url.Set(UTF8ToWide(url_to_open.spec()).c_str());
    }

    VARIANT flags = { VT_I4 };
    V_I4(&flags) = 0;

    IEVersion ie_version = GetIEVersion();
    DCHECK(ie_version != NON_IE && ie_version != IE_UNSUPPORTED);
    // Since IE6 doesn't support tabs, so we just use window instead.
    if (ie_version == IE_6) {
      if (open_disposition == NEW_FOREGROUND_TAB ||
          open_disposition == NEW_BACKGROUND_TAB ||
          open_disposition == NEW_WINDOW ||
          open_disposition == NEW_POPUP) {
        V_I4(&flags) = navOpenInNewWindow;
      } else if (open_disposition != CURRENT_TAB) {
        NOTREACHED() << "Unsupported open disposition in IE6";
      }
    } else {
      switch (open_disposition) {
        case NEW_FOREGROUND_TAB:
          V_I4(&flags) = navOpenInNewTab;
          break;
        case NEW_BACKGROUND_TAB:
          V_I4(&flags) = navOpenInBackgroundTab;
          break;
        case NEW_WINDOW:
        case NEW_POPUP:
          V_I4(&flags) = navOpenInNewWindow;
          break;
        default:
          break;
      }
    }

    // TODO(sanjeevr): The navOpenInNewWindow flag causes IE to open this
    // in a new window ONLY if the user has specified
    // "Always open popups in a new window". Otherwise it opens in a new tab.
    // We need to investigate more and see if we can force IE to display the
    // link in a new window. MSHTML uses the below code to force an open in a
    // new window. But this logic also fails for us. Perhaps this flag is not
    // honoured if the ActiveDoc is not MSHTML.
    // Even the HLNF_DISABLEWINDOWRESTRICTIONS flag did not work.
    // Start of MSHTML-like logic.
    // CComQIPtr<ITargetFramePriv2> target_frame = web_browser2;
    // if (target_frame) {
    //   CComPtr<IUri> uri;
    //   CreateUri(UTF8ToWide(open_url_command->url_.spec()).c_str(),
    //             Uri_CREATE_IE_SETTINGS, 0, &uri);
    //   CComPtr<IBindCtx> bind_ctx;
    //   CreateBindCtx(0, &bind_ctx);
    //   target_frame->AggregatedNavigation2(
    //       HLNF_TRUSTFIRSTDOWNLOAD|HLNF_OPENINNEWWINDOW, bind_ctx, NULL,
    //       L"No_Name", uri, L"");
    // }
    // End of MSHTML-like logic
    VARIANT empty = base::win::ScopedVariant::kEmptyVariant;
    base::win::ScopedVariant http_headers;

    if (referrer.is_valid()) {
      std::wstring referrer_header = L"Referer: ";
      referrer_header += UTF8ToWide(referrer.spec());
      referrer_header += L"\r\n\r\n";
      http_headers.Set(referrer_header.c_str());
    }

    HRESULT hr = web_browser2->Navigate2(url.AsInput(), &flags, &empty, &empty,
                                         http_headers.AsInput());
    // If the current window is a popup window then attempting to open a new
    // tab for the navigation will fail. We attempt to issue the navigation in
    // a new window in this case.
    // http://msdn.microsoft.com/en-us/library/aa752133(v=vs.85).aspx
    if (FAILED(hr) && V_I4(&flags) != navOpenInNewWindow) {
      V_I4(&flags) = navOpenInNewWindow;
      hr = web_browser2->Navigate2(url.AsInput(), &flags, &empty, &empty,
                                   http_headers.AsInput());

      DLOG_IF(ERROR, FAILED(hr))
          << "Navigate2 failed with error: "
          << base::StringPrintf("0x%08X", hr);
    }
  }

  void InitializeAutomationSettings() {
    static const wchar_t kHandleTopLevelRequests[] = L"HandleTopLevelRequests";
    static const wchar_t kUseChromeNetworking[] = L"UseChromeNetworking";

    // Query and assign the top-level-request routing, and host networking
    // settings from the registry.
    bool top_level_requests = GetConfigBool(true, kHandleTopLevelRequests);
    bool chrome_network = GetConfigBool(false, kUseChromeNetworking);
    automation_client_->set_handle_top_level_requests(top_level_requests);
    automation_client_->set_use_chrome_network(chrome_network);
  }

  base::win::ScopedBstr url_;
  base::win::ScopedComPtr<IOleDocumentSite> doc_site_;

  // If false, we tried but failed to fetch an IOleInPlaceFrame from our host.
  // Cached here so we don't try to fetch it every time if we keep failing.
  bool failed_to_fetch_in_place_frame_;
  bool draw_sad_tab_;

  base::win::ScopedComPtr<IOleInPlaceFrame> in_place_frame_;

  // For more information on the ready_state_ property see:
  // http://msdn.microsoft.com/en-us/library/aa768179(VS.85).aspx#
  READYSTATE ready_state_;

  // The following members contain IDispatch interfaces representing the
  // onload/onerror/onmessage handlers on the page.
  base::win::ScopedVariant onload_handler_;
  base::win::ScopedVariant onerror_handler_;
  base::win::ScopedVariant onmessage_handler_;

  EventHandlers onmessage_;
  EventHandlers onloaderror_;
  EventHandlers onload_;
  EventHandlers onreadystatechanged_;
  EventHandlers onprivatemessage_;
  EventHandlers onextensionready_;

  // Handle network requests when host network stack is used. Passed to the
  // automation client on initialization.
  scoped_ptr<UrlmonUrlRequestManager> url_fetcher_;
};

#endif  // CHROME_FRAME_CHROME_FRAME_ACTIVEX_BASE_H_
