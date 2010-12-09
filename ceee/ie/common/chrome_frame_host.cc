// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ChromeFrameHost implementation.
#include "ceee/ie/common/chrome_frame_host.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/process_utils_win.h"
#include "ceee/ie/common/ceee_module_util.h"
#include "chrome/browser/automation/extension_automation_constants.h"
#include "chrome/common/chrome_switches.h"

#include "toolband.h"  // NOLINT

namespace ext = extension_automation_constants;


_ATL_FUNC_INFO ChromeFrameHost::handler_type_idispatch_ =
    { CC_STDCALL, VT_EMPTY, 1, { VT_DISPATCH } };
_ATL_FUNC_INFO ChromeFrameHost::handler_type_long_ =
    { CC_STDCALL, VT_EMPTY, 1, { VT_I4 } };
_ATL_FUNC_INFO ChromeFrameHost::handler_type_idispatch_bstr_ =
    { CC_STDCALL, VT_EMPTY, 2, { VT_DISPATCH, VT_BSTR } };
_ATL_FUNC_INFO ChromeFrameHost::handler_type_idispatch_variantptr_ =
    { CC_STDCALL, VT_EMPTY, 2, { VT_DISPATCH, VT_BYREF | VT_VARIANT } };
_ATL_FUNC_INFO ChromeFrameHost::handler_type_bstr_i4_=
    { CC_STDCALL, VT_EMPTY, 2, { VT_BSTR, VT_I4 } };
_ATL_FUNC_INFO ChromeFrameHost::handler_type_bstrarray_=
    { CC_STDCALL, VT_EMPTY, 1, { VT_ARRAY | VT_BSTR } };
_ATL_FUNC_INFO ChromeFrameHost::handler_type_void_=
    { CC_STDCALL, VT_EMPTY, 0, { } };

// {AFA3E2CF-2C8E-4546-8CD0-A2D93759A4DE}
extern const GUID IID_IChromeFrameHost =
  { 0xafa3e2cf, 0x2c8e, 0x4546,
      { 0x8c, 0xd0, 0xa2, 0xd9, 0x37, 0x59, 0xa4, 0xde } };

ChromeFrameHost::ChromeFrameHost()
    : document_loaded_(false), origin_(ext::kAutomationOrigin) {
  LOG(INFO) << "Create ChromeFrameHost(" << this << ")";
}

ChromeFrameHost::~ChromeFrameHost() {
  LOG(INFO) << "Destroy ChromeFrameHost(" << this << ")";
}

HRESULT ChromeFrameHost::FinalConstruct() {
  return S_OK;
}

void ChromeFrameHost::FinalRelease() {
}

STDMETHODIMP ChromeFrameHost::GetWantsPrivileged(boolean* wants_privileged) {
  *wants_privileged = true;
  return S_OK;
}

STDMETHODIMP ChromeFrameHost::GetChromeProfileName(BSTR* profile_name) {
  return chrome_profile_name_.CopyTo(profile_name);
}

STDMETHODIMP ChromeFrameHost::GetExtensionApisToAutomate(
    BSTR* functions_enabled) {
  DCHECK(functions_enabled != NULL);
  HRESULT hr = S_FALSE;
  if (event_sink_ != NULL) {
    hr = event_sink_->OnCfGetExtensionApisToAutomate(functions_enabled);
#ifndef NDEBUG
    if (*functions_enabled != NULL) {
      // Only one chrome frame host is allowed to return a list of functions
      // to enable automation on, so make sure we are the one and only.
      std::wstring event_name(L"google-ceee-apiautomation!");

      DCHECK(chrome_profile_name_ != NULL);
      if (chrome_profile_name_ != NULL)
        event_name += chrome_profile_name_;

      std::replace(event_name.begin(), event_name.end(), '\\', '!');
      std::transform(
          event_name.begin(), event_name.end(), event_name.begin(), tolower);
      automating_extension_api_.Set(
          ::CreateEvent(NULL, TRUE, TRUE, event_name.c_str()));
      DWORD we = ::GetLastError();
      DCHECK(automating_extension_api_ != NULL &&
             we != ERROR_ALREADY_EXISTS &&
             we != ERROR_ACCESS_DENIED);
    }
#endif  // NDEBUG
  }
  return hr;
}

STDMETHODIMP ChromeFrameHost::ShouldShowVersionMismatchDialog() {
  // Only our toolband allows the notification dialog to be shown.
  return S_FALSE;
}

