// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_CLIENT_H_
#define CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"
#include "chrome/common/search/instant_types.h"
#include "components/omnibox/browser/omnibox_client.h"

class ChromeOmniboxEditController;
class OmniboxEditController;
class Profile;

class ChromeOmniboxClient : public OmniboxClient {
 public:
  ChromeOmniboxClient(OmniboxEditController* controller, Profile* profile);
  ~ChromeOmniboxClient() override;

  // OmniboxClient.
  std::unique_ptr<AutocompleteProviderClient> CreateAutocompleteProviderClient()
      override;
  std::unique_ptr<OmniboxNavigationObserver> CreateOmniboxNavigationObserver(
      const base::string16& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternate_nav_match) override;
  bool CurrentPageExists() const override;
  const GURL& GetURL() const override;
  const base::string16& GetTitle() const override;
  gfx::Image GetFavicon() const override;
  bool IsInstantNTP() const override;
  bool IsSearchResultsPage() const override;
  bool IsLoading() const override;
  bool IsPasteAndGoEnabled() const override;
  bool IsNewTabPage(const std::string& url) const override;
  bool IsHomePage(const std::string& url) const override;
  const SessionID& GetSessionID() const override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  TemplateURLService* GetTemplateURLService() override;
  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override;
  AutocompleteClassifier* GetAutocompleteClassifier() override;
  gfx::Image GetIconIfExtensionMatch(
      const AutocompleteMatch& match) const override;
  bool ProcessExtensionKeyword(TemplateURL* template_url,
                               const AutocompleteMatch& match,
                               WindowOpenDisposition disposition,
                               OmniboxNavigationObserver* observer) override;
  void OnInputStateChanged() override;
  void OnFocusChanged(OmniboxFocusState state,
                      OmniboxFocusChangeReason reason) override;
  void OnResultChanged(const AutocompleteResult& result,
                       bool default_match_changed,
                       const base::Callback<void(const SkBitmap& bitmap)>&
                           on_bitmap_fetched) override;
  void OnCurrentMatchChanged(const AutocompleteMatch& match) override;
  void OnTextChanged(const AutocompleteMatch& current_match,
                     bool user_input_in_progress,
                     base::string16& user_text,
                     const AutocompleteResult& result,
                     bool is_popup_open,
                     bool has_focus) override;
  void OnInputAccepted(const AutocompleteMatch& match) override;
  void OnRevert() override;
  void OnURLOpenedFromOmnibox(OmniboxLog* log) override;
  void OnBookmarkLaunched() override;
  void DiscardNonCommittedNavigations() override;

 private:
  // Performs prerendering for |match|.
  void DoPrerender(const AutocompleteMatch& match);

  // Performs preconnection for |match|.
  void DoPreconnect(const AutocompleteMatch& match);

  // Sends the current SearchProvider suggestion to the Instant page if any.
  void SetSuggestionToPrefetch(const InstantSuggestion& suggestion);

  void OnBitmapFetched(const BitmapFetchedCallback& callback,
                       const SkBitmap& bitmap);

  ChromeOmniboxEditController* controller_;
  Profile* profile_;
  ChromeAutocompleteSchemeClassifier scheme_classifier_;
  BitmapFetcherService::RequestId request_id_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOmniboxClient);
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_CLIENT_H_
