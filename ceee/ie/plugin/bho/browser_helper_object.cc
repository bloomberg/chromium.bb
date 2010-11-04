// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// IE browser helper object implementation.
#include "ceee/ie/plugin/bho/browser_helper_object.h"

#include <atlsafe.h>
#include <shlguid.h>

#include <algorithm>

#include "base/debug/trace_event.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/tuple.h"
#include "base/utf_string_conversions.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/window_utils.h"
#include "ceee/common/windows_constants.h"
#include "ceee/ie/broker/tab_api_module.h"
#include "ceee/ie/common/extension_manifest.h"
#include "ceee/ie/common/ie_util.h"
#include "ceee/ie/common/ceee_module_util.h"
#include "ceee/ie/plugin/bho/cookie_accountant.h"
#include "ceee/ie/plugin/bho/http_negotiate.h"
#include "ceee/ie/plugin/scripting/script_host.h"
#include "chrome/browser/automation/extension_automation_constants.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/automation_constants.h"
#include "googleurl/src/gurl.h"

#include "broker_lib.h"  // NOLINT

namespace keys = extension_tabs_module_constants;
namespace ext = extension_automation_constants;


_ATL_FUNC_INFO
    BrowserHelperObject::handler_type_idispatch_5variantptr_boolptr_ = {
  CC_STDCALL, VT_EMPTY, 7, {
    VT_DISPATCH,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_BOOL | VT_BYREF
  }
};

_ATL_FUNC_INFO BrowserHelperObject::handler_type_idispatch_variantptr_ = {
  CC_STDCALL, VT_EMPTY, 2, {
    VT_DISPATCH,
    VT_VARIANT | VT_BYREF
  }
};

_ATL_FUNC_INFO
    BrowserHelperObject::handler_type_idispatch_3variantptr_boolptr_ = {
  CC_STDCALL, VT_EMPTY, 5, {
    VT_DISPATCH,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_BOOL | VT_BYREF
  }
};

_ATL_FUNC_INFO BrowserHelperObject::handler_type_idispatchptr_boolptr_ = {
  CC_STDCALL, VT_EMPTY, 2, {
    VT_DISPATCH | VT_BYREF,
    VT_BOOL | VT_BYREF
  }
};

_ATL_FUNC_INFO
    BrowserHelperObject::handler_type_idispatchptr_boolptr_dword_2bstr_ = {
  CC_STDCALL, VT_EMPTY, 5, {
    VT_DISPATCH | VT_BYREF,
    VT_BOOL | VT_BYREF,
    VT_UI4,
    VT_BSTR,
    VT_BSTR
  }
};

// Remove the need to AddRef/Release for Tasks that target this class.
DISABLE_RUNNABLE_METHOD_REFCOUNT(BrowserHelperObject);

BrowserHelperObject::BrowserHelperObject()
    : already_tried_installing_(false),
      tab_window_(NULL),
      tab_id_(kInvalidChromeSessionId),
      fired_on_created_event_(false),
      lower_bound_ready_state_(READYSTATE_UNINITIALIZED),
      ie7_or_later_(false),
      thread_id_(::GetCurrentThreadId()),
      full_tab_chrome_frame_(false) {
  TRACE_EVENT_BEGIN("ceee.bho", this, "");
  // Only the first call to this function really does anything.
  CookieAccountant::GetInstance()->PatchWininetFunctions();
}

BrowserHelperObject::~BrowserHelperObject() {
  TRACE_EVENT_END("ceee.bho", this, "");
}

HRESULT BrowserHelperObject::FinalConstruct() {
  return S_OK;
}

void BrowserHelperObject::FinalRelease() {
  web_browser_.Release();
}

STDMETHODIMP BrowserHelperObject::SetSite(IUnknown* site) {
  typedef IObjectWithSiteImpl<BrowserHelperObject> SuperSite;

  // From experience, we know the site may be set multiple times.
  // Let's ignore second and subsequent set or unset.
  if (NULL != site && NULL != m_spUnkSite.p ||
      NULL == site && NULL == m_spUnkSite.p) {
    LOG(WARNING) << "Duplicate call to SetSite, previous site "
                 << m_spUnkSite.p << " new site " << site;
    return S_OK;
  }

  if (NULL == site) {
    // We're being torn down.
    TearDown();

    FireOnRemovedEvent();
    // This call should be the last thing we send to the broker.
    FireOnUnmappedEvent();
  }

  HRESULT hr = SuperSite::SetSite(site);
  if (FAILED(hr))
    return hr;

  if (NULL != site) {
    // We're being initialized.
    hr = Initialize(site);

    // Release the site in case of failure.
    if (FAILED(hr))
      SuperSite::SetSite(NULL);
  }

  return hr;
}

HRESULT BrowserHelperObject::GetParentBrowser(IWebBrowser2* browser,
                                              IWebBrowser2** parent_browser) {
  DCHECK(browser != NULL);
  DCHECK(parent_browser != NULL && *parent_browser == NULL);

  // Get the parent object.
  CComPtr<IDispatch> parent_disp;
  HRESULT hr = browser->get_Parent(&parent_disp);
  if (FAILED(hr)) {
    // NO DCHECK, or even log here, the caller will handle and log the error.
    return hr;
  }

  // Then get the associated browser through the appropriate contortions.
  CComQIPtr<IServiceProvider> parent_sp(parent_disp);
  if (parent_sp == NULL)
    return E_NOINTERFACE;

  CComPtr<IWebBrowser2> parent;
  hr = parent_sp->QueryService(SID_SWebBrowserApp,
                               IID_IWebBrowser2,
                               reinterpret_cast<void**>(&parent));
  if (FAILED(hr))
    return hr;

  DCHECK(parent != NULL);
  // IE seems to define the top-level browser as its own parent,
  // hence this check and error return.
  if (parent == browser)
    return E_FAIL;

  LOG(INFO) << "Child: " << std::hex << browser << " -> Parent: " <<
      std::hex << parent.p;

  *parent_browser = parent.Detach();
  return S_OK;
}

HRESULT BrowserHelperObject::GetBrokerRegistrar(
    ICeeeBrokerRegistrar** broker) {
  // This is a singleton and will not create a new one.
  return ::CoCreateInstance(CLSID_CeeeBroker, NULL, CLSCTX_ALL,
                            IID_ICeeeBrokerRegistrar,
                            reinterpret_cast<void**>(broker));
}

