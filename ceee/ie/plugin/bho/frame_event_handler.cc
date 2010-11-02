// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// @file
// Frame event handler implementation.
#include "ceee/ie/plugin/bho/frame_event_handler.h"
#include <mshtml.h>
#include <shlguid.h>
#include "base/file_util.h"
#include "base/logging.h"
#include "ceee/ie/common/ceee_module_util.h"
#include "ceee/ie/plugin/bho/dom_utils.h"
#include "ceee/ie/plugin/scripting/script_host.h"
#include "ceee/common/com_utils.h"
#include "chrome/common/extensions/extension_resource.h"
#include "third_party/activscp/activdbg.h"
#include "toolband.h"  // NOLINT

// {242CD33B-B386-44a7-B685-3A9999B95420}
const GUID IID_IFrameEventHandler =
  { 0x242cd33b, 0xb386, 0x44a7,
      { 0xb6, 0x85, 0x3a, 0x99, 0x99, 0xb9, 0x54, 0x20 } };

// {E68D8538-0E0F-4d42-A4A2-1A635675F376}
const GUID IID_IFrameEventHandlerHost =
  { 0xe68d8538, 0xe0f, 0x4d42,
      { 0xa4, 0xa2, 0x1a, 0x63, 0x56, 0x75, 0xf3, 0x76 } };


FrameEventHandler::FrameEventHandler()
    : property_notify_sink_cookie_(kInvalidCookie),
      advise_sink_cookie_(kInvalidCookie),
      document_ready_state_(READYSTATE_UNINITIALIZED),
      initialized_debugging_(false),
      loaded_css_(false),
      loaded_start_scripts_(false),
      loaded_end_scripts_(false) {
}

FrameEventHandler::~FrameEventHandler() {
  DCHECK_EQ(property_notify_sink_cookie_, kInvalidCookie);
  DCHECK_EQ(advise_sink_cookie_, kInvalidCookie);
}

HRESULT FrameEventHandler::Initialize(IWebBrowser2* browser,
                                      IWebBrowser2* parent_browser,
                                      IFrameEventHandlerHost* host) {
  DCHECK(browser != NULL);
  DCHECK(host != NULL);

  InitializeContentScriptManager();

  CComPtr<IDispatch> document_disp;
  HRESULT hr = browser->get_Document(&document_disp);
  if (FAILED(hr)) {
    // This should never happen.
    NOTREACHED() << "IWebBrowser2::get_Document failed " << com::LogHr(hr);
    return hr;
  }

  // We used to check for whether the document implements IHTMLDocument2
  // to see whether we have an HTML document instance, but that check
  // proved not to be specific enough, as e.g. Google Chrome Frame implements
  // IHTMLDocument2. So instead we check specifically for MSHTMLs CLSID.
  CComQIPtr<IPersist> document_persist(document_disp);
  bool document_is_mshtml = false;
  if (document_persist != NULL) {
    CLSID clsid = {};
    hr = document_persist->GetClassID(&clsid);
    if (SUCCEEDED(hr) && clsid == CLSID_HTMLDocument)
      document_is_mshtml = true;
  }

  // Check that the document is an MSHTML instance, as opposed to e.g. a
  // PDF document or a ChromeFrame server.
  if (document_is_mshtml) {
    document_ = document_disp;
    DCHECK(document_ != NULL);

    // Attach to the document, any error here is abnormal
    // and should be returned to our caller.
    hr = AttachToHtmlDocument(browser, parent_browser, host);

    if (SUCCEEDED(hr))
      // If we have a parent browser, then this frame is an iframe and we only
      // want to match content scripts where all_frames is true.
      hr = content_script_manager_->Initialize(host, parent_browser != NULL);

    if (FAILED(hr))
      TearDown();

    return hr;
  }

  // It wasn't an HTML document, we kindly decline to attach to this one.
  return E_DOCUMENT_NOT_MSHTML;
}

