// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/chrome_frame_activex.h"

#include <wininet.h>

#include <algorithm>
#include <map>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/singleton.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "googleurl/src/gurl.h"
#include "chrome_frame/utils.h"

namespace {

// Class used to maintain a mapping from top-level windows to ChromeFrameActivex
// instances.
class TopLevelWindowMapping {
 public:
  typedef std::vector<HWND> WindowList;

  static TopLevelWindowMapping* GetInstance() {
    return Singleton<TopLevelWindowMapping>::get();
  }

  // Add |cf_window| to the set of windows registered under |top_window|.
  void AddMapping(HWND top_window, HWND cf_window) {
    top_window_map_lock_.Lock();
    top_window_map_[top_window].push_back(cf_window);
    top_window_map_lock_.Unlock();
  }

  // Return the set of Chrome-Frame instances under |window|.
  WindowList GetInstances(HWND window) {
    top_window_map_lock_.Lock();
    WindowList list = top_window_map_[window];
    top_window_map_lock_.Unlock();
    return list;
  }

 private:
  // Constructor is private as this class it to be used as a singleton.
  // See static method instance().
  TopLevelWindowMapping() {}

  friend struct DefaultSingletonTraits<TopLevelWindowMapping>;

  typedef std::map<HWND, WindowList> TopWindowMap;
  TopWindowMap top_window_map_;

  CComAutoCriticalSection top_window_map_lock_;

  DISALLOW_COPY_AND_ASSIGN(TopLevelWindowMapping);
};

// Message pump hook function that monitors for WM_MOVE and WM_MOVING
// messages on a top-level window, and passes notification to the appropriate
// Chrome-Frame instances.
LRESULT CALLBACK TopWindowProc(int code, WPARAM wparam, LPARAM lparam) {
  CWPSTRUCT *info = reinterpret_cast<CWPSTRUCT*>(lparam);
  const UINT &message = info->message;
  const HWND &message_hwnd = info->hwnd;

  switch (message) {
    case WM_MOVE:
    case WM_MOVING: {
      TopLevelWindowMapping::WindowList cf_instances =
          TopLevelWindowMapping::GetInstance()->GetInstances(message_hwnd);
      TopLevelWindowMapping::WindowList::iterator
          iter(cf_instances.begin()), end(cf_instances.end());
      for (;iter != end; ++iter) {
        PostMessage(*iter, WM_HOST_MOVED_NOTIFICATION, NULL, NULL);
      }
      break;
    }
    default:
      break;
  }

  return CallNextHookEx(0, code, wparam, lparam);
}

HHOOK InstallLocalWindowHook(HWND window) {
  if (!window)
    return NULL;

  DWORD proc_thread = ::GetWindowThreadProcessId(window, NULL);
  if (!proc_thread)
    return NULL;

  // Note that this hook is installed as a LOCAL hook.
  return  ::SetWindowsHookEx(WH_CALLWNDPROC,
                             TopWindowProc,
                             NULL,
                             proc_thread);
}

}  // unnamed namespace

namespace chrome_frame {
std::string ActiveXCreateUrl(const GURL& parsed_url,
                             const AttachExternalTabParams& params) {
  return base::StringPrintf(
      "%hs?attach_external_tab&%I64u&%d&%d&%d&%d&%d&%hs",
      parsed_url.GetOrigin().spec().c_str(),
      params.cookie,
      params.disposition,
      params.dimensions.x(),
      params.dimensions.y(),
      params.dimensions.width(),
      params.dimensions.height(),
      params.profile_name.c_str());
}

int GetDisposition(const AttachExternalTabParams& params) {
  return params.disposition;
}

void GetMiniContextMenuData(UINT cmd,
                            const MiniContextMenuParams& params,
                            GURL* referrer,
                            GURL* url) {
  *referrer = params.frame_url.is_empty() ? params.page_url : params.frame_url;
  *url = (cmd == IDS_CONTENT_CONTEXT_SAVELINKAS ?
      params.link_url : params.src_url);
}

}  // namespace chrome_frame

ChromeFrameActivex::ChromeFrameActivex()
    : chrome_wndproc_hook_(NULL) {
  TRACE_EVENT_BEGIN("chromeframe.createactivex", this, "");
}

HRESULT ChromeFrameActivex::FinalConstruct() {
  HRESULT hr = Base::FinalConstruct();
  if (FAILED(hr))
    return hr;

  // No need to call FireOnChanged at this point since nobody will be listening.
  ready_state_ = READYSTATE_LOADING;
  return S_OK;
}

