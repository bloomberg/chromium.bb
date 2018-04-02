// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_controller.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "ash/app_list/model/search/search_result.h"
#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/search/history.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "ui/app_list/app_list_constants.h"

namespace app_list {

SearchController::SearchController(AppListModelUpdater* model_updater,
                                   History* history)
    : mixer_(new Mixer(model_updater)), history_(history) {}

SearchController::~SearchController() {}

void SearchController::Start(const base::string16& raw_query) {
  last_raw_query_ = raw_query;

  base::string16 query;
  base::TrimWhitespace(raw_query, base::TRIM_ALL, &query);

  dispatching_query_ = true;
  for (const auto& provider : providers_)
    provider->Start(query);

  dispatching_query_ = false;
  query_for_recommendation_ = query.empty();

  OnResultsChanged();
}

void SearchController::OpenResult(SearchResult* result, int event_flags) {
  // This can happen in certain circumstances due to races. See
  // https://crbug.com/534772
  if (!result)
    return;

  result->Open(event_flags);

  if (history_ && history_->IsReady())
    history_->AddLaunchEvent(base::UTF16ToUTF8(last_raw_query_), result->id());
}

void SearchController::InvokeResultAction(SearchResult* result,
                                          int action_index,
                                          int event_flags) {
  // TODO(xiyuan): Hook up with user learning.
  result->InvokeAction(action_index, event_flags);
}

size_t SearchController::AddGroup(size_t max_results,
                                  double multiplier,
                                  double boost) {
  return mixer_->AddGroup(max_results, multiplier, boost);
}

void SearchController::AddProvider(size_t group_id,
                                   std::unique_ptr<SearchProvider> provider) {
  provider->set_result_changed_callback(
      base::Bind(&SearchController::OnResultsChanged, base::Unretained(this)));
  mixer_->AddProviderToGroup(group_id, provider.get());
  providers_.emplace_back(std::move(provider));
}

void SearchController::OnResultsChanged() {
  if (dispatching_query_)
    return;

  KnownResults known_results;
  if (history_ && history_->IsReady()) {
    history_->GetKnownResults(base::UTF16ToUTF8(last_raw_query_))
        ->swap(known_results);
  }

  size_t num_max_results =
      query_for_recommendation_ ? kNumStartPageTiles : kMaxSearchResults;
  mixer_->MixAndPublish(known_results, num_max_results);
}

}  // namespace app_list