HRESULT BrowserHelperObject::CreateExecutor(IUnknown** executor) {
  HRESULT hr = ::CoCreateInstance(
      CLSID_CeeeExecutor, NULL, CLSCTX_INPROC_SERVER,
      IID_IUnknown, reinterpret_cast<void**>(executor));
  if (SUCCEEDED(hr)) {
    CComQIPtr<IObjectWithSite> executor_with_site(*executor);
    DCHECK(executor_with_site != NULL)
        << "Executor must implement IObjectWithSite.";
    if (executor_with_site != NULL) {
      CComPtr<IUnknown> bho_identity;
      hr = QueryInterface(IID_IUnknown,
                          reinterpret_cast<void**>(&bho_identity));
      DCHECK(SUCCEEDED(hr)) << "QueryInterface for IUnknown failed!!!" <<
          com::LogHr(hr);
      if (SUCCEEDED(hr))
        executor_with_site->SetSite(bho_identity);
    }
  }

  return hr;
}

WebProgressNotifier* BrowserHelperObject::CreateWebProgressNotifier() {
  scoped_ptr<WebProgressNotifier> web_progress_notifier(
      new WebProgressNotifier());
  HRESULT hr = web_progress_notifier->Initialize(this, tab_window_,
                                                 web_browser_);

  return SUCCEEDED(hr) ? web_progress_notifier.release() : NULL;
}

HRESULT BrowserHelperObject::Initialize(IUnknown* site) {
  TRACE_EVENT_INSTANT("ceee.bho.initialize", this, "");

  ie7_or_later_ = ie_util::GetIeVersion() > ie_util::IEVERSION_IE6;

  HRESULT hr = InitializeChromeFrameHost();
  DCHECK(SUCCEEDED(hr)) << "InitializeChromeFrameHost failed " <<
      com::LogHr(hr);
  if (FAILED(hr)) {
    TearDown();
    return hr;
  }

  // Initialize the extension port manager.
  extension_port_manager_.Initialize(chrome_frame_host_);

  // Preemptively feed the broker with an executor in our thread.
  hr = GetBrokerRegistrar(&broker_registrar_);
  LOG_IF(ERROR, FAILED(hr)) << "Failed to create broker, hr=" <<
      com::LogHr(hr);
  DCHECK(SUCCEEDED(hr)) << "CoCreating Broker. " << com::LogHr(hr);
  if (SUCCEEDED(hr)) {
    DCHECK(executor_ == NULL);
    hr = CreateExecutor(&executor_);
    LOG_IF(ERROR, FAILED(hr)) << "Failed to create Executor, hr=" <<
        com::LogHr(hr);
    DCHECK(SUCCEEDED(hr)) << "CoCreating Executor. " << com::LogHr(hr);
    if (SUCCEEDED(hr)) {
      // TODO(mad@chromium.org): Implement the proper manual/secure
      // registration.
      hr = broker_registrar_->RegisterTabExecutor(::GetCurrentThreadId(),
                                                  executor_);
      DCHECK(SUCCEEDED(hr)) << "Registering Executor. " << com::LogHr(hr);
    }
  }

  // We need the service provider for both the sink connections and
  // to get a handle to the tab window.
  CComQIPtr<IServiceProvider> service_provider(site);
  DCHECK(service_provider);
  if (service_provider == NULL) {
    TearDown();
    return E_FAIL;
  }

  hr = ConnectSinks(service_provider);
  DCHECK(SUCCEEDED(hr)) << "ConnectSinks failed " << com::LogHr(hr);
  if (FAILED(hr)) {
    TearDown();
    return hr;
  }

  hr = GetTabWindow(service_provider);
  DCHECK(SUCCEEDED(hr)) << "GetTabWindow failed " << com::LogHr(hr);
  if (FAILED(hr)) {
    TearDown();
    return hr;
  }

  // Patch IHttpNegotiate for user-agent and cookie functionality.
  HttpNegotiatePatch::Initialize();

  CheckToolBandVisibility(web_browser_);

  web_progress_notifier_.reset(CreateWebProgressNotifier());
  DCHECK(web_progress_notifier_ != NULL)
      << "Failed to initialize WebProgressNotifier";
  if (web_progress_notifier_ == NULL) {
    TearDown();
    return E_FAIL;
  }

  return S_OK;
}

HRESULT BrowserHelperObject::TearDown() {
  TRACE_EVENT_INSTANT("ceee.bho.teardown", this, "");

  if (web_progress_notifier_ != NULL) {
    web_progress_notifier_->TearDown();
    web_progress_notifier_.reset(NULL);
  }

  ToolBandVisibility::TearDown();

  if (executor_ != NULL) {
    CComQIPtr<IObjectWithSite> executor_with_site(executor_);
    DCHECK(executor_with_site != NULL)
        << "Executor must implement IObjectWithSite.";
    if (executor_with_site != NULL) {
      executor_with_site->SetSite(NULL);
    }
  }

  // Unregister our executor so that the broker don't have to rely
  // on the thread dying to release it and confuse COM in trying to release it.
  if (broker_registrar_ != NULL) {
    DCHECK(executor_ != NULL);
    // Manually disconnect executor_,
    // so it doesn't get called while we unregister it below.
    HRESULT hr = ::CoDisconnectObject(executor_, 0);
    DCHECK(SUCCEEDED(hr)) << "Couldn't disconnect Executor. " << com::LogHr(hr);

    // TODO(mad@chromium.org): Implement the proper manual/secure
    // unregistration.
    hr = broker_registrar_->UnregisterExecutor(::GetCurrentThreadId());
    DCHECK(SUCCEEDED(hr)) << "Unregistering Executor. " << com::LogHr(hr);
  } else {
    DCHECK(executor_ == NULL);
  }
  executor_.Release();

  if (web_browser_)
    DispEventUnadvise(web_browser_, &DIID_DWebBrowserEvents2);
  web_browser_.Release();

  HttpNegotiatePatch::Uninitialize();

  if (chrome_frame_host_) {
    chrome_frame_host_->SetEventSink(NULL);
    chrome_frame_host_->TearDown();
  }
  chrome_frame_host_.Release();

  return S_OK;
}

HRESULT BrowserHelperObject::InitializeChromeFrameHost() {
  HRESULT hr = CreateChromeFrameHost();
  DCHECK(SUCCEEDED(hr) && chrome_frame_host_ != NULL);
  if (FAILED(hr) || chrome_frame_host_ == NULL) {
    LOG(ERROR) << "Failed to get chrome frame host " << com::LogHr(hr);
    return com::AlwaysError(hr);
  }

  chrome_frame_host_->SetChromeProfileName(
      ceee_module_util::GetBrokerProfileNameForIe());
  chrome_frame_host_->SetEventSink(this);
  hr = chrome_frame_host_->StartChromeFrame();
  DCHECK(SUCCEEDED(hr)) << "Failed to start Chrome Frame." << com::LogHr(hr);
  if (FAILED(hr)) {
    chrome_frame_host_->SetEventSink(NULL);

    LOG(ERROR) << "Failed to start chrome frame " << com::LogHr(hr);
    return hr;
  }

  return hr;
}

