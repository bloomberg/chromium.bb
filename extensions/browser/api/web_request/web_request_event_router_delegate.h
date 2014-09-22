// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_EVENT_ROUTER_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_EVENT_ROUTER_DELEGATE_H_

#include <string>

#include "base/values.h"

class GURL;

namespace base {
class DictionaryValue;
}  // namspace base

namespace content {
class BrowserContext;
}  // namespace content

namespace net {
class URLRequest;
}  // namspace net

namespace extensions {

// A delegate class of WebRequestApi that are not a part of chrome.
class WebRequestEventRouterDelegate {
 public:
  WebRequestEventRouterDelegate();
  virtual ~WebRequestEventRouterDelegate();

  // Looks up the tab and window ID for a given request.
  // Called on the IO thread.
  virtual void ExtractExtraRequestDetails(
      net::URLRequest* request, base::DictionaryValue* out) = 0;

  // Called to check extra parameters (e.g., tab_id, windown_id) when filtering
  // event listeners.
  virtual bool OnGetMatchingListenersImplCheck(
      int tab_id, int window_id, net::URLRequest* request);

  // Logs an extension action.
  virtual void LogExtensionActivity(
      content::BrowserContext* browser_context,
      bool is_incognito,
      const std::string& extension_id,
      const GURL& url,
      const std::string& api_call,
       scoped_ptr<base::DictionaryValue> details) = 0;

  DISALLOW_COPY_AND_ASSIGN(WebRequestEventRouterDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_EVENT_ROUTER_DELEGATE_H_
