// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_DELEGATE_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_DELEGATE_H_

namespace content {
class WebContents;
}

class OmniboxView;

// Objects implement this interface to get notified about changes in the
// SearchTabHelper and to provide necessary functionality.
class SearchTabHelperDelegate {
 public:
  // Invoked when the |web_contents| no longer supports Instant.
  virtual void OnWebContentsInstantSupportDisabled(
      const content::WebContents* web_contents);

  // Returns the OmniboxView or NULL if not available.
  virtual OmniboxView* GetOmniboxView();

 protected:
  virtual ~SearchTabHelperDelegate();
};

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_TAB_HELPER_DELEGATE_H_