ChromeFrameActivex::~ChromeFrameActivex() {
  // We expect these to be released during a call to SetClientSite(NULL).
  DCHECK_EQ(0u, onmessage_.size());
  DCHECK_EQ(0u, onloaderror_.size());
  DCHECK_EQ(0u, onload_.size());
  DCHECK_EQ(0u, onreadystatechanged_.size());
  DCHECK_EQ(0u, onextensionready_.size());

  if (chrome_wndproc_hook_) {
    BOOL unhook_success = ::UnhookWindowsHookEx(chrome_wndproc_hook_);
    DCHECK(unhook_success);
  }

  // ChromeFramePlugin::Uninitialize()
  Base::Uninitialize();

  TRACE_EVENT_END("chromeframe.createactivex", this, "");
}

LRESULT ChromeFrameActivex::OnCreate(UINT message, WPARAM wparam, LPARAM lparam,
                                     BOOL& handled) {
  Base::OnCreate(message, wparam, lparam, handled);
  // Install the notification hook on the top-level window, so that we can
  // be notified on move events.  Note that the return value is not checked.
  // This hook is installed here, as opposed to during IOleObject_SetClientSite
  // because m_hWnd has not yet been assigned during the SetSite call.
  InstallTopLevelHook(m_spClientSite);
  return 0;
}

LRESULT ChromeFrameActivex::OnHostMoved(UINT message, WPARAM wparam,
                                        LPARAM lparam, BOOL& handled) {
  Base::OnHostMoved();
  return 0;
}

HRESULT ChromeFrameActivex::GetContainingDocument(IHTMLDocument2** doc) {
  ScopedComPtr<IOleContainer> container;
  HRESULT hr = m_spClientSite->GetContainer(container.Receive());
  if (container)
    hr = container.QueryInterface(doc);
  return hr;
}

HRESULT ChromeFrameActivex::GetDocumentWindow(IHTMLWindow2** window) {
  ScopedComPtr<IHTMLDocument2> document;
  HRESULT hr = GetContainingDocument(document.Receive());
  if (document)
    hr = document->get_parentWindow(window);
  return hr;
}

void ChromeFrameActivex::OnLoad(const GURL& gurl) {
  ScopedComPtr<IDispatch> event;
  std::string url = gurl.spec();
  if (SUCCEEDED(CreateDomEvent("event", url, "", event.Receive())))
    Fire_onload(event);

  FireEvent(onload_, url);
  Base::OnLoad(gurl);
}

void ChromeFrameActivex::OnLoadFailed(int error_code, const std::string& url) {
  ScopedComPtr<IDispatch> event;
  if (SUCCEEDED(CreateDomEvent("event", url, "", event.Receive())))
    Fire_onloaderror(event);

  FireEvent(onloaderror_, url);
  Base::OnLoadFailed(error_code, url);
}

void ChromeFrameActivex::OnMessageFromChromeFrame(const std::string& message,
                                                  const std::string& origin,
                                                  const std::string& target) {
  DVLOG(1) << __FUNCTION__;

  if (target.compare("*") != 0) {
    bool drop = true;

    if (is_privileged()) {
      // Forward messages if the control is in privileged mode.
      ScopedComPtr<IDispatch> message_event;
      if (SUCCEEDED(CreateDomEvent("message", message, origin,
                                   message_event.Receive()))) {
        base::win::ScopedBstr target_bstr(UTF8ToWide(target).c_str());
        Fire_onprivatemessage(message_event, target_bstr);

        FireEvent(onprivatemessage_, message_event, target_bstr);
      }
    } else {
      if (HaveSameOrigin(target, document_url_)) {
        drop = false;
      } else {
        DLOG(WARNING) << "Dropping posted message since target doesn't match "
            "the current document's origin. target=" << target;
      }
    }

    if (drop)
      return;
  }

  ScopedComPtr<IDispatch> message_event;
  if (SUCCEEDED(CreateDomEvent("message", message, origin,
                               message_event.Receive()))) {
    Fire_onmessage(message_event);

    FireEvent(onmessage_, message_event);

    base::win::ScopedVariant event_var;
    event_var.Set(static_cast<IDispatch*>(message_event));
    InvokeScriptFunction(onmessage_handler_, event_var.AsInput());
  }
}

