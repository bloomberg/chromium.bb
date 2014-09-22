// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_REQUEST_CHROME_EXTENSION_WEB_REQUEST_EVENT_ROUTER_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_REQUEST_CHROME_EXTENSION_WEB_REQUEST_EVENT_ROUTER_DELEGATE_H_

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "extensions/browser/api/web_request/web_request_event_router_delegate.h"

class ChromeExtensionWebRequestEventRouterDelegate :
  public extensions::WebRequestEventRouterDelegate {
 public:
  ChromeExtensionWebRequestEventRouterDelegate();
  virtual ~ChromeExtensionWebRequestEventRouterDelegate();

  // WebRequestEventRouterDelegate implementation.
  virtual void ExtractExtraRequestDetails(
    net::URLRequest* request, base::DictionaryValue* out) OVERRIDE;
  virtual bool OnGetMatchingListenersImplCheck(
    int tab_id, int window_id, net::URLRequest* request) OVERRIDE;
  virtual void LogExtensionActivity(
    content::BrowserContext* browser_context,
    bool is_incognito,
    const std::string& extension_id,
    const GURL& url,
    const std::string& api_call,
    scoped_ptr<base::DictionaryValue> details) OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_REQUEST_CHROME_EXTENSION_WEB_REQUEST_EVENT_ROUTER_DELEGATE_H_
