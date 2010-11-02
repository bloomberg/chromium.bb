// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Interface of WebBrowser events source.
#ifndef CEEE_IE_PLUGIN_BHO_WEB_BROWSER_EVENTS_SOURCE_H_
#define CEEE_IE_PLUGIN_BHO_WEB_BROWSER_EVENTS_SOURCE_H_

#include <exdisp.h>

// WebBrowserEventsSource defines the interface of a WebBrowser event publisher,
// which is used to register/unregister event consumers and fire WebBrowser
// events to them.
class WebBrowserEventsSource {
 public:
  // The interface of WebBrowser event consumers.
  class Sink {
   public:
    virtual ~Sink() {}
    virtual void OnBeforeNavigate(IWebBrowser2* browser, BSTR url) {}
    virtual void OnDocumentComplete(IWebBrowser2* browser, BSTR url) {}
    virtual void OnNavigateComplete(IWebBrowser2* browser, BSTR url) {}
    virtual void OnNavigateError(IWebBrowser2* browser, BSTR url,
                                 long status_code) {}
    virtual void OnNewWindow(BSTR url_context, BSTR url) {}
  };

  virtual ~WebBrowserEventsSource() {}

  virtual void RegisterSink(Sink* sink) = 0;
  virtual void UnregisterSink(Sink* sink) = 0;
};

#endif  // CEEE_IE_PLUGIN_BHO_WEB_BROWSER_EVENTS_SOURCE_H_
