// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/suggestions_source_top_sites.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "base/string_number_conversions.h"
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
    time_filter.SetFilterTime(base::Time::Now());
    time_filter.SetFilterWidth(GetFilterWidth());
    time_filter.set_sorting_order(GetSortingOrder());

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

// static
base::TimeDelta SuggestionsSourceTopSites::GetFilterWidth() {
  const CommandLine* cli = CommandLine::ForCurrentProcess();
  const std::string filter_width_switch =
      cli->GetSwitchValueASCII(switches::kSuggestionNtpFilterWidth);
  unsigned int filter_width;
  if (base::StringToUint(filter_width_switch, &filter_width))
    return base::TimeDelta::FromMinutes(filter_width);
  return base::TimeDelta::FromHours(1);
}

// static
history::VisitFilter::SortingOrder
SuggestionsSourceTopSites::GetSortingOrder() {
  const CommandLine* cli = CommandLine::ForCurrentProcess();
  if (cli->HasSwitch(switches::kSuggestionNtpGaussianFilter))
    return history::VisitFilter::ORDER_BY_TIME_GAUSSIAN;
  if (cli->HasSwitch(switches::kSuggestionNtpLinearFilter))
    return history::VisitFilter::ORDER_BY_TIME_LINEAR;
  return history::VisitFilter::ORDER_BY_RECENCY;
}