HRESULT FrameEventHandler::AttachToHtmlDocument(IWebBrowser2* browser,
                                                IWebBrowser2* parent_browser,
                                                IFrameEventHandlerHost* host) {
  DCHECK(browser);
  DCHECK(host);

  // Check that we can retrieve the document's ready state.
  READYSTATE new_ready_state = READYSTATE_UNINITIALIZED;
  HRESULT hr = GetDocumentReadyState(&new_ready_state);
  DCHECK(SUCCEEDED(hr)) << "Failed to retrieve document ready state "
        << com::LogHr(hr);

  // Set up the advise sink.
  if (SUCCEEDED(hr)) {
    CComQIPtr<IOleObject> ole_object(document_);
    DCHECK(ole_object != NULL) << "Document is not an OLE Object";

    hr = ole_object->Advise(this, &advise_sink_cookie_);
  }

  if (SUCCEEDED(hr)) {
    DCHECK(advise_sink_cookie_ != kInvalidCookie);
  } else {
    DCHECK(advise_sink_cookie_ == kInvalidCookie);
    LOG(ERROR) << "IOleObject::Advise failed " << com::LogHr(hr);
  }

  // Set up the property notify sink.
  if (SUCCEEDED(hr)) {
    hr = AtlAdvise(document_,
                   GetUnknown(),
                   IID_IPropertyNotifySink,
                   &property_notify_sink_cookie_);
    if (SUCCEEDED(hr)) {
      DCHECK(property_notify_sink_cookie_ != kInvalidCookie);
    } else {
      DCHECK(property_notify_sink_cookie_ == kInvalidCookie);
      LOG(ERROR) << "Subscribing IPropertyNotifySink failed "
          << com::LogHr(hr);
    }
  }

  if (SUCCEEDED(hr)) {
    // We're all good.
    browser_ = browser;
    parent_browser_ = parent_browser;
    host_ = host;

    if (!initialized_debugging_) {
      initialized_debugging_ = true;
      ScriptHost::default_debug_application()->Initialize(document_);
    }

    // Notify the host that we've attached this browser instance.
    hr = host_->AttachBrowser(browser_, parent_browser_, this);
  } else {
    // We had some sort of failure, tear down any initialization we've done.
    TearDown();
  }

  return hr;
}

HRESULT FrameEventHandler::AddSubHandler(IFrameEventHandler* handler) {
  std::pair<SubHandlerSet::iterator, bool> ret =
      sub_handlers_.insert(CAdapt<CComPtr<IFrameEventHandler> >(handler));
  DCHECK(ret.second == true) << "Double notification for a sub handler";

  return S_OK;
}

HRESULT FrameEventHandler::RemoveSubHandler(IFrameEventHandler* handler) {
  size_t removed =
      sub_handlers_.erase(CAdapt<CComPtr<IFrameEventHandler> >(handler));
  DCHECK_NE(0U, removed) << "Errant removal notification for a sub handler";
  DCHECK_EQ(1U, removed);  // There can be only one in a set.

  return S_OK;
}

void FrameEventHandler::InitializeContentScriptManager() {
  content_script_manager_.reset(new ContentScriptManager);
}

HRESULT FrameEventHandler::GetExtensionResourceContents(const FilePath& file,
                                                        std::string* contents) {
  DCHECK(contents);

  std::wstring extension_path;
  host_->GetExtensionPath(&extension_path);
  DCHECK(extension_path.size());

  FilePath file_path(ExtensionResource::GetFilePath(
      FilePath(extension_path), file));

  if (!file_util::ReadFileToString(file_path, contents)) {
    return STG_E_FILENOTFOUND;
  }

  return S_OK;
}

HRESULT FrameEventHandler::GetCodeOrFileContents(BSTR code, BSTR file,
                                                 std::wstring* contents) {
  DCHECK(contents);

  // Must have one of code or file, but not both.
  bool has_code = ::SysStringLen(code) > 0;
  bool has_file = ::SysStringLen(file) > 0;
  if (!(has_code ^ has_file))
    return E_INVALIDARG;

  if (has_code) {
    *contents = code;
  } else {
    std::string code_string_a;
    HRESULT hr = GetExtensionResourceContents(FilePath(file), &code_string_a);
    if (FAILED(hr))
      return hr;

    *contents = CA2W(code_string_a.c_str());
  }

  return S_OK;
}

void FrameEventHandler::TearDownSubHandlers() {
  // Copy the set to avoid problems with reentrant
  // modification of the set during teardown.
  SubHandlerSet sub_handlers(sub_handlers_);
  SubHandlerSet::iterator it(sub_handlers.begin());
  SubHandlerSet::iterator end(sub_handlers.end());
  for (; it != end; ++it) {
    DCHECK(it->m_T);
    it->m_T->TearDown();
  }

  // In the case where we tear down subhandlers on a readystate
  // drop, it appears that by this time, the child->parent relationship
  // between  the sub-browsers and our attached browser has been severed.
  // We therefore can't expect our host to have issued RemoveSubHandler
  // for our subhandlers, and should do manual cleanup.
  sub_handlers_.clear();
}

