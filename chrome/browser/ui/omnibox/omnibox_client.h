// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CLIENT_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CLIENT_H_

#include "base/basictypes.h"
#include "chrome/common/instant_types.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "ui/base/window_open_disposition.h"

class GURL;
class SessionID;
class TemplateURL;
struct AutocompleteMatch;
struct OmniboxLog;

namespace content {
class NavigationController;
}

// Interface that allows the omnibox component to interact with its embedder
// (e.g., getting information about the current page, retrieving objects
// associated with the current tab, or performing operations that rely on such
// objects under the hood).
class OmniboxClient {
 public:
  virtual ~OmniboxClient() {}

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

  // Returns whether paste-and-go functionality is enabled.
  virtual bool IsPasteAndGoEnabled() const = 0;

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

  // Called to notify clients that a URL was opened from the omnibox.
  // TODO(blundell): Eliminate this method in favor of having all //chrome-
  // level listeners of the OMNIBOX_OPENED_URL instead observe OmniboxEditModel.
  // Note that this is not trivial because (a) most of those listeners listen
  // for the notification from all Profiles and (b) the notification is also
  // sent from AutocompleteControllerAndroid, and it's unclear which listeners
  // are also listening from it being sent from there.
  virtual void OnURLOpenedFromOmnibox(OmniboxLog* log) = 0;

  // Performs prerendering for |match|.
  virtual void DoPrerender(const AutocompleteMatch& match) = 0;

  // Sends the current SearchProvider suggestion to the Instant page if any.
  virtual void SetSuggestionToPrefetch(const InstantSuggestion& suggestion) = 0;
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CLIENT_H_
