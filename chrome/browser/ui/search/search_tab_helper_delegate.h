// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_DELEGATE_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_DELEGATE_H_

#include <set>
#include <string>

#include "ui/base/window_open_disposition.h"

namespace content {
class WebContents;
}

class GURL;
class OmniboxView;

// Objects implement this interface to get notified about changes in the
// SearchTabHelper and to provide necessary functionality.
class SearchTabHelperDelegate {
 public:
  // Navigates the page to |url| in response to a click event. Usually used
  // by the page to navigate to privileged destinations (e.g. chrome:// URLs) or
  // to navigate to URLs that are hidden from the page using Restricted IDs
  // (rid in the API).
  //
  // TODO(kmadhusu): Handle search results page navigations to privileged
  // destinations in a seperate function. This function should handle only the
  // new tab page thumbnail click events.
  virtual void NavigateOnThumbnailClick(const GURL& url,
                                        WindowOpenDisposition disposition,
                                        content::WebContents* source_contents);

  // Invoked when the |web_contents| no longer supports Instant.
  virtual void OnWebContentsInstantSupportDisabled(
      const content::WebContents* web_contents);

  // Returns the OmniboxView or NULL if not available.
  virtual OmniboxView* GetOmniboxView();

  // Returns a set containing the canonical URLs of the currently open tabs.
  virtual std::set<std::string> GetOpenUrls();

 protected:
  virtual ~SearchTabHelperDelegate();
};

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_DELEGATE_H_
