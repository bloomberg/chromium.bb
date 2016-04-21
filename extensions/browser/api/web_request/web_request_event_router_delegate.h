// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_EVENT_ROUTER_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_EVENT_ROUTER_DELEGATE_H_

#include <memory>
#include <string>

#include "base/macros.h"

class GURL;

namespace base {
class DictionaryValue;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace net {
class URLRequest;
}  // namespace net

namespace extensions {

class WebRequestEventDetails;

// A delegate class of WebRequestApi that are not a part of chrome.
class WebRequestEventRouterDelegate {
 public:
  WebRequestEventRouterDelegate();
  virtual ~WebRequestEventRouterDelegate();

  // Logs an extension action.
  virtual void LogExtensionActivity(
      content::BrowserContext* browser_context,
      bool is_incognito,
      const std::string& extension_id,
      const GURL& url,
      const std::string& api_call,
      std::unique_ptr<base::DictionaryValue> details) {}

  // Notifies that a webRequest event that normally would be forwarded to a
  // listener was instead blocked because of withheld permissions.
  virtual void NotifyWebRequestWithheld(int render_process_id,
                                        int render_frame_id,
                                        const std::string& extension_id) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRequestEventRouterDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_EVENT_ROUTER_DELEGATE_H_