HRESULT BrowserHelperObject::OnCfReadyStateChanged(LONG state) {
  // If EnsureTabId() returns false, the session_id isn't available.
  // This means that the ExternalTab hasn't been created yet, which is certainly
  // a bug. Calling this here will also ensure we call all the deferred calls if
  // they haven't been called yet.
  bool id_available = EnsureTabId();
  if (!id_available) {
    NOTREACHED();
    return E_UNEXPECTED;
  }

  if (state == READYSTATE_COMPLETE) {
    extension_path_ = ceee_module_util::GetExtensionPath();

    if (ceee_module_util::IsCrxOrEmpty(extension_path_) &&
        ceee_module_util::NeedToInstallExtension()) {
      LOG(INFO) << "Installing extension: \"" << extension_path_ << "\"";
      chrome_frame_host_->InstallExtension(CComBSTR(extension_path_.c_str()));
    } else {
      // In the case where we don't have a CRX (or we don't need to install it),
      // we must ask for the currently enabled extension before we can decide
      // what we need to do.
      chrome_frame_host_->GetEnabledExtensions();
    }
  }

  return S_OK;
}

HRESULT BrowserHelperObject::OnCfExtensionReady(BSTR path, int response) {
  TRACE_EVENT_INSTANT("ceee.bho.oncfextensionready", this, "");

  if (ceee_module_util::IsCrxOrEmpty(extension_path_)) {
    // If we get here, it's because we just did the first-time
    // install, so save the installation path+time for future comparison.
    ceee_module_util::SetInstalledExtensionPath(
        FilePath(extension_path_));
  }

  // Now list enabled extensions so that we can properly start it whether
  // it's a CRX file or an exploded folder.
  //
  // Note that we do this even if Chrome says installation failed,
  // as that is the error code it uses when we try to install an
  // older version of the extension than it already has, which happens
  // on overinstall when Chrome has already auto-updated.
  //
  // If it turns out no extension is installed, we will handle that
  // error in the OnCfGetEnabledExtensionsComplete callback.
  chrome_frame_host_->GetEnabledExtensions();
  return S_OK;
}

void BrowserHelperObject::StartExtension(const wchar_t* base_dir) {
  chrome_frame_host_->SetUrl(CComBSTR(chrome::kAboutBlankURL));

  LoadManifestFile(base_dir);

  // There is a race between launching Chrome to get the directory of
  // the extension, and the first page this BHO is attached to being loaded.
  // If we hadn't loaded the manifest file when injection of scripts and
  // CSS should have been done for that page, then do it now as it is the
  // earliest opportunity.
  BrowserHandlerMap::const_iterator it = browsers_.begin();
  for (; it != browsers_.end(); ++it) {
    DCHECK(it->second.m_T != NULL);
    it->second.m_T->RedoDoneInjections();
  }

  // Now we should know the extension id and can pass it to the executor.
  if (extension_id_.empty()) {
    LOG(ERROR) << "Have no extension id after loading the extension.";
  } else if (executor_ != NULL) {
    CComPtr<ICeeeInfobarExecutor> infobar_executor;
    HRESULT hr = executor_->QueryInterface(IID_ICeeeInfobarExecutor,
        reinterpret_cast<void**>(&infobar_executor));
    DCHECK(SUCCEEDED(hr)) << "Failed to get ICeeeInfobarExecutor interface " <<
        com::LogHr(hr);
    if (SUCCEEDED(hr)) {
      infobar_executor->SetExtensionId(CComBSTR(extension_id_.c_str()));
    }
  }
}

HRESULT BrowserHelperObject::OnCfGetEnabledExtensionsComplete(
    SAFEARRAY* base_dirs) {
  CComSafeArray<BSTR> directories;
  directories.Attach(base_dirs);  // MUST DETACH BEFORE RETURNING

  // TODO(joi@chromium.org) Deal with multiple extensions.
  if (directories.GetCount() > 0) {
    // If our extension_path is not a CRX, it MUST be the same as the installed
    // extension path which would be an exploded extension.
    // If you get this DCHECK, you may have changed your registry settings to
    // debug with an exploded extension, but you didn't uninstall the previous
    // extension, either via the Chrome UI or by simply wiping out your
    // profile folder.
    DCHECK(ceee_module_util::IsCrxOrEmpty(extension_path_) ||
           extension_path_ == std::wstring(directories.GetAt(0)));
    StartExtension(directories.GetAt(0));
  } else if (!ceee_module_util::IsCrxOrEmpty(extension_path_)) {
    // We have an extension path that isn't a CRX and we don't have any
    // enabled extension, so we must load the exploded extension from this
    // given path. WE MUST DO THIS BEFORE THE NEXT ELSE IF because it assumes
    // a CRX file.
    chrome_frame_host_->LoadExtension(CComBSTR(extension_path_.c_str()));
  } else if (!already_tried_installing_ && !extension_path_.empty()) {
    // We attempt to install the .crx file from the CEEE folder; in the
    // default case this will happen only once after installation.
    already_tried_installing_ = true;
    chrome_frame_host_->InstallExtension(CComBSTR(extension_path_.c_str()));
  }

  // If no extension is installed, we do nothing. The toolband handles
  // this error and will show an explanatory message to the user.
  directories.Detach();
  return S_OK;
}

HRESULT BrowserHelperObject::OnCfGetExtensionApisToAutomate(
    BSTR* functions_enabled) {
  *functions_enabled = NULL;
  return S_FALSE;
}

HRESULT BrowserHelperObject::OnCfChannelError() {
  return S_FALSE;
}

bool BrowserHelperObject::EnsureTabId() {
  if (tab_id_ != kInvalidChromeSessionId) {
    return true;
  }

  HRESULT hr = chrome_frame_host_->GetSessionId(&tab_id_);
  DCHECK(SUCCEEDED(hr));
  if (hr == S_FALSE) {
    // The server returned false, the session_id isn't available yet
    return false;
  }

  // At this point if tab_id_ is still invalid we have a problem.
  if (tab_id_ == kInvalidChromeSessionId) {
    // TODO(hansl@google.com): uncomment the following code when the CF change
    // has landed.
    //// Something really bad happened.
    //NOTREACHED();
    //return false;
    tab_id_ = reinterpret_cast<int>(tab_window_);
  }

  CeeeWindowHandle handle = reinterpret_cast<CeeeWindowHandle>(tab_window_);
  hr = broker_registrar_->SetTabIdForHandle(tab_id_, handle);
  if (FAILED(hr)) {
    DCHECK(SUCCEEDED(hr)) << "An error occured when setting the tab_id: " <<
        com::LogHr(hr);
    tab_id_ = kInvalidChromeSessionId;
    return false;
  }
  VLOG(2) << "TabId(" << tab_id_ << ") set for Handle(" << handle << ")";

  // Call back all the methods we deferred. In order, please.
  while (!deferred_tab_id_call_.empty()) {
    // We pop here so that if an error happens in the call we don't recall the
    // faulty method. This has the side-effect of losing events.
    DeferredCallListType::value_type call = deferred_tab_id_call_.front();
    deferred_tab_id_call_.pop_front();
    call->Run();
    delete call;
  }

  return true;
}