void FrameEventHandler::TearDown() {
  // Start by tearing down subframes.
  TearDownSubHandlers();

  // Flush content script state.
  content_script_manager_->TearDown();

  // Then detach all event sinks.
  if (host_ != NULL) {
    DCHECK(browser_ != NULL);

    // Notify our host that we're detaching the browser.
    HRESULT hr = host_->DetachBrowser(browser_, parent_browser_, this);
    DCHECK(SUCCEEDED(hr));

    parent_browser_.Release();
    browser_.Release();
    host_.Release();
  }

  // Unadvise any document events we're sinking.
  if (property_notify_sink_cookie_ != kInvalidCookie) {
    HRESULT hr = AtlUnadvise(document_,
                             IID_IPropertyNotifySink,
                             property_notify_sink_cookie_);
    DCHECK(SUCCEEDED(hr)) << "Failed to unsubscribe IPropertyNotifySink "
        << com::LogHr(hr);
    property_notify_sink_cookie_ = kInvalidCookie;
  }

  if (advise_sink_cookie_ != kInvalidCookie) {
    CComQIPtr<IOleObject> ole_object(document_);
    DCHECK(ole_object != NULL);
    HRESULT hr = ole_object->Unadvise(advise_sink_cookie_);
    DCHECK(SUCCEEDED(hr)) << "Failed to unadvise IOleObject " << com::LogHr(hr);
    advise_sink_cookie_ = kInvalidCookie;
  }

  document_.Release();
}

HRESULT FrameEventHandler::InsertCode(BSTR code, BSTR file,
                                      CeeeTabCodeType type) {
  std::wstring extension_id;
  host_->GetExtensionId(&extension_id);
  if (!extension_id.size()) {
    // We haven't loaded an extension yet; defer until we do.
    DeferredInjection injection = {code ? code : L"", file ? file : L"", type};
    deferred_injections_.push_back(injection);
    LOG(INFO) << "Deferring InsertCode";
    return S_OK;
  } else {
    LOG(INFO) << "Executing InsertCode";
  }

  std::wstring code_string;
  HRESULT hr = GetCodeOrFileContents(code, file, &code_string);
  if (FAILED(hr))
    return hr;

  if (type == kCeeeTabCodeTypeCss) {
    return content_script_manager_->InsertCss(code_string.c_str(), document_);
  } else if (type == kCeeeTabCodeTypeJs) {
    const wchar_t* file_string;
    if (::SysStringLen(file) > 0) {
      file_string = OLE2W(file);
    } else {
      // TODO(rogerta@chromium.org): should not use a hardcoded name,
      // but one either extracted from the script itself or hashed
      // from the code. bb2146033.
      file_string = L"ExecuteScript.code";
    }

    return content_script_manager_->ExecuteScript(code_string.c_str(),
                                                  file_string,
                                                  document_);
  }

  return E_INVALIDARG;
}

void FrameEventHandler::RedoDoneInjections() {
  // Any type of injection we attempted to do before extensions were
  // loaded would have been a no-op.  This function is called once
  // extensions have been loaded to redo the ones that have
  // already been attempted.  Those that have not yet been attempted will
  // happen later, when appropriate (e.g. on readystate complete).
  GURL match_url(com::ToString(browser_url_));

  if (loaded_css_) {
    LoadCss(match_url);
  }

  if (loaded_start_scripts_) {
    LoadStartScripts(match_url);
  }

  if (loaded_end_scripts_) {
    LoadEndScripts(match_url);
  }

  // Take a copy to avoid an endless loop if we should for whatever
  // reason still not know the extension dir (this is just belt and
  // suspenders).
  std::list<DeferredInjection> injections = deferred_injections_;
  std::list<DeferredInjection>::iterator it = injections.begin();
  for (; it != injections.end(); ++it) {
    InsertCode(CComBSTR(it->code.c_str()),
               CComBSTR(it->file.c_str()),
               it->type);
  }
}

void FrameEventHandler::FinalRelease() {
  if (initialized_debugging_)
    ScriptHost::default_debug_application()->Terminate();
}

HRESULT FrameEventHandler::GetDocumentReadyState(READYSTATE* ready_state) {
  DCHECK(document_ != NULL);

  CComVariant ready_state_var;
  CComDispatchDriver document(document_);
  HRESULT hr = document.GetProperty(DISPID_READYSTATE, &ready_state_var);
  if (FAILED(hr))
    return hr;

  if (ready_state_var.vt != VT_I4)
    return E_UNEXPECTED;

  READYSTATE tmp = static_cast<READYSTATE>(ready_state_var.lVal);
  DCHECK(tmp >= READYSTATE_UNINITIALIZED && tmp <= READYSTATE_COMPLETE);
  *ready_state = tmp;
  return S_OK;
}

