// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_REQUEST_CHROME_EXTENSION_WEB_REQUEST_EVENT_ROUTER_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_REQUEST_CHROME_EXTENSION_WEB_REQUEST_EVENT_ROUTER_DELEGATE_H_

#include "base/memory/scoped_ptr.h"
#include "extensions/browser/api/web_request/web_request_event_router_delegate.h"

namespace base {
class DictionaryValue;
}

class ChromeExtensionWebRequestEventRouterDelegate
    : public extensions::WebRequestEventRouterDelegate {
 public:
  ChromeExtensionWebRequestEventRouterDelegate();
  ~ChromeExtensionWebRequestEventRouterDelegate() override;

  // WebRequestEventRouterDelegate implementation.
  void ExtractExtraRequestDetails(const net::URLRequest* request,
                                  base::DictionaryValue* out) override;
  bool OnGetMatchingListenersImplCheck(int tab_id,
                                       int window_id,
                                       const net::URLRequest* request) override;
  void LogExtensionActivity(content::BrowserContext* browser_context,
                            bool is_incognito,
                            const std::string& extension_id,
                            const GURL& url,
                            const std::string& api_call,
                            scoped_ptr<base::DictionaryValue> details) override;
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_REQUEST_CHROME_EXTENSION_WEB_REQUEST_EVENT_ROUTER_DELEGATE_H_
