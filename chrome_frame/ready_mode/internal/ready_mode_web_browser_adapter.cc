// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/ready_mode/internal/ready_mode_web_browser_adapter.h"

#include "base/logging.h"
#include "base/win/win_util.h"
#include "chrome_frame/chrome_tab.h"

_ATL_FUNC_INFO ReadyModeWebBrowserAdapter::kBeforeNavigate2Info = {
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

_ATL_FUNC_INFO ReadyModeWebBrowserAdapter::kDocumentCompleteInfo = {
  CC_STDCALL, VT_EMPTY, 2, {
    VT_DISPATCH,
    VT_VARIANT | VT_BYREF
  }
};

_ATL_FUNC_INFO ReadyModeWebBrowserAdapter::kOnQuitInfo = {
    CC_STDCALL, VT_EMPTY, 0, {NULL}};

ReadyModeWebBrowserAdapter::ReadyModeWebBrowserAdapter() {
}

bool ReadyModeWebBrowserAdapter::Initialize(IWebBrowser2* web_browser,
                                            Observer* observer) {
  base::win::ScopedComPtr<ReadyModeWebBrowserAdapter, NULL> self(this);

  DCHECK(web_browser != NULL);
  DCHECK(web_browser_ == NULL);
  DCHECK(observer != NULL);
  DCHECK(observer_ == NULL);

  observer_.reset(observer);

  web_browser->AddRef();
  web_browser_.Attach(web_browser);

  HRESULT hr = DispEventAdvise(web_browser_, &DIID_DWebBrowserEvents2);
  if (FAILED(hr)) {
    observer_.reset();
    web_browser_.Release();
    DLOG(ERROR) << "DispEventAdvise failed. Error: " << hr;
  }

  return SUCCEEDED(hr);
}

void ReadyModeWebBrowserAdapter::Uninitialize() {
  // DispEventUnadvise will free the WebBrowser's reference to us. In case
  // that's the last reference, take a temporary  reference in this function's
  // scope to allow us to finish the cleanup we would otherwise like to do.
  base::win::ScopedComPtr<ReadyModeWebBrowserAdapter, NULL> self(this);

  observer_.reset();

  DCHECK(web_browser_ != NULL);
  if (web_browser_ == NULL)
    return;

  HRESULT hr = DispEventUnadvise(web_browser_, &DIID_DWebBrowserEvents2);
  if (FAILED(hr)) {
    DLOG(ERROR) << "DispEventUnadvise failed. Error: " << hr;
  } else {
    web_browser_.Release();
  }
}

STDMETHODIMP_(void) ReadyModeWebBrowserAdapter::OnQuit() {
  Uninitialize();
}

STDMETHODIMP ReadyModeWebBrowserAdapter::BeforeNavigate2(
    IDispatch* /*dispatch*/, VARIANT* url, VARIANT* /*flags*/,
    VARIANT* /*target_frame_name*/, VARIANT* /*post_data*/,
    VARIANT* /*headers*/, VARIANT_BOOL* /*cancel*/) {
  if (observer_ != NULL)
    observer_->OnNavigateTo(url->bstrVal);

  return S_OK;
}

STDMETHODIMP_(void) ReadyModeWebBrowserAdapter::DocumentComplete(
    IDispatch* /*dispatch*/, VARIANT* url) {
  if (!url || V_VT(url) != VT_BSTR || url->bstrVal == NULL)
    return;

  if (observer_ == NULL)
    return;

  base::win::ScopedComPtr<IDispatch> doc_disp;
  web_browser_->get_Document(doc_disp.Receive());
  if (!doc_disp)
    return;

  base::win::ScopedComPtr<IChromeFrame> chrome_frame_doc;
  chrome_frame_doc.QueryFrom(doc_disp);

  if (chrome_frame_doc)
    observer_->OnRenderInChromeFrame(url->bstrVal);
  else
    observer_->OnRenderInHost(url->bstrVal);
}
