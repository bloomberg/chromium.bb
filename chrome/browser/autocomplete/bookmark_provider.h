// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_BOOKMARK_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_BOOKMARK_PROVIDER_H_

#include <string>

#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/history/snippet.h"

class BookmarkModel;
class Profile;

// This class is an autocomplete provider which quickly (and synchronously)
// provides autocomplete suggestions based on the titles of bookmarks. Page
// titles, URLs, and typed and visit counts of the bookmarks are not currently
// taken into consideration as those factors will have been used by the
// HistoryQuickProvider (HQP) while identifying suggestions.
//
// TODO(mrossetti): Propose a way to coordinate with the HQP the taking of typed
// and visit counts and last visit dates, etc. into consideration while scoring.
class BookmarkProvider : public AutocompleteProvider {
 public:
  BookmarkProvider(AutocompleteProviderListener* listener, Profile* profile);

  // When |minimal_changes| is true short circuit any additional searching and
  // leave the previous matches for this provider unchanged, otherwise perform
  // a complete search for |input| across all bookmark titles.
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes) OVERRIDE;

  // Sets the BookmarkModel for unit tests.
  void set_bookmark_model_for_testing(BookmarkModel* bookmark_model) {
    bookmark_model_ = bookmark_model;
  }

 private:
  virtual ~BookmarkProvider();

  // Performs the actual matching of |input| over the bookmarks and fills in
  // |matches_|. If |best_match| then only suggest the single best match,
  // otherwise suggest the top |kMaxMatches| matches.
  void DoAutocomplete(const AutocompleteInput& input, bool best_match);

  // Compose an AutocompleteMatch based on |title_match| that has 1) the URL of
  // title_match's bookmark, and 2) the bookmark's title, not the URL's page
  // title, as the description.
  AutocompleteMatch TitleMatchToACMatch(
      const bookmark_utils::TitleMatch& title_match);

  // Converts |positions| into ACMatchClassifications and returns the
  // classifications. |text_length| is used to determine the need to add an
  // 'unhighlighted' classification span so the tail of the source string
  // properly highlighted.
  static ACMatchClassifications ClassificationsFromMatch(
      const Snippet::MatchPositions& positions,
      size_t text_length);

  BookmarkModel* bookmark_model_;

  // Languages used during the URL formatting.
  std::string languages_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_BOOKMARK_PROVIDER_H_