// Fetch and remembers the tab window we are attached to.
HRESULT BrowserHelperObject::GetTabWindow(IServiceProvider* service_provider) {
  CComQIPtr<IOleWindow> ole_window;
  HRESULT hr = service_provider->QueryService(
      SID_SShellBrowser, IID_IOleWindow, reinterpret_cast<void**>(&ole_window));
  DCHECK(SUCCEEDED(hr)) << "Failed to get ole window " << com::LogHr(hr);
  if (FAILED(hr)) {
    return hr;
  }

  hr = ole_window->GetWindow(&tab_window_);
  DCHECK(SUCCEEDED(hr)) << "Failed to get window from ole window " <<
      com::LogHr(hr);
  if (FAILED(hr)) {
    return hr;
  }
  DCHECK(tab_window_ != NULL);

  // Initialize our executor to the right HWND
  if (executor_ == NULL)
    return E_POINTER;
  CComPtr<ICeeeTabExecutor> tab_executor;
  hr = executor_->QueryInterface(IID_ICeeeTabExecutor,
      reinterpret_cast<void**>(&tab_executor));
  if (SUCCEEDED(hr)) {
    CeeeWindowHandle handle = reinterpret_cast<CeeeWindowHandle>(tab_window_);
    hr = tab_executor->Initialize(handle);
  }
  return hr;
}

// Connect for notifications.
HRESULT BrowserHelperObject::ConnectSinks(IServiceProvider* service_provider) {
  HRESULT hr = service_provider->QueryService(
      SID_SWebBrowserApp, IID_IWebBrowser2,
      reinterpret_cast<void**>(&web_browser_));
  DCHECK(SUCCEEDED(hr)) << "Failed to get web browser " << com::LogHr(hr);
  if (FAILED(hr)) {
    return hr;
  }

  // Start sinking events from the web browser object.
  hr = DispEventAdvise(web_browser_, &DIID_DWebBrowserEvents2);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to set event sink " << com::LogHr(hr);
    return hr;
  }
  return hr;
}

HRESULT BrowserHelperObject::CreateChromeFrameHost() {
  return ChromeFrameHost::CreateInitializedIID(IID_IChromeFrameHost,
                                               &chrome_frame_host_);
}

HRESULT BrowserHelperObject::FireOnCreatedEvent(BSTR url) {
  DCHECK(url != NULL);
  DCHECK(tab_window_ != NULL);
  DCHECK(!fired_on_created_event_);
  HRESULT hr = tab_events_funnel().OnCreated(tab_window_, url,
      lower_bound_ready_state_ == READYSTATE_COMPLETE);
  fired_on_created_event_ = SUCCEEDED(hr);
  DCHECK(SUCCEEDED(hr)) << "Failed to fire the tab.onCreated event " <<
      com::LogHr(hr);
  return hr;
}

HRESULT BrowserHelperObject::FireOnRemovedEvent() {
  DCHECK(tab_window_ != NULL);
  HRESULT hr =
      tab_events_funnel().OnRemoved(tab_window_);
  DCHECK(SUCCEEDED(hr)) << "Failed to fire the tab.onRemoved event " <<
      com::LogHr(hr);
  return hr;
}

HRESULT BrowserHelperObject::FireOnUnmappedEvent() {
  DCHECK(tab_window_ != NULL);
  DCHECK(tab_id_ != kInvalidChromeSessionId);
  HRESULT hr = tab_events_funnel().OnTabUnmapped(tab_window_, tab_id_);
  DCHECK(SUCCEEDED(hr)) << "Failed to fire the ceee.onTabUnmapped event " <<
      com::LogHr(hr);
  return hr;
}

void BrowserHelperObject::CloseAll(IContentScriptNativeApi* instance) {
  extension_port_manager_.CloseAll(instance);
}

HRESULT BrowserHelperObject::OpenChannelToExtension(
    IContentScriptNativeApi* instance, const std::string& extension,
    const std::string& channel_name, int cookie) {
  // TODO(hansl@google.com): separate this method into an event and an Impl.
  if (EnsureTabId() == false) {
    deferred_tab_id_call_.push_back(NewRunnableMethod(
        this, &BrowserHelperObject::OpenChannelToExtension,
        instance, extension, channel_name, cookie));
    return S_OK;
  }

  scoped_ptr<DictionaryValue> tab_info(new DictionaryValue());

  DCHECK(tab_id_ != kInvalidChromeSessionId);
  tab_info->SetInteger(keys::kIdKey, tab_id_);

  return extension_port_manager_.OpenChannelToExtension(instance,
                                                        extension,
                                                        channel_name,
                                                        tab_info.release(),
                                                        cookie);
}

HRESULT BrowserHelperObject::PostMessage(int port_id,
                                         const std::string& message) {
  return extension_port_manager_.PostMessage(port_id, message);
}

HRESULT BrowserHelperObject::OnCfPrivateMessage(BSTR msg,
                                                BSTR origin,
                                                BSTR target) {
  // OnPortMessage uses tab_id_, so we need to check here.
  // TODO(hansl@google.com): separate this method into an event and an Impl.
  if (EnsureTabId() == false) {
    deferred_tab_id_call_.push_back(NewRunnableMethod(
        this, &BrowserHelperObject::OnCfPrivateMessage,
        CComBSTR(msg), origin, target));
    return S_OK;
  }

  const wchar_t* start = com::ToString(target);
  const wchar_t* end = start + ::SysStringLen(target);
  if (LowerCaseEqualsASCII(start, end, ext::kAutomationPortRequestTarget) ||
      LowerCaseEqualsASCII(start, end, ext::kAutomationPortResponseTarget)) {
    extension_port_manager_.OnPortMessage(msg);
    return S_OK;
  }

  // TODO(siggi@chromium.org): What to do here?
  LOG(ERROR) << "Unexpected message: '" << msg << "' to invalid target: "
      << target;
  return E_UNEXPECTED;
}