HRESULT ChromeFrameHost::Initialize() {
  return S_OK;
}

STDMETHODIMP ChromeFrameHost::TearDown() {
  if (IsWindow()) {
    // TearDown the ActiveX host window.
    CAxWindow host(m_hWnd);
    CComPtr<IObjectWithSite> host_with_site;
    HRESULT hr = host.QueryHost(&host_with_site);
    if (SUCCEEDED(hr))
      host_with_site->SetSite(NULL);

    DestroyWindow();
  }

  if (chrome_frame_)
    ChromeFrameEvents::DispEventUnadvise(chrome_frame_);

  chrome_frame_.Release();
#ifndef NDEBUG
  automating_extension_api_.Close();
#endif
  return S_OK;
}

STDMETHODIMP_(void) ChromeFrameHost::SetEventSink(
    IChromeFrameHostEvents* event_sink) {
  event_sink_ = event_sink;
}

HRESULT ChromeFrameHost::InstallExtension(BSTR crx_path) {
  if (chrome_frame_) {
    return chrome_frame_->installExtension(crx_path);
  } else {
    NOTREACHED();
    return E_UNEXPECTED;
  }
}

HRESULT ChromeFrameHost::LoadExtension(BSTR extension_dir) {
  if (chrome_frame_) {
    return chrome_frame_->installExtension(extension_dir);
  } else {
    NOTREACHED();
    return E_UNEXPECTED;
  }
}

HRESULT ChromeFrameHost::GetEnabledExtensions() {
  if (chrome_frame_) {
    return chrome_frame_->getEnabledExtensions();
  } else {
    NOTREACHED();
    return E_UNEXPECTED;
  }
}

HRESULT ChromeFrameHost::GetSessionId(int* session_id) {
  if (chrome_frame_) {
    CComQIPtr<IChromeFrameInternal> chrome_frame_internal_(chrome_frame_);
    if (chrome_frame_internal_) {
      return chrome_frame_internal_->getSessionId(session_id);
    }
  }
  NOTREACHED();
  return E_UNEXPECTED;
}

void ChromeFrameHost::OnFinalMessage(HWND window) {
  GetUnknown()->Release();
}

HRESULT ChromeFrameHost::SetChildSite(IUnknown* child) {
  if (child == NULL)
    return E_POINTER;

  HRESULT hr = S_OK;
  CComPtr<IObjectWithSite> child_site;
  hr = child->QueryInterface(&child_site);
  if (SUCCEEDED(hr))
    hr = child_site->SetSite(GetUnknown());

  return hr;
}

LRESULT ChromeFrameHost::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  // Grab a self-reference.
  GetUnknown()->AddRef();

  return 0;
}

HRESULT ChromeFrameHost::SetUrl(BSTR url) {
  HRESULT hr = chrome_frame_->put_src(url);
  DCHECK(SUCCEEDED(hr)) << "Failed to navigate Chrome Frame: " <<
    com::LogHr(hr);
  return hr;
}

STDMETHODIMP ChromeFrameHost::StartChromeFrame() {
  DCHECK(!IsWindow());

  // Create a message window to host our control.
  if (NULL == Create(HWND_MESSAGE))
    return E_FAIL;

  // Create a host window instance.
  CComPtr<IAxWinHostWindow> host;
  HRESULT hr = CreateActiveXHost(&host);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create ActiveX host window: " << com::LogHr(hr);
    return hr;
  }

  // We're the site for the host window, this needs to be in place
  // before we attach ChromeFrame to the ActiveX control window, so
  // as to allow it to probe our service provider.
  hr = SetChildSite(host);
  DCHECK(SUCCEEDED(hr));

  // Create the chrome frame instance.
  hr = CreateChromeFrame(&chrome_frame_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed create Chrome Frame: " << com::LogHr(hr);
    return hr;
  }

  // And attach it to our window. This causes the host to subclass
  // our window and attach itself to it.
  hr = host->AttachControl(chrome_frame_, m_hWnd);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to attach Chrome Frame to the host" << com::LogHr(hr);
    return hr;
  }

  // Hook up the chrome frame event listener.
  hr = ChromeFrameEvents::DispEventAdvise(chrome_frame_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to hook up event sink: " << com::LogHr(hr);
    return hr;
  }

  return hr;
}

HRESULT ChromeFrameHost::CreateActiveXHost(IAxWinHostWindow** host) {
  return CAxHostWindow::CreateInstance(host);
}

