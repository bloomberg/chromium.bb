// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_WEB_UI_FACTORY_H_
#define CHROME_BROWSER_DOM_UI_WEB_UI_FACTORY_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/favicon_service.h"

class DOMUI;
class GURL;
class Profile;
class RefCountedMemory;
class TabContents;

// An opaque identifier used to identify a DOMUI. This can only be compared to
// kNoWebUI or other DOMUI types. See GetWebUIType.
typedef void* WebUITypeID;

class WebUIFactory {
 public:
  // A special DOMUI type that signifies that a given page would not use the
  // DOM UI system.
  static const WebUITypeID kNoWebUI;

  // Returns a type identifier indicating what DOMUI we would use for the
  // given URL. This is useful for comparing the potential DOMUIs for two URLs.
  // Returns kNoWebUI if the given URL will not use the DOM UI system.
  static WebUITypeID GetWebUIType(Profile* profile, const GURL& url);

  // Returns true if the given URL's scheme would trigger the DOM UI system.
  // This is a less precise test than UseDONUIForURL, which tells you whether
  // that specific URL matches a known one. This one is faster and can be used
  // to determine security policy.
  static bool HasWebUIScheme(const GURL& url);

  // Returns true if the given URL must use the DOM UI system.
  static bool UseWebUIForURL(Profile* profile, const GURL& url);

  // Returns true if the given URL can be loaded by DOM UI system.  This
  // includes URLs that can be loaded by normal tabs as well, such as
  // javascript: URLs or about:hang.
  static bool IsURLAcceptableForWebUI(Profile* profile, const GURL& url);

  // Allocates a new DOMUI object for the given URL, and returns it. If the URL
  // is not a DOM UI URL, then it will return NULL. When non-NULL, ownership of
  // the returned pointer is passed to the caller.
  static DOMUI* CreateWebUIForURL(TabContents* tab_contents, const GURL& url);

  // Get the favicon for |page_url| and forward the result to the |request|
  // when loaded.
  static void GetFaviconForURL(Profile* profile,
                               FaviconService::GetFaviconRequest* request,
                               const GURL& page_url);

 private:
  // Gets the data for the favicon for a DOMUI page. Returns NULL if the DOMUI
  // does not have a favicon.
  static RefCountedMemory* GetFaviconResourceBytes(Profile* profile,
                                                   const GURL& page_url);

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebUIFactory);
};

#endif  // CHROME_BROWSER_DOM_UI_WEB_UI_FACTORY_H_