STDMETHODIMP_(void) BrowserHelperObject::OnBeforeNavigate2(
    IDispatch* webbrowser_disp, VARIANT* url, VARIANT* /*flags*/,
    VARIANT* /*target_frame_name*/, VARIANT* /*post_data*/,
    VARIANT* /*headers*/, VARIANT_BOOL* /*cancel*/) {
  if (webbrowser_disp == NULL || url == NULL) {
    LOG(ERROR) << "OnBeforeNavigate2 got invalid parameter(s)";
    return;
  }

  // TODO(hansl@google.com): separate this method into an event and an Impl.
  if (EnsureTabId() == false) {
    VARIANT* null_param = NULL;
    deferred_tab_id_call_.push_back(NewRunnableMethod(
        this, &BrowserHelperObject::OnBeforeNavigate2,
        webbrowser_disp, url, null_param, null_param, null_param, null_param,
        reinterpret_cast<VARIANT_BOOL*>(NULL)));
    return;
  }

  CComPtr<IWebBrowser2> webbrowser;
  HRESULT hr = webbrowser_disp->QueryInterface(&webbrowser);
  if (FAILED(hr)) {
    LOG(ERROR) << "OnBeforeNavigate2 failed to QI " << com::LogHr(hr);
    return;
  }

  if (V_VT(url) != VT_BSTR) {
    LOG(ERROR) << "OnBeforeNavigate2 url VT=" << V_VT(url);
    return;
  }

  for (std::vector<Sink*>::iterator iter = sinks_.begin();
       iter != sinks_.end(); ++iter) {
    (*iter)->OnBeforeNavigate(webbrowser, url->bstrVal);
  }

  // Notify the infobar executor on the event but only for the main browser.
  // TODO(vadimb@google.com): Refactor this so that the executor can just
  // register self as WebBrowserEventsSource::Sink. Right now this is
  // problematic because the executor is COM object.
  CComPtr<IWebBrowser2> parent_browser;
  if (executor_ != NULL && web_browser_ != NULL &&
      web_browser_.IsEqualObject(webbrowser_disp)) {
    CComPtr<ICeeeInfobarExecutor> infobar_executor;
    HRESULT hr = executor_->QueryInterface(IID_ICeeeInfobarExecutor,
        reinterpret_cast<void**>(&infobar_executor));
    DCHECK(SUCCEEDED(hr)) << "Failed to get ICeeeInfobarExecutor interface " <<
        com::LogHr(hr);
    if (SUCCEEDED(hr)) {
      infobar_executor->OnTopFrameBeforeNavigate(CComBSTR(url->bstrVal));
    }
  }
}

STDMETHODIMP_(void) BrowserHelperObject::OnDocumentComplete(
    IDispatch* webbrowser_disp, VARIANT* url) {
  if (webbrowser_disp == NULL || url == NULL) {
    LOG(ERROR) << "OnDocumentComplete got invalid parameter(s)";
    return;
  }

  // TODO(hansl@google.com): separate this method into an event and an Impl.
  if (EnsureTabId() == false) {
    deferred_tab_id_call_.push_back(NewRunnableMethod(
        this, &BrowserHelperObject::OnDocumentComplete, webbrowser_disp, url));
    return;
  }

  CComPtr<IWebBrowser2> webbrowser;
  HRESULT hr = webbrowser_disp->QueryInterface(&webbrowser);
  if (FAILED(hr)) {
    LOG(ERROR) << "OnDocumentComplete failed to QI " << com::LogHr(hr);
    return;
  }

  if (V_VT(url) != VT_BSTR) {
    LOG(ERROR) << "OnDocumentComplete url VT=" << V_VT(url);
    return;
  }

  for (std::vector<Sink*>::iterator iter = sinks_.begin();
       iter != sinks_.end(); ++iter) {
    (*iter)->OnDocumentComplete(webbrowser, url->bstrVal);
  }
}

STDMETHODIMP_(void) BrowserHelperObject::OnNavigateComplete2(
    IDispatch* webbrowser_disp, VARIANT* url) {
  if (webbrowser_disp == NULL || url == NULL) {
    LOG(ERROR) << "OnNavigateComplete2 got invalid parameter(s)";
    return;
  }

  // TODO(hansl@google.com): separate this method into an event and an Impl.
  if (EnsureTabId() == false) {
    deferred_tab_id_call_.push_back(NewRunnableMethod(
        this, &BrowserHelperObject::OnNavigateComplete2, webbrowser_disp, url));
    return;
  }

  CComPtr<IWebBrowser2> webbrowser;
  HRESULT hr = webbrowser_disp->QueryInterface(&webbrowser);
  if (FAILED(hr)) {
    LOG(ERROR) << "OnNavigateComplete2 failed to QI " << com::LogHr(hr);
    return;
  }

  if (V_VT(url) != VT_BSTR) {
    LOG(ERROR) << "OnNavigateComplete2 url VT=" << V_VT(url);
    return;
  }

  HandleNavigateComplete(webbrowser, url->bstrVal);

  for (std::vector<Sink*>::iterator iter = sinks_.begin();
       iter != sinks_.end(); ++iter) {
    (*iter)->OnNavigateComplete(webbrowser, url->bstrVal);
  }
}

STDMETHODIMP_(void) BrowserHelperObject::OnNavigateError(
    IDispatch* webbrowser_disp, VARIANT* url, VARIANT* /*target_frame_name*/,
    VARIANT* status_code, VARIANT_BOOL* /*cancel*/) {
  if (webbrowser_disp == NULL || url == NULL || status_code == NULL) {
    LOG(ERROR) << "OnNavigateError got invalid parameter(s)";
    return;
  }

  // TODO(hansl@google.com): separate this method into an event and an Impl.
  if (EnsureTabId() == false) {
    VARIANT* null_param = NULL;
    deferred_tab_id_call_.push_back(NewRunnableMethod(
        this, &BrowserHelperObject::OnNavigateError,
        webbrowser_disp, url, null_param, status_code,
        reinterpret_cast<VARIANT_BOOL*>(NULL)));
    return;
  }

  CComPtr<IWebBrowser2> webbrowser;
  HRESULT hr = webbrowser_disp->QueryInterface(&webbrowser);
  if (FAILED(hr)) {
    LOG(ERROR) << "OnNavigateError failed to QI " << com::LogHr(hr);
    return;
  }

  if (V_VT(url) != VT_BSTR) {
    LOG(ERROR) << "OnNavigateError url VT=" << V_VT(url);
    return;
  }

  if (V_VT(status_code) != VT_I4) {
    LOG(ERROR) << "OnNavigateError status_code VT=" << V_VT(status_code);
    return;
  }

  for (std::vector<Sink*>::iterator iter = sinks_.begin();
       iter != sinks_.end(); ++iter) {
    (*iter)->OnNavigateError(webbrowser, url->bstrVal, status_code->lVal);
  }
}

STDMETHODIMP_(void) BrowserHelperObject::OnNewWindow2(
    IDispatch** /*webbrowser_disp*/, VARIANT_BOOL* /*cancel*/) {
  // When a new window/tab is created, IE7 or later version of IE will also
  // fire NewWindow3 event, which extends NewWindow2 with additional
  // information. As a result, we ignore NewWindow2 event in that case.
  if (ie7_or_later_)
    return;

  CComBSTR url_context(L"");
  CComBSTR url(L"");
  for (std::vector<Sink*>::iterator iter = sinks_.begin();
       iter != sinks_.end(); ++iter) {
    (*iter)->OnNewWindow(url_context, url);
  }
}

