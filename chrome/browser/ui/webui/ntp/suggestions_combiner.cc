// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/suggestions_combiner.h"

#include <algorithm>

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/ntp/suggestions_page_handler.h"
#include "chrome/browser/ui/webui/ntp/suggestions_source.h"
#include "content/public/browser/web_contents.h"

namespace {

static const size_t kSuggestionsCount = 8;

}  // namespace

SuggestionsCombiner::SuggestionsCombiner(
    SuggestionsCombiner::Delegate* delegate,
    Profile* profile)
    : sources_fetching_count_(0),
      delegate_(delegate),
      suggestions_count_(kSuggestionsCount),
      page_values_(new base::ListValue()),
      debug_enabled_(false),
      profile_(profile) {
}

SuggestionsCombiner::~SuggestionsCombiner() {
}

void SuggestionsCombiner::AddSource(SuggestionsSource* source) {
  source->SetCombiner(this);
  source->SetDebug(debug_enabled_);
  sources_.push_back(source);
}

void SuggestionsCombiner::EnableDebug(bool enable) {
  debug_enabled_ = enable;
  for (size_t i = 0; i < sources_.size(); ++i) {
    sources_[i]->SetDebug(enable);
  }
}

void SuggestionsCombiner::FetchItems(Profile* profile) {
  sources_fetching_count_ = sources_.size();
  for (size_t i = 0; i < sources_.size(); ++i) {
    sources_[i]->FetchItems(profile);
  }
}

base::ListValue* SuggestionsCombiner::GetPageValues() {
  return page_values_.get();
}

void SuggestionsCombiner::OnItemsReady() {
  DCHECK_GT(sources_fetching_count_, 0);
  sources_fetching_count_--;
  if (sources_fetching_count_ == 0) {
    FillPageValues();
    delegate_->OnSuggestionsReady();
  }
}

void SuggestionsCombiner::SetSuggestionsCount(size_t suggestions_count) {
  suggestions_count_ = suggestions_count;
}

void SuggestionsCombiner::FillPageValues() {
  int total_weight = 0;
  for (size_t i = 0; i < sources_.size(); ++i)
    total_weight += sources_[i]->GetWeight();
  DCHECK_GT(total_weight, 0);

  page_values_.reset(new base::ListValue());

  // Evaluate how many items to obtain from each source. We use error diffusion
  // to ensure that we get the total desired number of items.
  int error = 0;

  // Holds the index at which the next item should be added for each source.
  std::vector<size_t> next_item_index_for_source;
  next_item_index_for_source.reserve(sources_.size());
  for (size_t i = 0; i < sources_.size(); ++i) {
    int numerator = sources_[i]->GetWeight() * suggestions_count_ + error;
    error = numerator % total_weight;
    int item_count = std::min(numerator / total_weight,
        sources_[i]->GetItemCount());

    for (int j = 0; j < item_count; ++j)
      page_values_->Append(sources_[i]->PopItem());
    next_item_index_for_source.push_back(page_values_->GetSize());
  }

  // Fill in extra items, prioritizing the first source.
  // Rather than updating |next_item_index_for_source| we keep track of the
  // number of extra items that were added and offset indices by that much.
  size_t extra_items_added = 0;
  for (size_t i = 0; i < sources_.size() &&
      page_values_->GetSize() < suggestions_count_; ++i) {

    size_t index = next_item_index_for_source[i] + extra_items_added;
    while (page_values_->GetSize() < suggestions_count_) {
      base::DictionaryValue* item = sources_[i]->PopItem();
      if (!item)
        break;
      page_values_->Insert(index++, item);
      extra_items_added++;
    }
  }

  // Add page value information common to all sources.
  for (size_t i = 0; i < page_values_->GetSize(); i++) {
    base::DictionaryValue* page_value;
    if (page_values_->GetDictionary(i, &page_value))
      AddExtendedInformation(page_value);
  }
}

void SuggestionsCombiner::AddExtendedInformation(
    base::DictionaryValue* page_value) {
  if (debug_enabled_) {
    std::string url_string;
    if (page_value->GetString("url", &url_string)) {
      GURL url(url_string);
      page_value->SetBoolean("already_open", IsUrlAlreadyOpen(url));
    }
  }
}

bool SuggestionsCombiner::IsUrlAlreadyOpen(const GURL &url) {
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    const Browser* browser = *it;
    if (browser->profile()->IsOffTheRecord() ||
        !browser->profile()->IsSameProfile(profile_))
      continue;

    for (int i = 0; i < browser->tab_strip_model()->count(); i++) {
      const content::WebContents* tab =
          browser->tab_strip_model()->GetWebContentsAt(i);
      if (tab->GetURL() == url)
        return true;
    }
  }
  return false;
}
