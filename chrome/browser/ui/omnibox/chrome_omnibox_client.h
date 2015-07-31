// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_CLIENT_H_
#define CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_CLIENT_H_

#include "base/compiler_specific.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"
#include "chrome/browser/ui/omnibox/omnibox_client.h"
#include "chrome/common/instant_types.h"

class OmniboxEditController;
class Profile;

class ChromeOmniboxClient : public OmniboxClient {
 public:
  ChromeOmniboxClient(OmniboxEditController* controller, Profile* profile);
  ~ChromeOmniboxClient() override;

  // OmniboxClient.
  scoped_ptr<AutocompleteProviderClient>
      CreateAutocompleteProviderClient() override;
  bool CurrentPageExists() const override;
  const GURL& GetURL() const override;
  bool IsInstantNTP() const override;
  bool IsSearchResultsPage() const override;
  bool IsLoading() const override;
  bool IsPasteAndGoEnabled() const override;
  content::NavigationController& GetNavigationController() const override;
  const SessionID& GetSessionID() const override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  TemplateURLService* GetTemplateURLService() override;
  gfx::Image GetIconIfExtensionMatch(
      const AutocompleteMatch& match) const override;
  bool ProcessExtensionKeyword(TemplateURL* template_url,
                               const AutocompleteMatch& match,
                               WindowOpenDisposition disposition) override;
  void OnInputStateChanged() override;
  void OnFocusChanged(OmniboxFocusState state,
                      OmniboxFocusChangeReason reason) override;
  void OnResultChanged(const AutocompleteResult& result,
                       bool default_match_changed,
                       const base::Callback<void(const SkBitmap& bitmap)>&
                           on_bitmap_fetched) override;
  void OnCurrentMatchChanged(const AutocompleteMatch& match) override;
  void OnURLOpenedFromOmnibox(OmniboxLog* log) override;
  void DoPrerender(const AutocompleteMatch& match) override;

  // TODO(blundell): Make private once OmniboxEditModel no longer refers to it.
  void DoPreconnect(const AutocompleteMatch& match) override;

 private:
  // Sends the current SearchProvider suggestion to the Instant page if any.
  void SetSuggestionToPrefetch(const InstantSuggestion& suggestion);

  void OnBitmapFetched(const BitmapFetchedCallback& callback,
                       const SkBitmap& bitmap);

  OmniboxEditController* controller_;
  Profile* profile_;
  BitmapFetcherService::RequestId request_id_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOmniboxClient);
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_CLIENT_H_
