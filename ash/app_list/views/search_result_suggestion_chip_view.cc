// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_suggestion_chip_view.h"

#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"

namespace app_list {

namespace {

// Records an app being launched.
void LogAppLaunch() {
  UMA_HISTOGRAM_BOOLEAN(kAppListAppLaunchedFullscreen,
                        true /* suggested app */);
  base::RecordAction(base::UserMetricsAction("AppList_OpenSuggestedApp"));
}

}  // namespace

SearchResultSuggestionChipView::SearchResultSuggestionChipView(
    AppListViewDelegate* view_delegate)
    : view_delegate_(view_delegate), weak_ptr_factory_(this) {}

SearchResultSuggestionChipView::~SearchResultSuggestionChipView() {
  DiscardItem();
}

void SearchResultSuggestionChipView::SetSearchResult(SearchResult* item) {
  // Replace old item with new item.
  DiscardItem();

  if (!item)
    return;
  item_ = item;

  item_->AddObserver(this);

  app_list::SuggestionChipView::Params params;
  params.text = item->title();
  params.icon = item->icon();
  suggestion_chip_view_ = new SuggestionChipView(params, /* listener */ this);
  AddChildView(suggestion_chip_view_);
}

void SearchResultSuggestionChipView::OnResultDestroying() {
  SetSearchResult(nullptr);
}

void SearchResultSuggestionChipView::ButtonPressed(views::Button* sender,
                                                   const ui::Event& event) {
  DCHECK(item_);
  LogAppLaunch();
  RecordSearchResultOpenSource(item_, view_delegate_->GetModel(),
                               view_delegate_->GetSearchModel());
  view_delegate_->OpenSearchResult(item_->id(), event.flags());
}

void SearchResultSuggestionChipView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty() || !item_)
    return;

  suggestion_chip_view_->SetBoundsRect(rect);
}

const char* SearchResultSuggestionChipView::GetClassName() const {
  return "SearchResultSuggestionChipView";
}

gfx::Size SearchResultSuggestionChipView::CalculatePreferredSize() const {
  if (!suggestion_chip_view_)
    return gfx::Size();

  return suggestion_chip_view_->GetPreferredSize();
}

void SearchResultSuggestionChipView::DiscardItem() {
  delete suggestion_chip_view_;
  suggestion_chip_view_ = nullptr;

  if (item_)
    item_->RemoveObserver(this);

  item_ = nullptr;
}

}  // namespace app_list
