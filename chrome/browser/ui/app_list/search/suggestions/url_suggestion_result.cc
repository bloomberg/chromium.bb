// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/suggestions/url_suggestion_result.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/search_util.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"
#include "components/suggestions/suggestions_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace app_list {

URLSuggestionResult::URLSuggestionResult(
    Profile* profile, AppListControllerDelegate* list_controller,
    FaviconService* favicon_service,
    suggestions::SuggestionsService* suggestions_service,
    const suggestions::ChromeSuggestion& suggestion)
    : profile_(profile),
      list_controller_(list_controller),
      favicon_service_(favicon_service),
      suggestions_service_(suggestions_service),
      suggestion_(suggestion),
      cancelable_task_tracker_(new base::CancelableTaskTracker()),
      weak_ptr_factory_(this) {
  set_id(suggestion_.url());
  set_title(base::UTF8ToUTF16(suggestion_.title()));
  set_display_type(DISPLAY_RECOMMENDATION);

  UpdateIcon();
}

URLSuggestionResult::~URLSuggestionResult() {}

void URLSuggestionResult::Open(int event_flags) {
  RecordHistogram(SUGGESTIONS_SEARCH_RESULT);
  list_controller_->OpenURL(profile_, GURL(suggestion_.url()),
                            ui::PageTransition::PAGE_TRANSITION_LINK,
                            ui::DispositionFromEventFlags(event_flags));
}

scoped_ptr<SearchResult> URLSuggestionResult::Duplicate() const {
  URLSuggestionResult* new_result = new URLSuggestionResult(
      profile_, list_controller_, favicon_service_, suggestions_service_,
      suggestion_);
  new_result->set_relevance(relevance());
  return scoped_ptr<SearchResult>(new_result);
}

void URLSuggestionResult::UpdateIcon() {
  std::vector<int> icon_types;
  icon_types.push_back(favicon_base::IconType::FAVICON);
  icon_types.push_back(favicon_base::IconType::TOUCH_ICON);
  icon_types.push_back(favicon_base::IconType::TOUCH_PRECOMPOSED_ICON);

  if (favicon_service_) {
    // NOTE: Favicons with size < kMinimumDesiredSizePixels are still returned.
    favicon_service_->GetLargestRawFaviconForPageURL(
        GURL(suggestion_.url()), icon_types, kTileIconSize,
        base::Bind(&URLSuggestionResult::OnDidGetIcon, base::Unretained(this)),
        cancelable_task_tracker_.get());
  }
}

void URLSuggestionResult::OnDidGetIcon(
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  if (!bitmap_result.is_valid()) {
    if (suggestions_service_) {
      suggestions_service_->GetPageThumbnail(
          GURL(suggestion_.url()),
          base::Bind(&URLSuggestionResult::OnSuggestionsThumbnailAvailable,
                     weak_ptr_factory_.GetWeakPtr()));
    } else {
      set_display_type(DISPLAY_NONE);
    }
    return;
  }

  // |bitmap_result| is valid.
  SkBitmap bitmap;
  if (gfx::PNGCodec::Decode(bitmap_result.bitmap_data->front(),
                            bitmap_result.bitmap_data->size(), &bitmap)) {
    gfx::ImageSkia image_skia;
    // TODO(mathp): Support hidpi displays by building an ImageSkia with as many
    // representations as possible?
    image_skia.AddRepresentation(gfx::ImageSkiaRep(bitmap, 1.0f));
    SetIcon(image_skia);
  } else {
    set_display_type(DISPLAY_NONE);
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
