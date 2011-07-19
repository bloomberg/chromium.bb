// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_READY_MODE_INTERNAL_READY_MODE_WEB_BROWSER_ADAPTER_H_
#define CHROME_FRAME_READY_MODE_INTERNAL_READY_MODE_WEB_BROWSER_ADAPTER_H_
#pragma once

#include <atlbase.h>
#include <atlcom.h>
#include <exdisp.h>
#include <exdispid.h>

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/win/scoped_comptr.h"

// Observes navigation and rendering in an IWebBrowser2 instance and reports
// activity to an observer.
class ATL_NO_VTABLE ReadyModeWebBrowserAdapter
    : public CComObjectRootEx<CComSingleThreadModel>,
      public IDispEventSimpleImpl<0, ReadyModeWebBrowserAdapter,
                                  &DIID_DWebBrowserEvents2> {
 public:
  // Receives notification of navigation and rendering activity in the
  // IWebBrowser2 instance.
  class Observer {
   public:
    virtual ~Observer() {}

    // Receives notification when the browser begins navigating.
    virtual void OnNavigateTo(const std::wstring& url) = 0;

    // Receives notification when the browser has rendered a page in Chrome
    // Frame.
    virtual void OnRenderInChromeFrame(const std::wstring& url) = 0;

    // Receives notification when the browser has rendered a page in the host
    // renderer.
    virtual void OnRenderInHost(const std::wstring& url) = 0;
  };  // class Observer

  ReadyModeWebBrowserAdapter();

  // Begins observation of the specified IWebBrowser2 instance, reporting
  // activity to the observer. Takes ownership of observer and deletes it
  // either upon failure to initialize, during Uninstall(), or when the browser
  // quits.
  bool Initialize(IWebBrowser2* web_browser_, Observer* observer);

  // Stops observing the IWebBrowser2.
  void Uninitialize();

DECLARE_NOT_AGGREGATABLE(ReadyModeWebBrowserAdapter)

BEGIN_COM_MAP(ReadyModeWebBrowserAdapter)
END_COM_MAP()

BEGIN_SINK_MAP(ReadyModeWebBrowserAdapter)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_BEFORENAVIGATE2,
                  BeforeNavigate2, &kBeforeNavigate2Info)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_DOCUMENTCOMPLETE,
                  DocumentComplete, &kDocumentCompleteInfo)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_ONQUIT,
                  OnQuit, &kOnQuitInfo)
END_SINK_MAP()

 private:
  // IWebBrowser2 event handlers
  STDMETHOD(BeforeNavigate2)(IDispatch* dispatch, VARIANT* url, VARIANT* flags,
      VARIANT* target_frame_name, VARIANT* post_data, VARIANT* headers,
      VARIANT_BOOL* cancel);
  STDMETHOD_(void, DocumentComplete)(IDispatch* dispatch, VARIANT* url);
  STDMETHOD_(void, OnQuit)();

  scoped_ptr<Observer> observer_;
  base::win::ScopedComPtr<IWebBrowser2> web_browser_;

  static _ATL_FUNC_INFO kBeforeNavigate2Info;
  static _ATL_FUNC_INFO kDocumentCompleteInfo;
  static _ATL_FUNC_INFO kOnQuitInfo;

  DISALLOW_COPY_AND_ASSIGN(ReadyModeWebBrowserAdapter);
};  // class ReadyModeWebBrowserAdapter

#endif  // CHROME_FRAME_READY_MODE_INTERNAL_READY_MODE_WEB_BROWSER_ADAPTER_H_
