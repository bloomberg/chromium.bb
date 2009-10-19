// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_FRAME_ACTIVEX_BASE_H_
#define CHROME_FRAME_CHROME_FRAME_ACTIVEX_BASE_H_

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>

// Copied min/max defs from windows headers to appease atlimage.h.
// TODO(slightlyoff): Figure out of more recent platform SDK's (> 6.1)
//   undo the janky "#define NOMINMAX" train wreck. See:
// http://connect.microsoft.com/VisualStudio/feedback/ViewFeedback.aspx?FeedbackID=100703
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#include <atlimage.h>
#undef max
#undef min

#include <shdeprecated.h>  // for IBrowserService2
#include <shlguid.h>

#include <set>
#include <string>

#include "base/histogram.h"
#include "base/scoped_bstr_win.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_variant_win.h"
#include "base/string_util.h"
#include "grit/chrome_frame_resources.h"
#include "grit/chrome_frame_strings.h"
#include "chrome_frame/chrome_frame_plugin.h"
#include "chrome_frame/com_type_info_holder.h"
#include "chrome_frame/urlmon_url_request.h"

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
    int connections = m_vec.GetSize();

    for (int connection = 0; connection < connections; ++connection) {
      me->Lock();
      CComPtr<IUnknown> sink(m_vec.GetAt(connection));
      me->Unlock();

      DIChromeFrameEvents* events = static_cast<DIChromeFrameEvents*>(sink.p);
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
            StringPrintf("0x%08X", hr);
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

  void Fire_onreadystatechanged(long readystate) {
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
};

extern bool g_first_launch_by_process_;

// Common implementation for ActiveX and Active Document
template <class T, const CLSID& class_id>
class ATL_NO_VTABLE ChromeFrameActivexBase :
  public CComObjectRootEx<CComSingleThreadModel>,
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
  public IConnectionPointContainerImpl<T>,
  public ProxyDIChromeFrameEvents<T>,
  public IPropertyNotifySinkCP<T>,
  public CComCoClass<T, &class_id>,
  public CComControl<T>,
  public ChromeFramePlugin<T> {
 protected:
  typedef std::set<ScopedComPtr<IDispatch> > EventHandlers;

 public:
  ChromeFrameActivexBase()
      : ready_state_(READYSTATE_UNINITIALIZED) {
    m_bWindowOnly = TRUE;
  }

  ~ChromeFrameActivexBase() {
  }

DECLARE_OLEMISC_STATUS(OLEMISC_RECOMPOSEONRESIZE | OLEMISC_CANTLINKINSIDE |
                       OLEMISC_INSIDEOUT | OLEMISC_ACTIVATEWHENVISIBLE |
                       OLEMISC_SETCLIENTSITEFIRST)

DECLARE_NOT_AGGREGATABLE(T)

BEGIN_COM_MAP(ChromeFrameActivexBase)
  COM_INTERFACE_ENTRY(IChromeFrame)
  COM_INTERFACE_ENTRY(IDispatch)
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
  COM_INTERFACE_ENTRY_FUNC_BLIND(0, InterfaceNotSupported)
END_COM_MAP()

BEGIN_CONNECTION_POINT_MAP(T)
  CONNECTION_POINT_ENTRY(IID_IPropertyNotifySink)
  CONNECTION_POINT_ENTRY(DIID_DIChromeFrameEvents)
END_CONNECTION_POINT_MAP()

BEGIN_MSG_MAP(ChromeFrameActivexBase)
  MESSAGE_HANDLER(WM_CREATE, OnCreate)
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

  HRESULT FinalConstruct() {
    if (!Initialize())
      return E_OUTOFMEMORY;

    // Set to true if this is the first launch by this process.
    // Used to perform one time tasks.
    if (g_first_launch_by_process_) {
      g_first_launch_by_process_ = false;
      UMA_HISTOGRAM_CUSTOM_COUNTS("ChromeFrame.IEVersion",
                                  GetIEVersion(),
                                  IE_INVALID,
                                  IE_8,
                                  IE_8 + 1);
    }
    return S_OK;
  }

  void FinalRelease() {
  }

  static HRESULT WINAPI InterfaceNotSupported(void* pv, REFIID riid, void** ppv,
                                              DWORD dw) {
#ifndef NDEBUG
    wchar_t buffer[64] = {0};
    ::StringFromGUID2(riid, buffer, arraysize(buffer));
    DLOG(INFO) << "E_NOINTERFACE: " << buffer;
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
  virtual HRESULT OnDraw(ATL_DRAWINFO& draw_info) {  // NO_LINT
    if (NULL == draw_info.prcBounds) {
      NOTREACHED();
      return E_FAIL;
    }
    // Don't draw anything.
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

    return CComControlBase::IOleObject_SetClientSite(client_site);
  }

  bool HandleContextMenuCommand(UINT cmd) {
    if (cmd == IDC_ABOUT_CHROME_FRAME) {
      int tab_handle = automation_client_->tab()->handle();
      OnOpenURL(tab_handle, GURL("about:version"), GURL(), NEW_WINDOW);
      return true;
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
  virtual bool is_frame_busting_enabled() const {
    return true;
  }

 protected:
  virtual void OnTabbedOut(int tab_handle, bool reverse) {
    DCHECK(m_bInPlaceActive);

    HWND parent = ::GetParent(m_hWnd);
    ::SetFocus(parent);
    ScopedComPtr<IOleControlSite> control_site;
    control_site.QueryFrom(m_spClientSite);
    if (control_site)
      control_site->OnFocus(FALSE);
  }

  virtual void OnOpenURL(int tab_handle, const GURL& url_to_open,
                         const GURL& referrer, int open_disposition) {
    ScopedComPtr<IWebBrowser2> web_browser2;
    DoQueryService(SID_SWebBrowserApp, m_spClientSite, web_browser2.Receive());
    DCHECK(web_browser2);

    ScopedVariant url;
    // Check to see if the URL uses a "view-source:" prefix, if so, open it
    // using chrome frame full tab mode by using 'cf:' protocol handler.
    // Also change the disposition to NEW_WINDOW since IE6 doesn't have tabs.
    if (url_to_open.has_scheme() && (url_to_open.SchemeIs("view-source") ||
                                     url_to_open.SchemeIs("about"))) {
      std::string chrome_url;
      chrome_url.append("cf:");
      chrome_url.append(url_to_open.spec());
      url.Set(UTF8ToWide(chrome_url).c_str());
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
          open_disposition == NEW_WINDOW) {
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
    VARIANT empty = ScopedVariant::kEmptyVariant;
    ScopedVariant http_headers;

    if (referrer.is_valid()) {
      std::wstring referrer_header = L"Referer: ";
      referrer_header += UTF8ToWide(referrer.spec());
      referrer_header += L"\r\n\r\n";
      http_headers.Set(referrer_header.c_str());
    }

    web_browser2->Navigate2(url.AsInput(), &flags, &empty, &empty,
                            http_headers.AsInput());
    web_browser2->put_Visible(VARIANT_TRUE);
  }

  virtual void OnRequestStart(int tab_handle, int request_id,
                              const IPC::AutomationURLRequest& request_info) {
    scoped_refptr<CComObject<UrlmonUrlRequest> > request;
    if (base_url_request_.get() &&
        GURL(base_url_request_->url()) == GURL(request_info.url)) {
      request.swap(base_url_request_);
    } else {
      CComObject<UrlmonUrlRequest>* new_request = NULL;
      CComObject<UrlmonUrlRequest>::CreateInstance(&new_request);
      request = new_request;
    }

    DCHECK(request.get() != NULL);

    if (request->Initialize(automation_client_.get(), tab_handle, request_id,
                            request_info.url, request_info.method,
                            request_info.referrer,
                            request_info.extra_request_headers,
                            request_info.upload_data.get(),
                            static_cast<T*>(this)->is_frame_busting_enabled())) {
      // If Start is successful, it will add a self reference.
      request->Start();
      request->set_parent_window(m_hWnd);
    }
  }

  virtual void OnRequestRead(int tab_handle, int request_id,
                             int bytes_to_read) {
    automation_client_->ReadRequest(request_id, bytes_to_read);
  }

  virtual void OnRequestEnd(int tab_handle, int request_id,
                            const URLRequestStatus& status) {
    automation_client_->RemoveRequest(request_id, status.status(), true);
  }

  virtual void OnSetCookieAsync(int tab_handle, const GURL& url,
                                const std::string& cookie) {
    std::string name;
    std::string data;
    size_t name_end = cookie.find('=');
    if (std::string::npos != name_end) {
      name = cookie.substr(0, name_end);
      data = cookie.substr(name_end + 1);
    } else {
      data = cookie;
    }

    BOOL ret = InternetSetCookieA(url.spec().c_str(), name.c_str(),
                                  data.c_str());
    DCHECK(ret) << "InternetSetcookie failed. Error: " << GetLastError();
  }

  virtual void OnAttachExternalTab(int tab_handle,
                                   intptr_t cookie,
                                   int disposition) {
    std::string url;
    url = StringPrintf("cf:attach_external_tab&%d&%d",
                       cookie, disposition);
    OnOpenURL(tab_handle, GURL(url), GURL(), disposition);
  }

  LRESULT OnCreate(UINT message, WPARAM wparam, LPARAM lparam,
                   BOOL& handled) {  // NO_LINT
    ModifyStyle(0, WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0);
    automation_client_->SetParentWindow(m_hWnd);
    // Only fire the 'interactive' ready state if we aren't there already.
    if (ready_state_ < READYSTATE_INTERACTIVE) {
      ready_state_ = READYSTATE_INTERACTIVE;
      FireOnChanged(DISPID_READYSTATE);
    }
    return 0;
  }

  // ChromeFrameDelegate override
  virtual void OnAutomationServerReady() {
    ChromeFramePlugin<T>::OnAutomationServerReady();

    ready_state_ = READYSTATE_COMPLETE;
    FireOnChanged(DISPID_READYSTATE);
  }

  // ChromeFrameDelegate override
  virtual void OnAutomationServerLaunchFailed(
      AutomationLaunchResult reason, const std::string& server_version) {
    ready_state_ = READYSTATE_UNINITIALIZED;
    FireOnChanged(DISPID_READYSTATE);
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
    if (full_url.empty()) {
      return E_INVALIDARG;
    }

    // We can initiate navigation here even if ready_state is not complete.
    // We do not have to set proxy, and AutomationClient will take care
    // of navigation just after CreateExternalTab is done.
    if (!automation_client_->InitiateNavigation(full_url,
                                                GetDocumentUrl(),
                                                is_privileged_)) {
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
    DLOG(INFO) << __FUNCTION__;
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
    if (!is_privileged_) {
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

    handlers->insert(ScopedComPtr<IDispatch>(listener));

    return hr;
  }

  STDMETHOD(removeEventListener)(BSTR event_type, IDispatch* listener,
                                 VARIANT use_capture) {
    EventHandlers* handlers = NULL;
    HRESULT hr = GetHandlersForEvent(event_type, &handlers);
    if (FAILED(hr))
      return hr;

    DCHECK(handlers != NULL);
    std::remove(handlers->begin(), handlers->end(), listener);

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

    if (!is_privileged_) {
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

  // Returns the vector of event handlers for a given event (e.g. "load").
  // If the event type isn't recognized, the function fills in a descriptive
  // error (IErrorInfo) and returns E_INVALIDARG.
  HRESULT GetHandlersForEvent(BSTR event_type, EventHandlers** handlers) {
    DCHECK(handlers != NULL);

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
      if (!is_privileged_) {
        Error("Event type 'privatemessage' is privileged");
        return E_ACCESSDENIED;
      }
      *handlers = &onprivatemessage_;
    } else {
      Error(StringPrintf("Event type '%ls' not found", event_type).c_str());
      hr = E_INVALIDARG;
    }

    return hr;
  }

  // Gives the browser a chance to handle an accelerator that was
  // sent to the out of proc chromium instance.
  // Returns S_OK iff the accelerator was handled by the browser.
  HRESULT AllowFrameToTranslateAccelerator(const MSG& msg) {
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

    HRESULT hr = S_FALSE;
    ScopedComPtr<IBrowserService2> bs2;
    if (S_OK == DoQueryService(SID_STopLevelBrowser, m_spInPlaceSite,
                               bs2.Receive())) {
      hr = bs2->v_MayTranslateAccelerator(const_cast<MSG*>(&msg));
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
      // So, for IE8, our approach is very simple.  Just post the message
      // to our parent window and IE will pick it up if it's an
      // accelerator. We won't know for sure if the browser handled the
      // keystroke or not.
      ::PostMessage(::GetParent(m_hWnd), msg.message, msg.wParam,
                    msg.lParam);
    }

    return hr;
  }

  virtual void OnAcceleratorPressed(int tab_handle, const MSG& accel_message) {
    DCHECK(m_spInPlaceSite != NULL);
    // Allow our host a chance to handle the accelerator.
    // This catches things like Ctrl+F, Ctrl+O etc, but not browser
    // accelerators such as F11, Ctrl+T etc.
    // (see AllowFrameToTranslateAccelerator for those).
    HRESULT hr = TranslateAccelerator(const_cast<MSG*>(&accel_message));
    if (hr != S_OK)
      hr = AllowFrameToTranslateAccelerator(accel_message);

    DLOG(INFO) << __FUNCTION__ << " browser response: "
               << StringPrintf("0x%08x", hr);

    if (hr != S_OK) {
      // The WM_SYSCHAR message is not processed by the IOleControlSite
      // implementation and the IBrowserService2::v_MayTranslateAccelerator
      // implementation. We need to understand this better. That is for
      // another day. For now we just post the WM_SYSCHAR message back to our
      // parent which forwards it off to the frame. This should not cause major
      // grief for Chrome as it does not need to handle WM_SYSCHAR accelerators
      // when running in ChromeFrame mode.
      // TODO(iyengar)
      // Understand and fix WM_SYSCHAR handling
      // We should probably unify the accelerator handling for the active
      // document and the activex.
      if (accel_message.message == WM_SYSCHAR) {
        ::PostMessage(GetParent(), WM_SYSCHAR, accel_message.wParam,
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
  ScopedBstr url_;
  ScopedComPtr<IOleDocumentSite> doc_site_;

  // For more information on the ready_state_ property see:
  // http://msdn.microsoft.com/en-us/library/aa768179(VS.85).aspx#
  READYSTATE ready_state_;

  // The following members contain IDispatch interfaces representing the
  // onload/onerror/onmessage handlers on the page.
  ScopedVariant onload_handler_;
  ScopedVariant onerror_handler_;
  ScopedVariant onmessage_handler_;

  EventHandlers onmessage_;
  EventHandlers onloaderror_;
  EventHandlers onload_;
  EventHandlers onreadystatechanged_;
  EventHandlers onprivatemessage_;

  // The UrlmonUrlRequest instance instantiated for downloading the base URL.
  scoped_refptr<CComObject<UrlmonUrlRequest> > base_url_request_;
};

#endif  // CHROME_FRAME_CHROME_FRAME_ACTIVEX_BASE_H_