STDMETHODIMP_(void) BrowserHelperObject::OnNewWindow3(
    IDispatch** /*webbrowser_disp*/, VARIANT_BOOL* /*cancel*/, DWORD /*flags*/,
    BSTR url_context, BSTR url) {
  // IE6 uses NewWindow2 event.
  if (!ie7_or_later_)
    return;

  for (std::vector<Sink*>::iterator iter = sinks_.begin();
       iter != sinks_.end(); ++iter) {
    (*iter)->OnNewWindow(url_context, url);
  }
}

bool BrowserHelperObject::BrowserContainsChromeFrame(IWebBrowser2* browser) {
  CComPtr<IDispatch> document_disp;
  HRESULT hr = browser->get_Document(&document_disp);
  if (FAILED(hr)) {
    // This should never happen.
    NOTREACHED() << "IWebBrowser2::get_Document failed " << com::LogHr(hr);
    return false;
  }

  CComQIPtr<IPersist> document_persist(document_disp);
  if (document_persist != NULL) {
    CLSID clsid = {};
    hr = document_persist->GetClassID(&clsid);
    if (SUCCEEDED(hr) && (clsid == CLSID_ChromeFrame ||
                          clsid == CLSID_ChromeActiveDocument)) {
      return true;
    }
  }
  return false;
}

HRESULT BrowserHelperObject::AttachBrowserHandler(IWebBrowser2* webbrowser,
    IFrameEventHandler** handler) {
  // We're not attached yet, figure out whether we're attaching
  // to the top-level browser or a frame, and looukup the parentage
  // in the latter case.
  CComPtr<IWebBrowser2> parent_browser;
  HRESULT hr = S_OK;
  bool is_top_frame = web_browser_.IsEqualObject(webbrowser);
  if (!is_top_frame) {
    // Not the top-level browser, so find the parent. If this fails,
    // we assume webbrowser is orphaned, and will not attach to it.
    // This can happen when a FRAME/IFRAME is removed from the DOM tree.
    hr = GetParentBrowser(webbrowser, &parent_browser);
    if (FAILED(hr))
      LOG(WARNING) << "Failed to get parent browser " << com::LogHr(hr);
  }

  // Attempt to attach to the web browser.
  if (SUCCEEDED(hr)) {
    hr = CreateFrameEventHandler(webbrowser, parent_browser, handler);
    bool document_is_mshtml = SUCCEEDED(hr);
    DCHECK(SUCCEEDED(hr) || hr == E_DOCUMENT_NOT_MSHTML) <<
        "Unexpected error creating a frame handler " << com::LogHr(hr);

    if (is_top_frame) {
      // Check if it is a chrome frame.
      bool is_chrome_frame = BrowserContainsChromeFrame(webbrowser);

      if (is_chrome_frame) {
        fired_on_created_event_ = true;
        full_tab_chrome_frame_ = true;
        // Send a tabs.onRemoved event to make the extension believe the tab is
        // dead.
        hr = FireOnRemovedEvent();
        DCHECK(SUCCEEDED(hr));
      } else if (document_is_mshtml) {
        // We know it's MSHTML. We check if the last page was chrome frame.
        if (full_tab_chrome_frame_) {
          // This will send a tabs.onCreated event later to balance the
          // onRemoved event above.
          fired_on_created_event_ = false;
          full_tab_chrome_frame_ = false;
        }
      }
    }
  }

  return hr;
}

void BrowserHelperObject::HandleNavigateComplete(IWebBrowser2* webbrowser,
                                                 BSTR url) {
  // If the top-level document or a sub-frame is navigated, we'll already
  // be attached to the browser in question, so don't reattach.
  HRESULT hr = S_OK;
  CComPtr<IFrameEventHandler> handler;
  if (FAILED(GetBrowserHandler(webbrowser, &handler))) {
    hr = AttachBrowserHandler(webbrowser, &handler);

    DCHECK(SUCCEEDED(hr)) << "Failed to attach ourselves to the web browser " <<
        com::LogHr(hr);
  }

  bool is_hash_change = false;
  if (handler) {
    // Find out if this is a hash change.
    CComBSTR prev_url;
    handler->GetUrl(&prev_url);
    is_hash_change = IsHashChange(url, prev_url);

    // Notify the handler of the current URL.
    hr = handler->SetUrl(url);
    DCHECK(SUCCEEDED(hr)) << "Failed setting the handler URL " <<
        com::LogHr(hr);
  }

  // SetUrl returns S_FALSE if the URL didn't change.
  if (SUCCEEDED(hr) && hr != S_FALSE) {
    // And we should only fire events for URL changes on the top frame.
    if (web_browser_.IsEqualObject(webbrowser)) {
      // We can assume that we are NOT in a complete state when we get
      // a navigation complete.
      lower_bound_ready_state_ = READYSTATE_UNINITIALIZED;

      // At this point, we should have all the tab windows created,
      // including the proper lower bound ready state set just before,
      // so setup the tab info if it has not been set yet.
      if (!fired_on_created_event_) {
        hr = FireOnCreatedEvent(url);
        DCHECK(SUCCEEDED(hr)) << "Failed to fire tab created event " <<
            com::LogHr(hr);
      }

      // The onUpdate event usually gets fired after the onCreated,
      // which is fired from FireOnCreatedEvent above.
      DCHECK(tab_window_ != NULL);
      hr = tab_events_funnel().OnUpdated(tab_window_, url,
                                         lower_bound_ready_state_);
      DCHECK(SUCCEEDED(hr)) << "Failed to fire tab updated event " <<
          com::LogHr(hr);

      // If this is a hash change, we manually fire the OnUpdated for the
      // complete ready state as we don't receive ready state notifications
      // for hash changes.
      if (is_hash_change) {
        hr = tab_events_funnel().OnUpdated(tab_window_, url,
                                           READYSTATE_COMPLETE);
        DCHECK(SUCCEEDED(hr)) << "Failed to fire tab updated event " <<
            com::LogHr(hr);
      }
    }
  }
}

HRESULT BrowserHelperObject::CreateFrameEventHandler(
    IWebBrowser2* browser, IWebBrowser2* parent_browser,
    IFrameEventHandler** handler) {
  return FrameEventHandler::CreateInitializedIID(
      browser, parent_browser, this, IID_IFrameEventHandler, handler);
}

