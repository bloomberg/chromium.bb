// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CLIENT_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CLIENT_H_

#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_navigation_observer.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "ui/base/window_open_disposition.h"

class AutocompleteResult;
class GURL;
class SessionID;
class TemplateURL;
class TemplateURLService;
struct AutocompleteMatch;
struct OmniboxLog;

namespace bookmarks {
class BookmarkModel;
}

namespace gfx {
class Image;
}

typedef base::Callback<void(const SkBitmap& bitmap)> BitmapFetchedCallback;

// Interface that allows the omnibox component to interact with its embedder
// (e.g., getting information about the current page, retrieving objects
// associated with the current tab, or performing operations that rely on such
// objects under the hood).
class OmniboxClient {
 public:
  virtual ~OmniboxClient() {}

  // Returns an AutocompleteProviderClient specific to the embedder context.
  virtual std::unique_ptr<AutocompleteProviderClient>
  CreateAutocompleteProviderClient() = 0;

  // Returns an OmniboxNavigationObserver specific to the embedder context. May
  // return null if the embedder has no need to observe omnibox navigations.
  virtual std::unique_ptr<OmniboxNavigationObserver>
  CreateOmniboxNavigationObserver(const base::string16& text,
                                  const AutocompleteMatch& match,
                                  const AutocompleteMatch& alternate_nav_match);

  // Returns whether there is any associated current page.  For example, during
  // startup or shutdown, the omnibox may exist but have no attached page.
  virtual bool CurrentPageExists() const;

  // Returns the URL of the current page.
  virtual const GURL& GetURL() const;

  // Returns the title of the current page.
  virtual const base::string16& GetTitle() const;

  // Returns the favicon of the current page.
  virtual gfx::Image GetFavicon() const;

  // Returns true if the visible entry is a New Tab Page rendered by Instant.
  virtual bool IsInstantNTP() const;

  // Returns true if the committed entry is a search results page.
  virtual bool IsSearchResultsPage() const;

  // Returns whether the current page is loading.
  virtual bool IsLoading() const;

  // Returns whether paste-and-go functionality is enabled.
  virtual bool IsPasteAndGoEnabled() const;

  // Returns whether |url| corresponds to the new tab page.
  virtual bool IsNewTabPage(const GURL& url) const;

  // Returns whether |url| corresponds to the user's home page.
  virtual bool IsHomePage(const GURL& url) const;

  // Returns the session ID of the current page.
  virtual const SessionID& GetSessionID() const = 0;

  virtual bookmarks::BookmarkModel* GetBookmarkModel();
  virtual TemplateURLService* GetTemplateURLService();
  virtual const AutocompleteSchemeClassifier& GetSchemeClassifier() const = 0;
  virtual AutocompleteClassifier* GetAutocompleteClassifier();

  // Returns the icon corresponding to |match| if match is an extension match
  // and an empty icon otherwise.
  virtual gfx::Image GetIconIfExtensionMatch(
      const AutocompleteMatch& match) const;

  // Checks whether |template_url| is an extension keyword; if so, asks the
  // ExtensionOmniboxEventRouter to process |match| for it and returns true.
  // Otherwise returns false. |observer| is the OmniboxNavigationObserver
  // that was created by CreateOmniboxNavigationObserver() for |match|; in some
  // embedding contexts, processing an extension keyword involves invoking
  // action on this observer.
  virtual bool ProcessExtensionKeyword(const TemplateURL* template_url,
                                       const AutocompleteMatch& match,
                                       WindowOpenDisposition disposition,
                                       OmniboxNavigationObserver* observer);

  // Called to notify clients that the omnibox input state has changed.
  virtual void OnInputStateChanged() {}

  // Called to notify clients that the omnibox focus state has changed.
  virtual void OnFocusChanged(OmniboxFocusState state,
                              OmniboxFocusChangeReason reason) {}

  // Called when the autocomplete result has changed. If the embedder supports
  // fetching of bitmaps for URLs (not all embedders do), |on_bitmap_fetched|
  // will be called when the bitmap has been fetched.
  virtual void OnResultChanged(const AutocompleteResult& result,
                               bool default_match_changed,
                               const BitmapFetchedCallback& on_bitmap_fetched) {
  }

  // Called when the current autocomplete match has changed.
  virtual void OnCurrentMatchChanged(const AutocompleteMatch& match) {}

  // Called when the text may have changed in the edit.
  virtual void OnTextChanged(const AutocompleteMatch& current_match,
                             bool user_input_in_progress,
                             base::string16& user_text,
                             const AutocompleteResult& result,
                             bool is_popup_open,
                             bool has_focus) {}

  // Called when input has been accepted.
  virtual void OnInputAccepted(const AutocompleteMatch& match) {}

  // Called when the edit model is being reverted back to its unedited state.
  virtual void OnRevert() {}

  // Called to notify clients that a URL was opened from the omnibox.
  virtual void OnURLOpenedFromOmnibox(OmniboxLog* log) {}

  // Called when a bookmark is launched from the omnibox.
  virtual void OnBookmarkLaunched() {}

  // Discards the state for all pending and transient navigations.
  virtual void DiscardNonCommittedNavigations() {}
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_CLIENT_H_