bool ChromeFrameActivex::ShouldShowVersionMismatchDialog(
    bool is_privileged,
    IOleClientSite* client_site) {
  if (!is_privileged) {
    return true;
  }

  if (client_site) {
    ScopedComPtr<IChromeFramePrivileged> service;
    HRESULT hr = DoQueryService(SID_ChromeFramePrivileged,
                                client_site,
                                service.Receive());
    if (SUCCEEDED(hr) && service) {
      return (S_FALSE != service->ShouldShowVersionMismatchDialog());
    }
  }

  NOTREACHED();
  return true;
}

void ChromeFrameActivex::OnAutomationServerLaunchFailed(
    AutomationLaunchResult reason, const std::string& server_version) {
  Base::OnAutomationServerLaunchFailed(reason, server_version);

  if (reason == AUTOMATION_VERSION_MISMATCH &&
      ShouldShowVersionMismatchDialog(is_privileged(), m_spClientSite)) {
    THREAD_SAFE_UMA_HISTOGRAM_COUNTS(
        "ChromeFrame.VersionMismatchDisplayed", 1);
    DisplayVersionMismatchWarning(m_hWnd, server_version);
  }
}

void ChromeFrameActivex::OnExtensionInstalled(
    const FilePath& path,
    void* user_data,
    AutomationMsg_ExtensionResponseValues response) {
  base::win::ScopedBstr path_str(path.value().c_str());
  Fire_onextensionready(path_str, response);
}

void ChromeFrameActivex::OnGetEnabledExtensionsComplete(
    void* user_data,
    const std::vector<FilePath>& extension_directories) {
  SAFEARRAY* sa = ::SafeArrayCreateVector(VT_BSTR, 0,
                                          extension_directories.size());
  sa->fFeatures = sa->fFeatures | FADF_BSTR;
  ::SafeArrayLock(sa);

  for (size_t i = 0; i < extension_directories.size(); ++i) {
    LONG index = static_cast<LONG>(i);
    ::SafeArrayPutElement(sa, &index, reinterpret_cast<void*>(
        CComBSTR(extension_directories[i].value().c_str()).Detach()));
  }

  Fire_ongetenabledextensionscomplete(sa);
  ::SafeArrayUnlock(sa);
  ::SafeArrayDestroy(sa);
}

void ChromeFrameActivex::OnChannelError() {
  Fire_onchannelerror();
}

HRESULT ChromeFrameActivex::OnDraw(ATL_DRAWINFO& draw_info) {  // NOLINT
  HRESULT hr = S_OK;
  int dc_type = ::GetObjectType(draw_info.hicTargetDev);
  if (dc_type == OBJ_ENHMETADC) {
    RECT print_bounds = {0};
    print_bounds.left = draw_info.prcBounds->left;
    print_bounds.right = draw_info.prcBounds->right;
    print_bounds.top = draw_info.prcBounds->top;
    print_bounds.bottom = draw_info.prcBounds->bottom;

    automation_client_->Print(draw_info.hdcDraw, print_bounds);
  } else {
    hr = Base::OnDraw(draw_info);
  }

  return hr;
}

STDMETHODIMP ChromeFrameActivex::Load(IPropertyBag* bag, IErrorLog* error_log) {
  DCHECK(bag);

  const wchar_t* event_props[] = {
    (L"onload"),
    (L"onloaderror"),
    (L"onmessage"),
    (L"onreadystatechanged"),
  };

  base::win::ScopedComPtr<IHTMLObjectElement> obj_element;
  GetObjectElement(obj_element.Receive());

  base::win::ScopedBstr object_id;
  GetObjectScriptId(obj_element, object_id.Receive());

  ScopedComPtr<IHTMLElement2> element;
  element.QueryFrom(obj_element);
  HRESULT hr = S_OK;

  for (int i = 0; SUCCEEDED(hr) && i < arraysize(event_props); ++i) {
    base::win::ScopedBstr prop(event_props[i]);
    base::win::ScopedVariant value;
    if (SUCCEEDED(bag->Read(prop, value.Receive(), error_log))) {
      if (value.type() != VT_BSTR ||
          FAILED(hr = CreateScriptBlockForEvent(element, object_id,
                                                V_BSTR(&value), prop))) {
        DLOG(ERROR) << "Failed to create script block for " << prop
                    << base::StringPrintf(L"hr=0x%08X, vt=%i", hr,
                                         value.type());
      } else {
        DVLOG(1) << "script block created for event " << prop
                 << base::StringPrintf(" (0x%08X)", hr) << " connections: " <<
            ProxyDIChromeFrameEvents<ChromeFrameActivex>::m_vec.GetSize();
      }
    } else {
      DVLOG(1) << "event property " << prop << " not in property bag";
    }
  }

  base::win::ScopedVariant src;
  if (SUCCEEDED(bag->Read(base::win::ScopedBstr(L"src"), src.Receive(),
                          error_log))) {
    if (src.type() == VT_BSTR) {
      hr = put_src(V_BSTR(&src));
      DCHECK(hr != E_UNEXPECTED);
    }
  }

  base::win::ScopedVariant use_chrome_network;
  if (SUCCEEDED(bag->Read(base::win::ScopedBstr(L"useChromeNetwork"),
                          use_chrome_network.Receive(), error_log))) {
    VariantChangeType(use_chrome_network.AsInput(),
                      use_chrome_network.AsInput(),
                      0, VT_BOOL);
    if (use_chrome_network.type() == VT_BOOL) {
      hr = put_useChromeNetwork(V_BOOL(&use_chrome_network));
      DCHECK(hr != E_UNEXPECTED);
    }
  }

  DLOG_IF(ERROR, FAILED(hr))
      << base::StringPrintf("Failed to load property bag: 0x%08X", hr);

  return hr;
}

