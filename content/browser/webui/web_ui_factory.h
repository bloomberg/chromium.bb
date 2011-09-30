// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_FACTORY_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_FACTORY_H_
#pragma once

#include "content/browser/webui/web_ui.h"
#include "content/common/content_export.h"

class TabContents;
class GURL;

namespace content {

class BrowserContext;

// Interface for an object which controls which URLs are considered WebUI URLs
// and creates WebUI instances for given URLs.
class CONTENT_EXPORT WebUIFactory {
 public:
  // Returns a WebUI instance for the given URL, or NULL if the URL doesn't
  // correspond to a WebUI.
  virtual WebUI* CreateWebUIForURL(TabContents* source,
                                   const GURL& url) const = 0;

  // Gets the WebUI type for the given URL. This will return kNoWebUI if the
  // corresponding call to CreateWebUIForURL would fail, or something non-NULL
  // if CreateWebUIForURL would succeed.
  virtual WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                     const GURL& url) const = 0;

  // Shorthand for the above, but returns a simple yes/no.
  virtual bool UseWebUIForURL(content::BrowserContext* browser_context,
                              const GURL& url) const = 0;

  // Returns true if the url has a scheme for WebUI. This differs from the above
  // in that it only checks the scheme; it is faster and can be used to
  // determine security policy.
  virtual bool HasWebUIScheme(const GURL& url) const = 0;

  // Returns true if the given URL can be loaded by Web UI system. This allows
  // URLs with WebUI types (as above) and also URLs that can be loaded by
  // normal tabs such as javascript: URLs or about:hang.
  virtual bool IsURLAcceptableForWebUI(content::BrowserContext* browser_context,
                                       const GURL& url) const = 0;

  virtual ~WebUIFactory() {}

  // Helper function to streamline retrieval of the current WebUIFactory. Only
  // to be used in content/. Guaranteed to return non-NULL.
  static WebUIFactory* Get();
};


}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_FACTORY_H_