HRESULT ChromeFrameHost::CreateChromeFrame(IChromeFrame** chrome_frame) {
  CComPtr<IChromeFrame> new_cf;
  HRESULT hr = new_cf.CoCreateInstance(L"ChromeTab.ChromeFrame");
  if (SUCCEEDED(hr))
    hr = new_cf.CopyTo(chrome_frame);

  return hr;
}

STDMETHODIMP ChromeFrameHost::PostMessage(BSTR message, BSTR target) {
  if (!document_loaded_) {
    PostedMessage posted_message = { message, target };
    posted_messages_.push_back(posted_message);
    return S_FALSE;
  }

  HRESULT hr = chrome_frame_->postPrivateMessage(message, origin_, target);

  return hr;
}

STDMETHODIMP_(void) ChromeFrameHost::OnCfLoad(IDispatch* event) {
  DLOG(INFO) << "OnCfLoad";
  if (document_loaded_) {
    // If we were already loaded, our list should be empty.
    DCHECK(posted_messages_.empty());
    return;
  }
  document_loaded_ = true;

  // Flush all posted messages.
  PostedMessageList::iterator it(posted_messages_.begin());
  for (; it != posted_messages_.end(); ++it) {
    HRESULT hr = chrome_frame_->postPrivateMessage(it->message, origin_,
                                                   it->target);
    DCHECK(SUCCEEDED(hr)) << "postPrivateMessage failed with: " <<
        com::LogHr(hr);
  }
  posted_messages_.clear();
}

STDMETHODIMP_(void) ChromeFrameHost::OnCfLoadError(IDispatch* event) {
  DLOG(INFO) << "OnCfLoadError";
}

STDMETHODIMP_(void) ChromeFrameHost::OnCfExtensionReady(BSTR path,
                                                        int response) {
  DLOG(INFO) << "OnCfExtensionReady: " << path << ", " << response;
  // Early exit if there's no sink.
  if (event_sink_ == NULL)
    return;
  event_sink_->OnCfExtensionReady(path, response);
}

STDMETHODIMP_(void) ChromeFrameHost::OnCfGetEnabledExtensionsComplete(
    SAFEARRAY* extension_directories) {
  DLOG(INFO) << "OnCfGetEnabledExtensionsComplete";
  if (event_sink_)
    event_sink_->OnCfGetEnabledExtensionsComplete(extension_directories);
}

STDMETHODIMP_(void) ChromeFrameHost::OnCfChannelError() {
  DCHECK(false) << "OnCfChannelError means that Chrome has Crashed!";
  if (event_sink_)
    event_sink_->OnCfChannelError();
}

STDMETHODIMP_(void) ChromeFrameHost::OnCfMessage(IDispatch* event) {
  DLOG(INFO) << "OnCfMessage";
}

STDMETHODIMP_(void) ChromeFrameHost::OnCfReadyStateChanged(LONG state) {
  DLOG(INFO) << "OnCfReadyStateChanged(" << state << ")";
  if (event_sink_)
    event_sink_->OnCfReadyStateChanged(state);
}

STDMETHODIMP_(void) ChromeFrameHost::OnCfPrivateMessage(IDispatch* event,
                                                        BSTR target) {
  // Early exit if there's no sink.
  if (event_sink_ == NULL)
    return;

  // Make sure that the message has a "data" member and get it.  This should
  // be a JSON-encoded command to execute.
  CComDispatchDriver event_dispatch(event);

  CComVariant origin;
  HRESULT hr = event_dispatch.GetPropertyByName(L"origin", &origin);
  DCHECK(SUCCEEDED(hr) && origin.vt == VT_BSTR);
  if (FAILED(hr) || origin.vt != VT_BSTR) {
    NOTREACHED() << "No origin on event";
    return;
  }

  CComVariant data;
  hr = event_dispatch.GetPropertyByName(L"data", &data);
  DCHECK(SUCCEEDED(hr) && data.vt == VT_BSTR);
  if (FAILED(hr) || data.vt != VT_BSTR) {
    NOTREACHED() << "No data on event";
    return;
  }

  // Forward to the sink.
  event_sink_->OnCfPrivateMessage(V_BSTR(&data), V_BSTR(&origin), target);
}

STDMETHODIMP_(void) ChromeFrameHost::SetChromeProfileName(
    const wchar_t* chrome_profile_name) {
  chrome_profile_name_ = chrome_profile_name;
  DLOG(INFO) << "Assigned profile name " << chrome_profile_name_;
}