HRESULT BrowserHelperObject::AttachBrowser(IWebBrowser2* browser,
                                           IWebBrowser2* parent_browser,
                                           IFrameEventHandler* handler) {
  DCHECK(browser);
  DCHECK(handler);
  // Get the identity unknown of the browser.
  CComPtr<IUnknown> browser_identity;
  HRESULT hr = browser->QueryInterface(&browser_identity);
  DCHECK(SUCCEEDED(hr)) << "QueryInterface for IUnknown failed!!!" <<
      com::LogHr(hr);
  if (FAILED(hr))
    return hr;

  std::pair<BrowserHandlerMap::iterator, bool> inserted =
      browsers_.insert(std::make_pair(browser_identity, handler));
  // We shouldn't have a previous entry for any inserted browser.
  // map::insert().second is true iff an item was inserted.
  DCHECK(inserted.second);

  // Now try and find a parent event handler for this browser.
  // If we have a parent browser, locate its handler
  // and notify it of the association.
  if (parent_browser) {
    CComPtr<IFrameEventHandler> parent_handler;
    hr = GetBrowserHandler(parent_browser, &parent_handler);

    // Notify the parent of its new underling.
    if (parent_handler != NULL) {
      hr = parent_handler->AddSubHandler(handler);
      DCHECK(SUCCEEDED(hr)) << "AddSubHandler()" << com::LogHr(hr);
    } else {
      LOG(INFO) << "Received an orphan handler: " << std::hex << handler <<
          com::LogHr(hr);
      // Lets see if we can find an ancestor up the chain of parent browsers
      // that we could connect to our existing hierarchy of handlers.
      CComQIPtr<IWebBrowser2> grand_parent_browser;
      hr = GetParentBrowser(parent_browser, &grand_parent_browser);
      if (FAILED(hr))
        LOG(WARNING) << "Failed to get parent browser " << com::LogHr(hr);
      bool valid_grand_parent = (grand_parent_browser != NULL &&
          !grand_parent_browser.IsEqualObject(parent_browser));
      DCHECK(valid_grand_parent) <<
          "Orphan handler without a valid grand parent!";
      LOG_IF(ERROR, !valid_grand_parent) << "Orphan handler: " << std::hex <<
          handler << ", with parent browser: " << std::hex << parent_browser;
      if (grand_parent_browser != NULL &&
          !grand_parent_browser.IsEqualObject(parent_browser)) {
        DCHECK(!web_browser_.IsEqualObject(parent_browser));
        // We have a grand parent IWebBrowser2, so create a handler for the
        // parent we were given that doesn't have a handler already.
        CComBSTR parent_browser_url;
        parent_browser->get_LocationURL(&parent_browser_url);
        DLOG(INFO) << "Creating handler for parent browser: " << std::hex <<
            parent_browser << ", at URL: " << parent_browser_url;
        hr = CreateFrameEventHandler(parent_browser, grand_parent_browser,
                                     &parent_handler);
        // If we have a handler for the child, we must be able to create one for
        // the parent... And CreateFrameEventHandler should have attached it
        // to the parent by calling us again via IFrameEventHandler::Init...
        DCHECK(SUCCEEDED(hr) && parent_handler != NULL) << com::LogHr(hr);
        if (FAILED(hr) || parent_handler == NULL)
          return hr;

        // When we create a handler, we must set its URL.
        hr = parent_handler->SetUrl(parent_browser_url);
        DCHECK(SUCCEEDED(hr)) << "Handler::SetUrl()" << com::LogHr(hr);
        if (FAILED(hr))
          return hr;

        // And now that we have a fully created parent handler, we can add
        // the handler that looked orphan, to its newly created parent.
        hr = parent_handler->AddSubHandler(handler);
        DCHECK(SUCCEEDED(hr)) << "AddSubHandler()" << com::LogHr(hr);
      } else {
        // No grand parent for the orphan handler?
        return E_UNEXPECTED;
      }
    }
  }

  if (inserted.second)
    return S_OK;
  else
    return E_FAIL;
}

HRESULT BrowserHelperObject::DetachBrowser(IWebBrowser2* browser,
                                           IWebBrowser2* parent_browser,
                                           IFrameEventHandler* handler) {
  // Get the identity unknown of the browser.
  CComPtr<IUnknown> browser_identity;
  HRESULT hr = browser->QueryInterface(&browser_identity);
  DCHECK(SUCCEEDED(hr)) << "QueryInterface for IUnknown failed!!!" <<
      com::LogHr(hr);
  if (FAILED(hr))
    return hr;

  // If we have a parent browser, locate its handler
  // and notify it of the disassociation.
  if (parent_browser) {
    CComPtr<IFrameEventHandler> parent_handler;

    hr = GetBrowserHandler(parent_browser, &parent_handler);
    LOG_IF(WARNING, FAILED(hr) || parent_handler == NULL) << "No Parent " <<
        "Handler" << com::LogHr(hr);

    // Notify the parent of its underling removal.
    if (parent_handler != NULL) {
      hr = parent_handler->RemoveSubHandler(handler);
      DCHECK(SUCCEEDED(hr)) << "RemoveSubHandler" << com::LogHr(hr);
    }
  }

  BrowserHandlerMap::iterator it = browsers_.find(browser_identity);
  DCHECK(it != browsers_.end());
  if (it == browsers_.end())
    return E_FAIL;

  browsers_.erase(it);
  return S_OK;
}

HRESULT BrowserHelperObject::GetTopLevelBrowser(IWebBrowser2** browser) {
  DCHECK(browser != NULL);
  return web_browser_.CopyTo(browser);
}

HRESULT BrowserHelperObject::OnReadyStateChanged(READYSTATE ready_state) {
  // We make sure to always keep the lowest ready state of all the handlers
  // and only fire an event when we get from not completed to completed or
  // vice versa.
  READYSTATE new_lowest_ready_state = READYSTATE_COMPLETE;
  BrowserHandlerMap::const_iterator it = browsers_.begin();
  for (; it != browsers_.end(); ++it) {
    DCHECK(it->second.m_T != NULL);
    READYSTATE this_ready_state = it->second.m_T->GetReadyState();
    if (this_ready_state < new_lowest_ready_state) {
      new_lowest_ready_state = this_ready_state;
    }
  }

  return HandleReadyStateChanged(lower_bound_ready_state_,
                                 new_lowest_ready_state);
}

HRESULT BrowserHelperObject::GetReadyState(READYSTATE* ready_state) {
  DCHECK(ready_state != NULL);
  if (ready_state != NULL) {
    *ready_state = lower_bound_ready_state_;
    return S_OK;
  } else {
    return E_POINTER;
  }
}

HRESULT BrowserHelperObject::GetExtensionId(std::wstring* extension_id) {
  *extension_id = extension_id_;
  return S_OK;
}

HRESULT BrowserHelperObject::GetExtensionPath(std::wstring* extension_path) {
  *extension_path = extension_base_dir_;
  return S_OK;
}

