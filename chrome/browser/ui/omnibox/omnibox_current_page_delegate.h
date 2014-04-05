// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CURRENT_PAGE_DELEGATE_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CURRENT_PAGE_DELEGATE_H_

#include "base/basictypes.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/omnibox_focus_state.h"
#include "ui/base/window_open_disposition.h"

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

  // Returns true if the visible entry is a New Tab Page rendered by Instant.
  virtual bool IsInstantNTP() const = 0;

  // Returns true if the committed entry is a search results page.
  virtual bool IsSearchResultsPage() const = 0;

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
                                       const AutocompleteMatch& match,
                                       WindowOpenDisposition disposition) = 0;

  // Called to notify clients that the omnibox input state has changed.
  virtual void OnInputStateChanged() = 0;

  // Called to notify clients that the omnibox focus state has changed.
  virtual void OnFocusChanged(OmniboxFocusState state,
                              OmniboxFocusChangeReason reason) = 0;

  // Performs prerendering for |match|.
  virtual void DoPrerender(const AutocompleteMatch& match) = 0;

  // Sends the current SearchProvider suggestion to the Instant page if any.
  virtual void SetSuggestionToPrefetch(const InstantSuggestion& suggestion) = 0;
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CURRENT_PAGE_DELEGATE_H_
