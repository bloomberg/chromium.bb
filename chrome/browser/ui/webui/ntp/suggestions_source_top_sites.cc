// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/suggestions_source_top_sites.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/history/visit_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/ntp/suggestions_combiner.h"
#include "chrome/common/chrome_switches.h"


namespace {

// The weight used by the combiner to determine which ratio of suggestions
// should be obtained from this source.
const int kSuggestionsTopListWeight = 1;

}  // namespace

SuggestionsSourceTopSites::SuggestionsSourceTopSites()
    : combiner_(NULL),
      debug_(false) {
}

SuggestionsSourceTopSites::~SuggestionsSourceTopSites() {
  STLDeleteElements(&items_);
}

void SuggestionsSourceTopSites::SetDebug(bool enable) {
  debug_ = enable;
}

inline int SuggestionsSourceTopSites::GetWeight() {
  return kSuggestionsTopListWeight;
}

int SuggestionsSourceTopSites::GetItemCount() {
  return items_.size();
}

base::DictionaryValue* SuggestionsSourceTopSites::PopItem() {
  if (items_.empty())
    return NULL;

  base::DictionaryValue* item = items_.front();
  items_.pop_front();
  return item;
}

void SuggestionsSourceTopSites::FetchItems(Profile* profile) {
  DCHECK(combiner_);
  STLDeleteElements(&items_);

  history_tracker_.TryCancelAll();
  HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile, Profile::EXPLICIT_ACCESS);
  // |history| may be null during unit tests.
  if (history) {
    history::VisitFilter time_filter;
    time_filter.SetFilterTime(base::Time::Now());
    time_filter.SetFilterWidth(GetFilterWidth());
    time_filter.set_sorting_order(GetSortingOrder());

    history->QueryFilteredURLs(
        0,
        time_filter,
        debug_,
        base::Bind(&SuggestionsSourceTopSites::OnSuggestionsUrlsAvailable,
                   base::Unretained(this)),
        &history_tracker_);
  }
}

void SuggestionsSourceTopSites::SetCombiner(SuggestionsCombiner* combiner) {
  DCHECK(!combiner_);
  combiner_ = combiner;
}

void SuggestionsSourceTopSites::OnSuggestionsUrlsAvailable(
    const history::FilteredURLList* data) {
  DCHECK(data);
  DCHECK(combiner_);
  for (size_t i = 0; i < data->size(); i++) {
    const history::FilteredURL& suggested_url = (*data)[i];
    if (suggested_url.url.is_empty())
      continue;

    base::DictionaryValue* page_value = new base::DictionaryValue();
    NewTabUI::SetUrlTitleAndDirection(page_value,
                                      suggested_url.title,
                                      suggested_url.url);
    page_value->SetDouble("score", suggested_url.score);
    if (debug_) {
      if (suggested_url.extended_info.total_visits) {
        page_value->SetInteger("extended_info.total visits",
                               suggested_url.extended_info.total_visits);
      }
      if (suggested_url.extended_info.visits) {
        page_value->SetInteger("extended_info.visits",
                               suggested_url.extended_info.visits);
      }
      if (suggested_url.extended_info.duration_opened) {
        page_value->SetInteger("extended_info.duration opened",
                               suggested_url.extended_info.duration_opened);
      }
      if (!suggested_url.extended_info.last_visit_time.is_null()) {
        base::TimeDelta deltaTime =
            base::Time::Now() - suggested_url.extended_info.last_visit_time;
        page_value->SetInteger("extended_info.seconds since last visit",
                               deltaTime.InSeconds());
      }
    }
    items_.push_back(page_value);
  }

  combiner_->OnItemsReady();
}

// static
base::TimeDelta SuggestionsSourceTopSites::GetFilterWidth() {
  return base::TimeDelta::FromHours(1);
}

// static
history::VisitFilter::SortingOrder
SuggestionsSourceTopSites::GetSortingOrder() {
  return history::VisitFilter::ORDER_BY_RECENCY;
}
