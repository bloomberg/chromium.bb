// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PageNavigator defines an interface that can be used to express the user's
// intention to navigate to a particular URL.  The implementing class should
// perform the navigation.

#ifndef CONTENT_PUBLIC_BROWSER_PAGE_NAVIGATOR_H_
#define CONTENT_PUBLIC_BROWSER_PAGE_NAVIGATOR_H_

#include <string>

#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "googleurl/src/gurl.h"
#include "ui/base/window_open_disposition.h"

namespace content {

class WebContents;

struct CONTENT_EXPORT OpenURLParams {
  OpenURLParams(const GURL& url,
                const Referrer& referrer,
                WindowOpenDisposition disposition,
                PageTransition transition,
                bool is_renderer_initiated);
  OpenURLParams(const GURL& url,
                const Referrer& referrer,
                int64 source_frame_id,
                WindowOpenDisposition disposition,
                PageTransition transition,
                bool is_renderer_initiated);
  ~OpenURLParams();

  // The URL/referrer to be opened.
  GURL url;
  Referrer referrer;

  // Extra headers to add to the request for this page.  Headers are
  // represented as "<name>: <value>" and separated by \r\n.  The entire string
  // is terminated by \r\n.  May be empty if no extra headers are needed.
  std::string extra_headers;

  // The source frame id or -1 to indicate the main frame.
  int64 source_frame_id;

  // The disposition requested by the navigation source.
  WindowOpenDisposition disposition;

  // The transition type of navigation.
  PageTransition transition;

  // Whether this navigation is initiated by the renderer process.
  bool is_renderer_initiated;

  // The override encoding of the URL contents to be opened.
  std::string override_encoding;

  // Reference to the old request id in case this is a navigation that is being
  // transferred to a new renderer.
  GlobalRequestID transferred_global_request_id;

  // Indicates whether this navigation involves a cross-process redirect,
  // in which case it should replace the current navigation entry.
  bool is_cross_site_redirect;

 private:
  OpenURLParams();
};

class PageNavigator {
 public:
  virtual ~PageNavigator() {}

  // Opens a URL with the given disposition.  The transition specifies how this
  // navigation should be recorded in the history system (for example, typed).
  // Returns the WebContents the URL is opened in, or NULL if the URL wasn't
  // opened immediately.
  virtual WebContents* OpenURL(const OpenURLParams& params) = 0;
};

}

#endif  // CONTENT_PUBLIC_BROWSER_PAGE_NAVIGATOR_H_
