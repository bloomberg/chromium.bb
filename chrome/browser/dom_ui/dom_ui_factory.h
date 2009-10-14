// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_DOM_UI_FACTORY_H_
#define CHROME_BROWSER_DOM_UI_DOM_UI_FACTORY_H_

#include <vector>

class DOMUI;
class GURL;
class TabContents;

// An opaque identifier used to identify a DOMUI. This can only be compared to
// kNoDOMUI or other DOMUI types. See GetDOMUIType.
typedef void* DOMUITypeID;

class DOMUIFactory {
 public:
  // A special DOMUI type that signifies that a given page would not use the
  // DOM UI system.
  static const DOMUITypeID kNoDOMUI;

  // Returns a type identifier indicating what DOMUI we would use for the
  // given URL. This is useful for comparing the potential DOMUIs for two URLs.
  // Returns kNoDOMUI if the given URL will not use the DOM UI system.
  static DOMUITypeID GetDOMUIType(const GURL& url);

  // Returns true if the given URL's scheme would trigger the DOM UI system.
  // This is a less precise test than UseDONUIForURL, which tells you whether
  // that specific URL matches a known one. This one is faster and can be used
  // to determine security policy.
  static bool HasDOMUIScheme(const GURL& url);

  // Returns true if the given URL will use the DOM UI system.
  static bool UseDOMUIForURL(const GURL& url);

  // Allocates a new DOMUI object for the given URL, and returns it. If the URL
  // is not a DOM UI URL, then it will return NULL. When non-NULL, ownership of
  // the returned pointer is passed to the caller.
  static DOMUI* CreateDOMUIForURL(TabContents* tab_contents, const GURL& url);

  // Gets the data for the favicon for a DOMUI page. Returns false if the DOMUI
  // does not have a favicon.
  static bool GetFaviconResourceBytes(const GURL& page_url,
                                      std::vector<unsigned char>* bytes);

 private:
  // Class is for scoping only.
  DOMUIFactory() {}
};

#endif  // CHROME_BROWSER_DOM_UI_DOM_UI_FACTORY_H_
