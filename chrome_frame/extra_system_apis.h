// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header file contains declarations for system APIs and interfaces
// that are either undocumented or are documented but not included in any
// Platform SDK header files.
#ifndef CHROME_FRAME_EXTRA_SYSTEM_APIS_H_
#define CHROME_FRAME_EXTRA_SYSTEM_APIS_H_

// This is an interface provided by the WebBrowser object. It allows us to
// notify the browser of navigation events. MSDN documents this interface
// (see http://msdn2.microsoft.com/en-us/library/aa752109(VS.85).aspx)
// but this is not included in any Platform SDK header file.
class __declspec(uuid("54A8F188-9EBD-4795-AD16-9B4945119636"))
IWebBrowserEventsService : public IUnknown {
 public:
  STDMETHOD(FireBeforeNavigate2Event)(VARIANT_BOOL *cancel) = 0;
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
  STDMETHOD(GetUrlForEvents)(BSTR *url) = 0;
};

#endif  // CHROME_FRAME_EXTRA_SYSTEM_APIS_H_
