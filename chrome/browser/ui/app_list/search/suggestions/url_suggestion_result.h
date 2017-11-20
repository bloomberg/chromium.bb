// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SUGGESTIONS_URL_SUGGESTION_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SUGGESTIONS_URL_SUGGESTION_RESULT_H_

#include <memory>

#include "ash/app_list/model/search_result.h"
#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/suggestions/proto/suggestions.pb.h"

class AppListControllerDelegate;
class GURL;
class Profile;

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace favicon_base {
struct FaviconRawBitmapResult;
}  // namespace favicon_base

namespace gfx {
class Image;
}  // namespace gfx

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
                      favicon::FaviconService* favicon_service,
                      suggestions::SuggestionsService* suggestions_service,
                      const suggestions::ChromeSuggestion& suggestion);
  ~URLSuggestionResult() override;

  // SearchResult overrides:
  void Open(int event_flags) override;
  std::unique_ptr<SearchResult> Duplicate() const override;

  // Refer to SearchResult::set_relevance for documentation.
  using SearchResult::set_relevance;

 private:
  void UpdateIcon();
  void OnDidGetIcon(
      const favicon_base::FaviconRawBitmapResult& bitmap_result);
  void OnSuggestionsThumbnailAvailable(const GURL& url,
                                       const gfx::Image& image);

  Profile* profile_;
  AppListControllerDelegate* list_controller_;
  favicon::FaviconService* favicon_service_;
  suggestions::SuggestionsService* suggestions_service_;
  suggestions::ChromeSuggestion suggestion_;
  std::unique_ptr<base::CancelableTaskTracker> cancelable_task_tracker_;

  // For callbacks that may be run after destruction.
  base::WeakPtrFactory<URLSuggestionResult> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLSuggestionResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SUGGESTIONS_URL_SUGGESTION_RESULT_H_
