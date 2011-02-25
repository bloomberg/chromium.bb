// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_FACTORY_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_FACTORY_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/favicon_service.h"

class WebUI;
class GURL;
class Profile;
class RefCountedMemory;
class TabContents;

// An opaque identifier used to identify a WebUI. This can only be compared to
// kNoWebUI or other WebUI types. See GetWebUIType.
typedef void* WebUITypeID;

class WebUIFactory {
 public:
  // A special WebUI type that signifies that a given page would not use the
  // Web UI system.
  static const WebUITypeID kNoWebUI;

  // Returns a type identifier indicating what WebUI we would use for the
  // given URL. This is useful for comparing the potential WebUIs for two URLs.
  // Returns kNoWebUI if the given URL will not use the Web UI system.
  static WebUITypeID GetWebUIType(Profile* profile, const GURL& url);

  // Returns true if the given URL's scheme would trigger the Web UI system.
  // This is a less precise test than UseDONUIForURL, which tells you whether
  // that specific URL matches a known one. This one is faster and can be used
  // to determine security policy.
  static bool HasWebUIScheme(const GURL& url);

  // Returns true if the given URL must use the Web UI system.
  static bool UseWebUIForURL(Profile* profile, const GURL& url);

  // Returns true if the given URL can be loaded by Web UI system.  This
  // includes URLs that can be loaded by normal tabs as well, such as
  // javascript: URLs or about:hang.
  static bool IsURLAcceptableForWebUI(Profile* profile, const GURL& url);

  // Allocates a new WebUI object for the given URL, and returns it. If the URL
  // is not a Web UI URL, then it will return NULL. When non-NULL, ownership of
  // the returned pointer is passed to the caller.
  static WebUI* CreateWebUIForURL(TabContents* tab_contents, const GURL& url);

  // Get the favicon for |page_url| and forward the result to the |request|
  // when loaded.
  static void GetFaviconForURL(Profile* profile,
                               FaviconService::GetFaviconRequest* request,
                               const GURL& page_url);

 private:
  // Gets the data for the favicon for a WebUI page. Returns NULL if the WebUI
  // does not have a favicon.
  static RefCountedMemory* GetFaviconResourceBytes(Profile* profile,
                                                   const GURL& page_url);

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebUIFactory);
};

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_FACTORY_H_
