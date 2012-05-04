// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/suggestions_source_top_sites.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/history/visit_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/ntp/suggestions_combiner.h"

namespace {

// The weight used by the combiner to determine which ratio of suggestions
// should be obtained from this source.
const int kSuggestionsTopListWeight = 1;

}  // namespace

SuggestionsSourceTopSites::SuggestionsSourceTopSites() : combiner_(NULL) {
}

SuggestionsSourceTopSites::~SuggestionsSourceTopSites() {
  STLDeleteElements(&items_);
}

inline int SuggestionsSourceTopSites::GetWeight() {
  return kSuggestionsTopListWeight;
}

int SuggestionsSourceTopSites::GetItemCount() {
  return items_.size();
}

DictionaryValue* SuggestionsSourceTopSites::PopItem() {
  if (items_.empty())
    return NULL;

  DictionaryValue* item = items_.front();
  items_.pop_front();
  return item;
}

void SuggestionsSourceTopSites::FetchItems(Profile* profile) {
  DCHECK(combiner_);
  STLDeleteElements(&items_);

  history_consumer_.CancelAllRequests();
  HistoryService* history =
      profile->GetHistoryService(Profile::EXPLICIT_ACCESS);
  // |history| may be null during unit tests.
  if (history) {
    history::VisitFilter time_filter;
    base::TimeDelta half_an_hour =
       base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerHour / 2);
    base::Time now = base::Time::Now();
    time_filter.SetTimeInRangeFilter(now - half_an_hour, now + half_an_hour);
    history->QueryFilteredURLs(0, time_filter, &history_consumer_,
        base::Bind(&SuggestionsSourceTopSites::OnSuggestionsURLsAvailable,
                   base::Unretained(this)));
  }
}

void SuggestionsSourceTopSites::SetCombiner(SuggestionsCombiner* combiner) {
  DCHECK(!combiner_);
  combiner_ = combiner;
}

void SuggestionsSourceTopSites::OnSuggestionsURLsAvailable(
    CancelableRequestProvider::Handle handle,
    const history::FilteredURLList& data) {
  DCHECK(combiner_);
  for (size_t i = 0; i < data.size(); i++) {
    const history::FilteredURL& suggested_url = data[i];
    if (suggested_url.url.is_empty())
      continue;

    DictionaryValue* page_value = new DictionaryValue();
    NewTabUI::SetURLTitleAndDirection(page_value,
                                      suggested_url.title,
                                      suggested_url.url);
    page_value->SetDouble("score", suggested_url.score);
    items_.push_back(page_value);
  }

  combiner_->OnItemsReady();
}
