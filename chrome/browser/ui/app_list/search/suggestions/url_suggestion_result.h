// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SUGGESTIONS_URL_SUGGESTION_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SUGGESTIONS_URL_SUGGESTION_RESULT_H_

#include "base/memory/scoped_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "ui/app_list/search_result.h"

class AppListControllerDelegate;
class FaviconService;
class GURL;
class Profile;
class SkBitmap;

namespace favicon_base {
struct FaviconRawBitmapResult;
}  // namespace favicon_base

namespace suggestions {
class SuggestionsService;
}  // namespace suggestions

namespace app_list {

class URLSuggestionResult : public SearchResult {
 public:
  // |suggestion| is copied to a local member, because this URLSuggestionResult
  // is expected to live longer that |suggestion|.
  URLSuggestionResult(Profile* profile,
                      AppListControllerDelegate* list_controller,
                      FaviconService* favicon_service,
                      suggestions::SuggestionsService* suggestions_service,
                      const suggestions::ChromeSuggestion& suggestion);
  ~URLSuggestionResult() override;

  // SearchResult overrides:
  void Open(int event_flags) override;
  scoped_ptr<SearchResult> Duplicate() const override;

  // Refer to SearchResult::set_relevance for documentation.
  using SearchResult::set_relevance;

 private:
  void UpdateIcon();
  void OnDidGetIcon(
      const favicon_base::FaviconRawBitmapResult& bitmap_result);
  void OnSuggestionsThumbnailAvailable(const GURL& url, const SkBitmap* bitmap);

  Profile* profile_;
  AppListControllerDelegate* list_controller_;
  FaviconService* favicon_service_;
  suggestions::SuggestionsService* suggestions_service_;
  suggestions::ChromeSuggestion suggestion_;
  scoped_ptr<base::CancelableTaskTracker> cancelable_task_tracker_;

  // For callbacks that may be run after destruction.
  base::WeakPtrFactory<URLSuggestionResult> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLSuggestionResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SUGGESTIONS_URL_SUGGESTION_RESULT_H_