const wchar_t g_activex_insecure_content_error[] = {
    L"data:text/html,<html><body><b>ChromeFrame Security Error<br><br>"
    L"Cannot navigate to HTTP url when document URL is HTTPS</body></html>"};

STDMETHODIMP ChromeFrameActivex::put_src(BSTR src) {
  GURL document_url(GetDocumentUrl());
  if (document_url.SchemeIsSecure()) {
    GURL source_url(src);
    if (!source_url.SchemeIsSecure()) {
      Base::put_src(base::win::ScopedBstr(g_activex_insecure_content_error));
      return E_ACCESSDENIED;
    }
  }
  return Base::put_src(src);
}

HRESULT ChromeFrameActivex::IOleObject_SetClientSite(
    IOleClientSite* client_site) {
  HRESULT hr = Base::IOleObject_SetClientSite(client_site);
  if (FAILED(hr) || !client_site) {
    EventHandlers* handlers[] = {
      &onmessage_,
      &onloaderror_,
      &onload_,
      &onreadystatechanged_,
      &onextensionready_,
    };

    for (int i = 0; i < arraysize(handlers); ++i)
      handlers[i]->clear();

    // Drop privileged mode on uninitialization.
    set_is_privileged(false);
  } else {
    ScopedComPtr<IHTMLDocument2> document;
    GetContainingDocument(document.Receive());
    if (document) {
      base::win::ScopedBstr url;
      if (SUCCEEDED(document->get_URL(url.Receive())))
        WideToUTF8(url, url.Length(), &document_url_);
    }

    // Probe to see whether the host implements the privileged service.
    ScopedComPtr<IChromeFramePrivileged> service;
    HRESULT service_hr = DoQueryService(SID_ChromeFramePrivileged,
                                        m_spClientSite,
                                        service.Receive());
    if (SUCCEEDED(service_hr) && service) {
      // Does the host want privileged mode?
      boolean wants_privileged = false;
      service_hr = service->GetWantsPrivileged(&wants_privileged);

      if (SUCCEEDED(service_hr) && wants_privileged)
        set_is_privileged(true);

      url_fetcher_->set_privileged_mode(is_privileged());
    }

    std::wstring profile_name(GetHostProcessName(false));
    if (is_privileged()) {

      base::win::ScopedBstr automated_functions_arg;
      service_hr = service->GetExtensionApisToAutomate(
          automated_functions_arg.Receive());
      if (S_OK == service_hr && automated_functions_arg) {
        std::string automated_functions(
            WideToASCII(static_cast<BSTR>(automated_functions_arg)));
        functions_enabled_.clear();
        // base::SplitString writes one empty entry for blank strings, so we
        // need this to allow specifying zero automation of API functions.
        if (!automated_functions.empty())
          base::SplitString(automated_functions, ',', &functions_enabled_);
      }

      base::win::ScopedBstr profile_name_arg;
      service_hr = service->GetChromeProfileName(profile_name_arg.Receive());
      if (S_OK == service_hr && profile_name_arg)
        profile_name.assign(profile_name_arg, profile_name_arg.Length());
    }

    std::string utf8_url;
    if (url_.Length()) {
      WideToUTF8(url_, url_.Length(), &utf8_url);
    }

    InitializeAutomationSettings();

    // To avoid http://code.google.com/p/chromium/issues/detail?id=63427,
    // we always pass this flag needed by CEEE. It has no effect on
    // normal CF operation.
    //
    // Extra arguments are passed on verbatim, so we add the -- prefix.
    std::wstring chrome_extra_arguments(L"--");
    chrome_extra_arguments.append(
        ASCIIToWide(switches::kEnableExperimentalExtensionApis));

    url_fetcher_->set_frame_busting(!is_privileged());
    automation_client_->SetUrlFetcher(url_fetcher_.get());
    if (!InitializeAutomation(profile_name, chrome_extra_arguments,
                              IsIEInPrivate(), true, GURL(utf8_url),
                              GURL(), false)) {
      DLOG(ERROR) << "Failed to navigate to url:" << utf8_url;
      return E_FAIL;
    }

    // Log a metric that Chrome Frame is being used in Widget mode
    THREAD_SAFE_UMA_LAUNCH_TYPE_COUNT(RENDERER_TYPE_CHROME_WIDGET);
  }

  return hr;
}