HRESULT BrowserHelperObject::GetExtensionPortMessagingProvider(
    IExtensionPortMessagingProvider** messaging_provider) {
  GetUnknown()->AddRef();
  *messaging_provider = this;
  return S_OK;
}

HRESULT BrowserHelperObject::InsertCode(BSTR code, BSTR file, BOOL all_frames,
                                        CeeeTabCodeType type) {
  // TODO(hansl@google.com): separate this method into an event and an Impl.
  if (EnsureTabId() == false) {
    deferred_tab_id_call_.push_back(NewRunnableMethod(
        this, &BrowserHelperObject::InsertCode,
        code, file, all_frames, type));
    return S_OK;
  }

  // If all_frames is false, we execute only in the top level frame.  Otherwise
  // we do the top level frame as well as all the inner frames.
  if (all_frames) {
    // Make a copy of the browser handler map since it could potentially be
    // modified in the loop.
    BrowserHandlerMap browsers_copy(browsers_.begin(), browsers_.end());
    BrowserHandlerMap::const_iterator it = browsers_copy.begin();
    for (; it != browsers_copy.end(); ++it) {
      DCHECK(it->second.m_T != NULL);
      if (it->second.m_T != NULL) {
        HRESULT hr = it->second.m_T->InsertCode(code, file, type);
        DCHECK(SUCCEEDED(hr)) << "IFrameEventHandler::InsertCode()" <<
            com::LogHr(hr);
      }
    }
  } else if (web_browser_ != NULL) {
    CComPtr<IFrameEventHandler> handler;
    HRESULT hr = GetBrowserHandler(web_browser_, &handler);
    DCHECK(SUCCEEDED(hr) && handler != NULL) << com::LogHr(hr);

    if (handler != NULL) {
      hr = handler->InsertCode(code, file, type);
      // TODO(joi@chromium.org) We don't DCHECK for now, because Chrome may have
      // multiple extensions loaded whereas CEEE only knows about a single
      // extension.  Clean this up once we support multiple extensions.
    }
  }

  return S_OK;
}

HRESULT BrowserHelperObject::HandleReadyStateChanged(READYSTATE old_state,
                                                     READYSTATE new_state) {
  if (old_state == new_state)
    return S_OK;

  // Remember the new lowest ready state as our current one.
  lower_bound_ready_state_ = new_state;

  if (old_state == READYSTATE_COMPLETE || new_state == READYSTATE_COMPLETE) {
    // The new ready state got us to or away from complete, so fire the event.
    DCHECK(tab_window_ != NULL);
    return tab_events_funnel().OnUpdated(tab_window_, NULL, new_state);
  }
  return S_OK;
}

HRESULT BrowserHelperObject::GetMatchingUserScriptsCssContent(
    const GURL& url, bool require_all_frames, std::string* css_content) {
  return librarian_.GetMatchingUserScriptsCssContent(url, require_all_frames,
                                                     css_content);
}

HRESULT BrowserHelperObject::GetMatchingUserScriptsJsContent(
    const GURL& url, UserScript::RunLocation location, bool require_all_frames,
    UserScriptsLibrarian::JsFileList* js_file_list) {
  return librarian_.GetMatchingUserScriptsJsContent(url, location,
                                                    require_all_frames,
                                                    js_file_list);
}

HRESULT BrowserHelperObject::GetBrowserHandler(IWebBrowser2* webbrowser,
                                               IFrameEventHandler** handler) {
  DCHECK(webbrowser != NULL);
  DCHECK(handler != NULL && *handler == NULL);
  CComPtr<IUnknown> browser_identity;
  HRESULT hr = webbrowser->QueryInterface(&browser_identity);
  DCHECK(SUCCEEDED(hr)) << com::LogHr(hr);

  BrowserHandlerMap::iterator found(browsers_.find(browser_identity));
  if (found != browsers_.end()) {
    found->second.m_T.CopyTo(handler);
    return S_OK;
  }

  return E_FAIL;
}

void BrowserHelperObject::LoadManifestFile(const std::wstring& base_dir) {
  // TODO(siggi@chromium.org): Generalize this to the possibility of
  // multiple extensions.
  FilePath extension_path(base_dir);
  if (extension_path.empty()) {
    // expected case if no extensions registered/found
    return;
  }

  extension_base_dir_ = base_dir;

  ExtensionManifest manifest;
  if (SUCCEEDED(manifest.ReadManifestFile(extension_path, true))) {
    extension_id_ = UTF8ToWide(manifest.extension_id());
    librarian_.AddUserScripts(manifest.content_scripts());
  }
}

void BrowserHelperObject::OnFinalMessage(HWND window) {
  GetUnknown()->Release();
}

LRESULT BrowserHelperObject::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  // Grab a self-reference.
  GetUnknown()->AddRef();

  return 0;
}

bool BrowserHelperObject::IsHashChange(BSTR url1, BSTR url2) {
  if (::SysStringLen(url1) == 0 || ::SysStringLen(url2) == 0) {
    return false;
  }

  GURL gurl1(url1);
  GURL gurl2(url2);

  // The entire URL should be the same except for the hash.
  // We could compare gurl1.ref() to gurl2.ref() on the last step, but this
  // doesn't differentiate between URLs like http://a/ and http://a/#.
  return gurl1.scheme() == gurl2.scheme() &&
      gurl1.username() == gurl2.username() &&
      gurl1.password() == gurl2.password() &&
      gurl1.host() == gurl2.host() &&
      gurl1.port() == gurl2.port() &&
      gurl1.path() == gurl2.path() &&
      gurl1.query() == gurl2.query() &&
      gurl1 != gurl2;
}

void BrowserHelperObject::RegisterSink(Sink* sink) {
  DCHECK(thread_id_ == ::GetCurrentThreadId());

  if (sink == NULL)
    return;

  // Although the registration logic guarantees that the same sink won't be
  // registered twice, we prefer to use std::vector rather than std::set.
  // Using std::vector, we could keep "first-registered-first-notified".
  //
  // With std::set, however, the notifying order can only be decided at
  // run-time. Moreover, in different runs, the notifying order may be
  // different, since the value of the pointer to each sink is changing. The may
  // cause unstable behavior and hard-to-debug issues.
  std::vector<Sink*>::iterator iter = std::find(sinks_.begin(), sinks_.end(),
                                                sink);
  if (iter == sinks_.end())
    sinks_.push_back(sink);
}

void BrowserHelperObject::UnregisterSink(Sink* sink) {
  DCHECK(thread_id_ == ::GetCurrentThreadId());

  if (sink == NULL)
    return;

  std::vector<Sink*>::iterator iter = std::find(sinks_.begin(), sinks_.end(),
                                                sink);
  if (iter != sinks_.end())
    sinks_.erase(iter);
}
