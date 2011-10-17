// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_HTML_PRIVATE_WINDOW_IMPL_H_
#define CHROME_FRAME_HTML_PRIVATE_WINDOW_IMPL_H_

#include <atlbase.h>
#include <atlcom.h>
#include <mshtml.h>

#include "chrome_tab.h"  // NOLINT
#include "chrome_frame/resource.h"
#include "grit/chrome_frame_resources.h"

interface __declspec(uuid("3050F6DC-98B5-11CF-BB82-00AA00BDCE0B"))
IHTMLPrivateWindow : public IUnknown {
  STDMETHOD(SuperNavigate)(BSTR , BSTR, BSTR, BSTR , VARIANT*,
      VARIANT*, ULONG) = 0;
  STDMETHOD(GetPendingUrl)(BSTR*) = 0;
  STDMETHOD(SetPICSTarget)(IOleCommandTarget*) = 0;
  STDMETHOD(PICSComplete)(int) = 0;
  STDMETHOD(FindWindowByName)(PCWSTR, IHTMLWindow2**) = 0;
  STDMETHOD(GetAddressBarUrl)(BSTR* url) = 0;
};

template <typename T>
class ATL_NO_VTABLE HTMLPrivateWindowImpl : public T {
 public:
  HTMLPrivateWindowImpl() {}

  // IHTMLPrivateWindow
  STDMETHOD(SuperNavigate)(BSTR , BSTR, BSTR, BSTR , VARIANT*,
      VARIANT*, ULONG) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(GetPendingUrl)(BSTR*) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(SetPICSTarget)(IOleCommandTarget*) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(PICSComplete)(int) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(FindWindowByName)(LPCWSTR, IHTMLWindow2**) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(GetAddressBarUrl)(BSTR* url) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }
};

#endif  // CHROME_FRAME_HTML_PRIVATE_WINDOW_IMPL_H_

