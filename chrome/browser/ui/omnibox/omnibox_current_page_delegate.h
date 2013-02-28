// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CURRENT_PAGE_DELEGATE_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CURRENT_PAGE_DELEGATE_H_

#include "base/basictypes.h"

class GURL;
class SessionID;
class TemplateURL;
struct AutocompleteMatch;

namespace content {
class NavigationController;
}

// A class that abstracts omnibox functionality that depends on the current
// tab/page (e.g., getting information about the current page, retrieving
// objects associated with the current tab, or performing operations that rely
// on such objects under the hood).
class OmniboxCurrentPageDelegate {
 public:
  virtual ~OmniboxCurrentPageDelegate() {}

  // Returns whether there is any associated current page.  For example, during
  // startup or shutdown, the omnibox may exist but have no attached page.
  virtual bool CurrentPageExists() const = 0;

  // Returns the URL of the current page.
  virtual const GURL& GetURL() const = 0;

  // Returns whether the current page is loading.
  virtual bool IsLoading() const = 0;

  // Returns the NavigationController for the current page.
  virtual content::NavigationController& GetNavigationController() const = 0;

  // Returns the session ID of the current page.
  virtual const SessionID& GetSessionID() const = 0;

  // Checks whether |template_url| is an extension keyword; if so, asks the
  // ExtensionOmniboxEventRouter to process |match| for it and returns true.
  // Otherwise returns false.
  virtual bool ProcessExtensionKeyword(TemplateURL* template_url,
                                       const AutocompleteMatch& match) = 0;

  // Notifies the SearchTabHelper, if one exists, of relevant changes to the
  // omnibox state.
  virtual void NotifySearchTabHelper(bool user_input_in_progress,
                                     bool cancelling) = 0;

  // Performs prerendering for |match|.
  virtual void DoPrerender(const AutocompleteMatch& match) = 0;
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CURRENT_PAGE_DELEGATE_H_