HRESULT ChromeFrameActivex::GetObjectScriptId(IHTMLObjectElement* object_elem,
                                              BSTR* id) {
  DCHECK(object_elem != NULL);
  DCHECK(id != NULL);

  HRESULT hr = E_FAIL;
  if (object_elem) {
    ScopedComPtr<IHTMLElement> elem;
    hr = elem.QueryFrom(object_elem);
    if (elem) {
      hr = elem->get_id(id);
    }
  }

  return hr;
}

HRESULT ChromeFrameActivex::GetObjectElement(IHTMLObjectElement** element) {
  DCHECK(m_spClientSite);
  if (!m_spClientSite)
    return E_UNEXPECTED;

  ScopedComPtr<IOleControlSite> site;
  HRESULT hr = site.QueryFrom(m_spClientSite);
  if (site) {
    ScopedComPtr<IDispatch> disp;
    hr = site->GetExtendedControl(disp.Receive());
    if (disp) {
      hr = disp.QueryInterface(element);
    } else {
      DCHECK(FAILED(hr));
    }
  }

  return hr;
}

HRESULT ChromeFrameActivex::CreateScriptBlockForEvent(
    IHTMLElement2* insert_after, BSTR instance_id, BSTR script,
    BSTR event_name) {
  DCHECK(insert_after);
  DCHECK_GT(::SysStringLen(event_name), 0UL);  // should always have this

  // This might be 0 if not specified in the HTML document.
  if (!::SysStringLen(instance_id)) {
    // TODO(tommi): Should we give ourselves an ID if this happens?
    NOTREACHED() << "Need to handle this";
    return E_INVALIDARG;
  }

  ScopedComPtr<IHTMLDocument2> document;
  HRESULT hr = GetContainingDocument(document.Receive());
  if (SUCCEEDED(hr)) {
    ScopedComPtr<IHTMLElement> element, new_element;
    document->createElement(base::win::ScopedBstr(L"script"),
                            element.Receive());
    if (element) {
      ScopedComPtr<IHTMLScriptElement> script_element;
      if (SUCCEEDED(hr = script_element.QueryFrom(element))) {
        script_element->put_htmlFor(instance_id);
        script_element->put_event(event_name);
        script_element->put_text(script);

        hr = insert_after->insertAdjacentElement(
            base::win::ScopedBstr(L"afterEnd"),
            element,
            new_element.Receive());
      }
    }
  }

  return hr;
}

void ChromeFrameActivex::FireEvent(const EventHandlers& handlers,
                                   const std::string& arg) {
  if (handlers.size()) {
    ScopedComPtr<IDispatch> event;
    if (SUCCEEDED(CreateDomEvent("event", arg, "", event.Receive()))) {
      FireEvent(handlers, event);
    }
  }
}

void ChromeFrameActivex::FireEvent(const EventHandlers& handlers,
                                   IDispatch* event) {
  DCHECK(event != NULL);
  VARIANT arg = { VT_DISPATCH };
  arg.pdispVal = event;
  DISPPARAMS params = { &arg, NULL, 1, 0 };
  for (EventHandlers::const_iterator it = handlers.begin();
       it != handlers.end();
       ++it) {
    HRESULT hr = (*it)->Invoke(DISPID_VALUE, IID_NULL, LOCALE_USER_DEFAULT,
                               DISPATCH_METHOD, &params, NULL, NULL, NULL);
    // 0x80020101 == SCRIPT_E_REPORTED.
    // When the script we're invoking has an error, we get this error back.
    DLOG_IF(ERROR, FAILED(hr) && hr != 0x80020101)
        << base::StringPrintf(L"Failed to invoke script: 0x%08X", hr);
  }
}

