// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/suggestions/url_suggestion_result.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/search_util.h"
#include "components/suggestions/suggestions_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace app_list {

URLSuggestionResult::URLSuggestionResult(
    Profile* profile, AppListControllerDelegate* list_controller,
    suggestions::SuggestionsService* suggestions_service,
    const suggestions::ChromeSuggestion& suggestion)
    : profile_(profile),
      list_controller_(list_controller),
      suggestions_service_(suggestions_service),
      suggestion_(suggestion),
      weak_ptr_factory_(this) {
  set_id(suggestion_.url());
  set_title(base::UTF8ToUTF16(suggestion_.title()));
  set_display_type(DISPLAY_TILE);

  UpdateIcon();
}

URLSuggestionResult::~URLSuggestionResult() {}

void URLSuggestionResult::Open(int event_flags) {
  RecordHistogram(SUGGESTIONS_SEARCH_RESULT);
  list_controller_->OpenURL(profile_, GURL(suggestion_.url()),
                            ui::PageTransition::PAGE_TRANSITION_LINK,
                            ui::DispositionFromEventFlags(event_flags));
}

scoped_ptr<SearchResult> URLSuggestionResult::Duplicate() {
  URLSuggestionResult* new_result = new URLSuggestionResult(
      profile_, list_controller_, suggestions_service_, suggestion_);
  new_result->set_relevance(relevance());
  return scoped_ptr<SearchResult>(new_result);
}

void URLSuggestionResult::UpdateIcon() {
  if (suggestions_service_) {
    suggestions_service_->GetPageThumbnail(
        GURL(suggestion_.url()),
        base::Bind(&URLSuggestionResult::OnSuggestionsThumbnailAvailable,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void URLSuggestionResult::OnSuggestionsThumbnailAvailable(
    const GURL& url, const SkBitmap* bitmap) {
  if (bitmap) {
    SetIcon(gfx::ImageSkia::CreateFrom1xBitmap(*bitmap));
  } else {
    // There is no image for this suggestion. Disable being shown on the start
    // screen.
    set_display_type(DISPLAY_NONE);
  }
}

}  // namespace app_list
