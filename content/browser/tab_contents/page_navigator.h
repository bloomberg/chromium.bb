// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PageNavigator defines an interface that can be used to express the user's
// intention to navigate to a particular URL.  The implementing class should
// perform the navigation.

#ifndef CONTENT_BROWSER_TAB_CONTENTS_PAGE_NAVIGATOR_H_
#define CONTENT_BROWSER_TAB_CONTENTS_PAGE_NAVIGATOR_H_
#pragma once

#include <string>

#include "content/common/content_export.h"
#include "content/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/window_open_disposition.h"

class TabContents;

struct CONTENT_EXPORT OpenURLParams {
  OpenURLParams(const GURL& url,
                const GURL& referrer,
                WindowOpenDisposition disposition,
                PageTransition::Type transition);
  ~OpenURLParams();
class TabContents;

  // The URL/referrer to be opened.
  GURL url;
  GURL referrer;

  // The disposition requested by the navigation source.
  WindowOpenDisposition disposition;

  // The transition type of navigation.
  PageTransition::Type transition;

  // The override encoding of the URL contents to be opened.
  std::string override_encoding;

 private:
  OpenURLParams();
};

class CONTENT_EXPORT PageNavigator {
 public:
  // Deprecated. Please use the one-argument variant instead.
  // TODO(adriansc): Remove this method when refactoring changed all call sites.
  virtual TabContents* OpenURL(const GURL& url,
                               const GURL& referrer,
                               WindowOpenDisposition disposition,
                               PageTransition::Type transition) = 0;

  // Opens a URL with the given disposition.  The transition specifies how this
  // navigation should be recorded in the history system (for example, typed).
  // Returns the TabContents the URL is opened in, or NULL if the URL wasn't
  // opened immediately.
  virtual TabContents* OpenURL(const OpenURLParams& params) = 0;

 protected:
  virtual ~PageNavigator() {}
};

#endif  // CONTENT_BROWSER_TAB_CONTENTS_PAGE_NAVIGATOR_H_