void FrameEventHandler::HandleReadyStateChange(READYSTATE old_ready_state,
                                               READYSTATE new_ready_state) {
  // We should always have been notified of our corresponding document's URL
  DCHECK(browser_url_ != NULL);
  DCHECK(document_ != NULL);

  if (new_ready_state <= READYSTATE_LOADING &&
      old_ready_state > READYSTATE_LOADING) {
    ReInitialize();
  }

  GURL match_url(com::ToString(browser_url_));

  if (new_ready_state >= READYSTATE_LOADED && !loaded_css_) {
    loaded_css_ = true;
    LoadCss(match_url);
  }

  if (new_ready_state >= READYSTATE_LOADED && !loaded_start_scripts_) {
    loaded_start_scripts_ = true;
    LoadStartScripts(match_url);
  }

  if (new_ready_state == READYSTATE_COMPLETE && !loaded_end_scripts_) {
    loaded_end_scripts_ = true;
    LoadEndScripts(match_url);
  }

  // Let our host know of this change.
  DCHECK(host_ != NULL);
  if (host_ != NULL) {
    HRESULT hr = host_->OnReadyStateChanged(new_ready_state);
    DCHECK(SUCCEEDED(hr)) << com::LogHr(hr);
  }
}

void FrameEventHandler::SetNewReadyState(READYSTATE new_ready_state) {
  READYSTATE old_ready_state = document_ready_state_;
  if (old_ready_state != new_ready_state) {
    document_ready_state_ = new_ready_state;
    HandleReadyStateChange(old_ready_state, new_ready_state);
  }
}

void FrameEventHandler::ReInitialize() {
  // This function should only be called when the readystate
  // drops from above LOADING to LOADING (or below).
  DCHECK(document_ready_state_ <= READYSTATE_LOADING);

  // A readystate drop means our associated document is being
  // re-navigated or refreshed. We need to tear down all sub
  // frame handlers, because they'll otherwise receive no
  // notification of this event.
  TearDownSubHandlers();

  // Reset our indicators, and manager.
  // We'll need to re-inject on subsquent up-transitions.
  loaded_css_ = false;
  loaded_start_scripts_ = false;
  loaded_end_scripts_ = false;

  content_script_manager_->TearDown();
}

STDMETHODIMP FrameEventHandler::OnChanged(DISPID property_disp_id) {
  if (property_disp_id == DISPID_READYSTATE) {
    READYSTATE new_ready_state = READYSTATE_UNINITIALIZED;
    HRESULT hr = GetDocumentReadyState(&new_ready_state);
    DCHECK(SUCCEEDED(hr));
    SetNewReadyState(new_ready_state);
  }
  return S_OK;
}

STDMETHODIMP FrameEventHandler::OnRequestEdit(DISPID property_disp_id) {
  return S_OK;
}

STDMETHODIMP_(void) FrameEventHandler::OnDataChange(FORMATETC* format,
                                                    STGMEDIUM* storage) {
}

STDMETHODIMP_(void) FrameEventHandler::OnViewChange(DWORD aspect, LONG index) {
}

STDMETHODIMP_(void) FrameEventHandler::OnRename(IMoniker* moniker) {
}

STDMETHODIMP_(void) FrameEventHandler::OnSave() {
}

STDMETHODIMP_(void) FrameEventHandler::OnClose() {
  // TearDown may release all references to ourselves, so we have to
  // maintain a self-reference over this call.
  CComPtr<IUnknown> staying_alive_oooh_oooh_oooh_staying_alive(GetUnknown());

  TearDown();
}

void FrameEventHandler::GetUrl(BSTR* url) {
  *url = CComBSTR(browser_url_).Detach();
}

HRESULT FrameEventHandler::SetUrl(BSTR url) {
  // This method is called by our host on NavigateComplete, at which time
  // our corresponding browser is either doing first-time navigation, or
  // it's being re-navigated to a different URL, or possibly to the same URL.
  // Note that as there's no NavigateComplete event fired for refresh,
  // we won't hit here in that case, but will rather notice that case on
  // our readystate dropping on an IPropertyNotifySink change notification.
  // The readystate for first-time navigation on the top-level browser
  // will be interactive, whereas for subsequent navigations and for
  // sub-browsers, the readystate will be loading.
  // This would be a fine time to probe and act on the readystate, except
  // for the fact that in some weird cases, GetDocumentReadyState incorrectly
  // returns READYSTATE_COMPLETE, which means we act too soon. So instead
  // we patiently wait for a property change notification and act on the
  // document's ready state then.
  if (browser_url_ == url)
    return S_FALSE;

  browser_url_ = url;
  return S_OK;
}

void FrameEventHandler::LoadCss(const GURL& match_url) {
  content_script_manager_->LoadCss(match_url, document_);
}

void FrameEventHandler::LoadStartScripts(const GURL& match_url) {
  // Run the document start scripts.
  content_script_manager_->LoadStartScripts(match_url, document_);
}

void FrameEventHandler::LoadEndScripts(const GURL& match_url) {
  // Run the document end scripts.
  content_script_manager_->LoadEndScripts(match_url, document_);
}