void ChromeFrameActivex::FireEvent(const EventHandlers& handlers,
                                   IDispatch* event, BSTR target) {
  DCHECK(event != NULL);
  // Arguments in reverse order to event handler function declaration,
  // because that's what DISPPARAMS requires.
  VARIANT args[2] = { { VT_BSTR }, { VT_DISPATCH }, };
  args[0].bstrVal = target;
  args[1].pdispVal = event;
  DISPPARAMS params = { args, NULL, arraysize(args), 0 };
  for (EventHandlers::const_iterator it = handlers.begin();
       it != handlers.end();
       ++it) {
    HRESULT hr = (*it)->Invoke(DISPID_VALUE, IID_NULL, LOCALE_USER_DEFAULT,
                               DISPATCH_METHOD, &params, NULL, NULL, NULL);
    // 0x80020101 == SCRIPT_E_REPORTED.
    // When the script we're invoking has an error, we get this error back.
    DLOG_IF(ERROR, FAILED(hr) && hr != 0x80020101)
        << base::StringPrintf(L"Failed to invoke script: 0x%08X", hr);
  }
}

HRESULT ChromeFrameActivex::InstallTopLevelHook(IOleClientSite* client_site) {
  // Get the parent window of the site, and install our hook on the topmost
  // window of the parent.
  ScopedComPtr<IOleWindow> ole_window;
  HRESULT hr = ole_window.QueryFrom(client_site);
  if (FAILED(hr))
    return hr;

  HWND parent_wnd;
  hr = ole_window->GetWindow(&parent_wnd);
  if (FAILED(hr))
    return hr;

  HWND top_window = ::GetAncestor(parent_wnd, GA_ROOT);
  chrome_wndproc_hook_ = InstallLocalWindowHook(top_window);
  if (chrome_wndproc_hook_)
    TopLevelWindowMapping::GetInstance()->AddMapping(top_window, m_hWnd);

  return chrome_wndproc_hook_ ? S_OK : E_FAIL;
}

HRESULT ChromeFrameActivex::registerBhoIfNeeded() {
  if (!m_spUnkSite) {
    NOTREACHED() << "Invalid client site";
    return E_FAIL;
  }

  if (NavigationManager::GetThreadInstance() != NULL) {
    DVLOG(1) << "BHO already loaded";
    return S_OK;
  }

  ScopedComPtr<IWebBrowser2> web_browser2;
  HRESULT hr = DoQueryService(SID_SWebBrowserApp, m_spUnkSite,
                              web_browser2.Receive());
  if (FAILED(hr) || web_browser2.get() == NULL) {
    DLOG(WARNING) << "Failed to get IWebBrowser2 from client site. Error:"
                  << base::StringPrintf(" 0x%08X", hr);
    return hr;
  }

  wchar_t bho_class_id_as_string[MAX_PATH] = {0};
  StringFromGUID2(CLSID_ChromeFrameBHO, bho_class_id_as_string,
                  arraysize(bho_class_id_as_string));

  ScopedComPtr<IObjectWithSite> bho;
  hr = bho.CreateInstance(CLSID_ChromeFrameBHO, NULL, CLSCTX_INPROC_SERVER);
  if (FAILED(hr)) {
    NOTREACHED() << "Failed to register ChromeFrame BHO. Error:"
                 << base::StringPrintf(" 0x%08X", hr);
    return hr;
  }

  hr = UrlMkSetSessionOption(URLMON_OPTION_USERAGENT_REFRESH, NULL, 0, 0);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to refresh user agent string from registry. "
                << "UrlMkSetSessionOption returned "
                << base::StringPrintf("0x%08x", hr);
    return hr;
  }

  hr = bho->SetSite(web_browser2);
  if (FAILED(hr)) {
    NOTREACHED() << "ChromeFrame BHO SetSite failed. Error:"
                 << base::StringPrintf(" 0x%08X", hr);
    return hr;
  }

  web_browser2->PutProperty(base::win::ScopedBstr(bho_class_id_as_string),
                            base::win::ScopedVariant(bho));
  return S_OK;
}
