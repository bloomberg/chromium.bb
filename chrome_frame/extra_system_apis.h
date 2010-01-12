// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header file contains declarations for system APIs and interfaces
// that are either undocumented or are documented but not included in any
// Platform SDK header files.
#ifndef CHROME_FRAME_EXTRA_SYSTEM_APIS_H_
#define CHROME_FRAME_EXTRA_SYSTEM_APIS_H_

#include <mshtml.h>

// This is an interface provided by the WebBrowser object. It allows us to
// notify the browser of navigation events. MSDN documents this interface
// (see http://msdn2.microsoft.com/en-us/library/aa752109(VS.85).aspx)
// but this is not included in any Platform SDK header file.
class __declspec(uuid("54A8F188-9EBD-4795-AD16-9B4945119636"))
IWebBrowserEventsService : public IUnknown {
 public:
  STDMETHOD(FireBeforeNavigate2Event)(VARIANT_BOOL* cancel) = 0;
  STDMETHOD(FireNavigateComplete2Event)(VOID) = 0;
  STDMETHOD(FireDownloadBeginEvent)(VOID) = 0;
  STDMETHOD(FireDownloadCompleteEvent)(VOID) = 0;
  STDMETHOD(FireDocumentCompleteEvent)(VOID) = 0;
};

// This interface is used in conjunction with the IWebBrowserEventsService
// interface. The web browser queries us for this interface when we invoke
// one of the IWebBrowserEventsService methods. This interface supplies the
// WebBrowser with a URL to use for the events. MSDN documents this interface
// (see http://msdn2.microsoft.com/en-us/library/aa752103(VS.85).aspx)
// but this is not included in any Platform SDK header file.
class __declspec(uuid("{87CC5D04-EAFA-4833-9820-8F986530CC00}"))
IWebBrowserEventsUrlService : public IUnknown {
 public:
  STDMETHOD(GetUrlForEvents)(BSTR* url) = 0;
};

// Web browser methods that are used by MSHTML when navigating or
// initiating downloads.  IE6.
class __declspec(uuid("{3050F804-98B5-11CF-BB82-00AA00BDCE0B}"))
IWebBrowserPriv : public IUnknown {
 public:
  STDMETHOD(NavigateWithBindCtx)(VARIANT* uri, VARIANT* flags,
                                 VARIANT* target_frame, VARIANT* post_data,
                                 VARIANT* headers, IBindCtx* bind_ctx,
                                 LPOLESTR url_fragment);
  STDMETHOD(OnClose)();
};

// The common denominator for IE7 and IE8 versions.  There's no specific IID
// here since all apply.  We use this interface for simplicities sake as all we
// want to do is invoke the NavigateWithBindCtx2 method.
class IWebBrowserPriv2Common : public IUnknown {
 public:
  STDMETHOD(NavigateWithBindCtx2)(IUri* uri, VARIANT* flags,
                                  VARIANT* target_frame, VARIANT* post_data,
                                  VARIANT* headers, IBindCtx* bind_ctx,
                                  LPOLESTR url_fragment);
};

// This interface is used to call FireBeforeNavigate with additional
// information like url. Available on IE7 onwards.
//
// MSDN documents this interface see:
// http://msdn.microsoft.com/en-us/library/aa770053(VS.85).aspx)
// but this is not included in any Platform SDK header file.
interface __declspec(uuid("3050f801-98b5-11cf-bb82-00aa00bdce0b"))
IDocObjectService : public IUnknown {
  STDMETHOD(FireBeforeNavigate2)(IDispatch* dispatch,
      LPCTSTR url, DWORD flags, LPCTSTR frame_name, BYTE* post_data,
      DWORD post_data_len, LPCTSTR headers, BOOL play_nav_sound,
      BOOL* cancel) = 0;
  STDMETHOD(FireNavigateComplete2)(IHTMLWindow2* html_window2,
      DWORD flags) = 0;
  STDMETHOD(FireDownloadBegin)() = 0;
  STDMETHOD(FireDownloadComplete)() = 0;
  STDMETHOD(FireDocumentComplete)(IHTMLWindow2* html_window2, DWORD flags) = 0;
  STDMETHOD(UpdateDesktopComponent)(IHTMLWindow2* html_window2) = 0;
  STDMETHOD(GetPendingUrl)(BSTR* pending_url) = 0;
  STDMETHOD(ActiveElementChanged)(IHTMLElement* html_element) = 0;
  STDMETHOD(GetUrlSearchComponent)(BSTR* search) = 0;
  STDMETHOD(IsErrorUrl)(LPCTSTR url, BOOL* is_error) = 0;
};

#endif  // CHROME_FRAME_EXTRA_SYSTEM_APIS_H_
