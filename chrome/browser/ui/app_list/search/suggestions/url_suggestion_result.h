// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SUGGESTIONS_URL_SUGGESTION_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SUGGESTIONS_URL_SUGGESTION_RESULT_H_

#include "base/memory/scoped_ptr.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "ui/app_list/search_result.h"

class AppListControllerDelegate;
class GURL;
class Profile;
class SkBitmap;

namespace suggestions {
class SuggestionsService;
}  // namespace suggestions

namespace app_list {

class URLSuggestionResult : public SearchResult {
 public:
  // |suggestion| is copied to a local member, because this SuggestionResult is
  // expected to live longer that |suggestions|.
  URLSuggestionResult(Profile* profile,
                      AppListControllerDelegate* list_controller,
                      suggestions::SuggestionsService* suggestions_service,
                      const suggestions::ChromeSuggestion& suggestion);
  ~URLSuggestionResult() override;

  // SearchResult overrides:
  void Open(int event_flags) override;
  scoped_ptr<SearchResult> Duplicate() override;

  // Refer to SearchResult::set_relevance for documentation.
  using SearchResult::set_relevance;

 private:
  void UpdateIcon();
  void OnSuggestionsThumbnailAvailable(const GURL& url, const SkBitmap* bitmap);

  Profile* profile_;
  AppListControllerDelegate* list_controller_;
  suggestions::SuggestionsService* suggestions_service_;
  suggestions::ChromeSuggestion suggestion_;

  // For callbacks that may be run after destruction.
  base::WeakPtrFactory<URLSuggestionResult> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLSuggestionResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SUGGESTIONS_URL_SUGGESTION_RESULT_H_
