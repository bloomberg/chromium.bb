// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We handle some special browser-level URLs (like "about:version")
// before they're handed to a renderer.  This lets us do the URL handling
// on the browser side (which has access to more information than the
// renderers do) as well as sidestep the risk of exposing data to
// random web pages (because from the resource loader's perspective, these
// URL schemes don't exist).

#ifndef CHROME_BROWSER_BROWSER_URL_HANDLER_H_
#define CHROME_BROWSER_BROWSER_URL_HANDLER_H_
#pragma once

#include <vector>
#include <utility>

class GURL;
class Profile;

// BrowserURLHandler manages the list of all special URLs and manages
// dispatching the URL handling to registered handlers.
class BrowserURLHandler {
 public:
  // The type of functions that can process a URL.
  // If a handler handles |url|, it should :
  // - optionally modify |url| to the URL that should be sent to the renderer
  // If the URL is not handled by a handler, it should return false.
  typedef bool (*URLHandler)(GURL* url, Profile* profile);

  // RewriteURLIfNecessary gives all registered URLHandlers a shot at processing
  // the given URL, and modifies it in place.
  // If the original URL needs to be adjusted if the modified URL is redirected,
  // this function sets |reverse_on_redirect| to true.
  static void RewriteURLIfNecessary(GURL* url, Profile* profile,
                                    bool* reverse_on_redirect);

  // Reverses the rewriting that was done for |original| using the new |url|.
  static bool ReverseURLRewrite(GURL* url, const GURL& original,
                                Profile* profile);

  // We initialize the list of url_handlers_ lazily the first time
  // RewriteURLIfNecessary is called.
  static void InitURLHandlers();

 private:
  // The list of known URLHandlers, optionally with reverse-rewriters.
  typedef std::pair<URLHandler, URLHandler> HandlerPair;
  static std::vector<HandlerPair> url_handlers_;
};

#endif  // CHROME_BROWSER_BROWSER_URL_HANDLER_H_
