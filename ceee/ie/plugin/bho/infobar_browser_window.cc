// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the infobar browser window.

#include "ceee/ie/plugin/bho/infobar_browser_window.h"

#include <atlapp.h>
#include <atlcrack.h>
#include <atlmisc.h>
#include <atlsafe.h>
#include <atlwin.h>

#include "base/logging.h"
#include "ceee/common/com_utils.h"
#include "ceee/ie/common/ceee_module_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome_frame/com_message_event.h"

namespace infobar_api {

_ATL_FUNC_INFO InfobarBrowserWindow::handler_type_long_ =
    { CC_STDCALL, VT_EMPTY, 1, { VT_I4 } };
_ATL_FUNC_INFO InfobarBrowserWindow::handler_type_bstr_i4_=
    { CC_STDCALL, VT_EMPTY, 2, { VT_BSTR, VT_I4 } };
_ATL_FUNC_INFO InfobarBrowserWindow::handler_type_void_=
    { CC_STDCALL, VT_EMPTY, 0, { } };

InfobarBrowserWindow::InfobarBrowserWindow() : delegate_(NULL) {
}

InfobarBrowserWindow::~InfobarBrowserWindow() {
}

STDMETHODIMP InfobarBrowserWindow::GetWantsPrivileged(
    boolean* wants_privileged) {
  *wants_privileged = true;
  return S_OK;
}

STDMETHODIMP InfobarBrowserWindow::GetChromeExtraArguments(BSTR* args) {
  DCHECK(args);

  // Must enable experimental extensions because we want to load html pages
  // from our extension.
  // Extra arguments are passed on verbatim, so we add the -- prefix.
  CComBSTR str = "--";
  str.Append(switches::kEnableExperimentalExtensionApis);

  *args = str.Detach();
  return S_OK;
}

STDMETHODIMP InfobarBrowserWindow::GetChromeProfileName(BSTR* profile_name) {
  *profile_name = ::SysAllocString(
      ceee_module_util::GetBrokerProfileNameForIe());
  return S_OK;
}

STDMETHODIMP InfobarBrowserWindow::GetExtensionApisToAutomate(
    BSTR* functions_enabled) {
  *functions_enabled = NULL;
  return S_FALSE;
}

STDMETHODIMP InfobarBrowserWindow::ShouldShowVersionMismatchDialog() {
  // Only our toolband allows the notification dialog to be shown.
  return S_FALSE;
}

STDMETHODIMP_(void) InfobarBrowserWindow::OnCfReadyStateChanged(LONG state) {
  if (state == READYSTATE_COMPLETE) {
    // We already loaded the extension, enable them in this CF.
    chrome_frame_->getEnabledExtensions();
    // Also we should already have URL, navigate to it.
    Navigate();
    infobar_events_funnel().OnDocumentComplete();
  }
}

STDMETHODIMP_(void) InfobarBrowserWindow::OnCfExtensionReady(BSTR path,
                                                             int response) {
  if (ceee_module_util::IsCrxOrEmpty(extension_path_)) {
    // If we get here, it's because we just did the first-time
    // install, so save the installation path+time for future comparison.
    ceee_module_util::SetInstalledExtensionPath(FilePath(extension_path_));
  }

  chrome_frame_->getEnabledExtensions();
}

STDMETHODIMP_(void) InfobarBrowserWindow::OnCfClose() {
  if (delegate_ != NULL)
    delegate_->OnBrowserWindowClose();
}

HRESULT InfobarBrowserWindow::Initialize(BSTR url, Delegate* delegate) {
  set_delegate(delegate);
  SetUrl(url);

  return S_OK;
}

HRESULT InfobarBrowserWindow::CreateAndShowWindow(HWND parent) {
  HRESULT hr = InitializeAndShowWindow(parent);
  if (FAILED(hr)) {
    LOG(ERROR) << "Infobar browser failed to initialize its site window: " <<
                  com::LogHr(hr);
    return hr;
  }

  return S_OK;
}

HRESULT InfobarBrowserWindow::InitializeAndShowWindow(HWND parent) {
  if (NULL == Create(parent))
    return E_FAIL;

  BOOL shown = ShowWindow(SW_SHOW);
  DCHECK(shown);

  return shown ? S_OK : E_FAIL;
}

HRESULT InfobarBrowserWindow::Teardown() {
  set_delegate(NULL);

  if (IsWindow()) {
    // Teardown the ActiveX host window.
    CAxWindow host(m_hWnd);
    CComPtr<IObjectWithSite> host_with_site;
    HRESULT hr = host.QueryHost(&host_with_site);
    if (SUCCEEDED(hr))
      host_with_site->SetSite(NULL);

    DestroyWindow();
  }

  if (chrome_frame_) {
    ChromeFrameEvents::DispEventUnadvise(chrome_frame_);
    chrome_frame_.Release();
  }

  return S_OK;
}

STDMETHODIMP InfobarBrowserWindow::SetUrl(BSTR url) {
  // Navigate to the URL if the browser exists, otherwise just store the URL.
  url_ = url;
  Navigate();
  return S_OK;
}

STDMETHODIMP InfobarBrowserWindow::SetWindowSize(int width, int height) {
  if (IsWindow())
    SetWindowPos(NULL, 0, 0, width, height, SWP_NOACTIVATE | SWP_NOZORDER);
  return S_OK;
}

LRESULT InfobarBrowserWindow::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  // Grab a self-reference. Released in OnFinalMessage.
  GetUnknown()->AddRef();

  // Create a host window instance.
  CComPtr<IAxWinHostWindow> host;
  HRESULT hr = CAxHostWindow::CreateInstance(&host);
  if (FAILED(hr)) {
    LOG(ERROR) << "Infobar failed to create ActiveX host window. " <<
                  com::LogHr(hr);
    return -1;
  }

  // We're the site for the host window, this needs to be in place
  // before we attach ChromeFrame to the ActiveX control window, so
  // as to allow it to probe our service provider.
  hr = SetChildSite(host);
  DCHECK(SUCCEEDED(hr));

  // Create the chrome frame instance.
  hr = chrome_frame_.CoCreateInstance(L"ChromeTab.ChromeFrame");
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create the Chrome Frame instance. " <<
        com::LogHr(hr);
    return -1;
  }

  // And attach it to our window.
  hr = host->AttachControl(chrome_frame_, m_hWnd);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to attach Chrome Frame to the host. " <<
        com::LogHr(hr);
    return -1;
  }

  // Hook up the chrome frame event listener.
  hr = ChromeFrameEvents::DispEventAdvise(chrome_frame_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to hook up event sink. " << com::LogHr(hr);
  }

  return 0;
}

void InfobarBrowserWindow::OnDestroy() {
  if (chrome_frame_) {
    ChromeFrameEvents::DispEventUnadvise(chrome_frame_);
    chrome_frame_.Release();
  }
}

void InfobarBrowserWindow::OnFinalMessage(HWND window) {
  GetUnknown()->Release();
}

void InfobarBrowserWindow::Navigate() {
  // If the browser has not been created then just return.
  if (!chrome_frame_)
    return;

  if (url_.empty()) {
    LOG(WARNING) << "Navigating infobar to not specified URL";
  } else {
    HRESULT hr = chrome_frame_->put_src(CComBSTR(url_.c_str()));
    LOG_IF(WARNING, FAILED(hr)) <<
      "Infobar: ChromeFrame::put_src returned: " << com::LogHr(hr);
  }
}

}  // namespace infobar_api
