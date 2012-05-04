// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/suggestions_combiner.h"

#include <algorithm>

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp/suggestions_page_handler.h"
#include "chrome/browser/ui/webui/ntp/suggestions_source.h"

namespace {

static const size_t kSuggestionsCount = 8;

}  // namespace

SuggestionsCombiner::SuggestionsCombiner(
    SuggestionsHandler* suggestions_handler)
    : sources_fetching_count_(0),
      suggestions_handler_(suggestions_handler),
      suggestions_count_(kSuggestionsCount),
      pages_value_(new base::ListValue()) {
}

SuggestionsCombiner::~SuggestionsCombiner() {
}

base::ListValue* SuggestionsCombiner::GetPagesValue() {
  return pages_value_.get();
}

void SuggestionsCombiner::FetchItems(Profile* profile) {
  sources_fetching_count_ = sources_.size();
  for (size_t i = 0; i < sources_.size(); ++i) {
    sources_[i]->FetchItems(profile);
  }
}

void SuggestionsCombiner::AddSource(SuggestionsSource* source) {
  source->SetCombiner(this);
  sources_.push_back(source);
}

void SuggestionsCombiner::FillPagesValue() {
  int total_weight = 0;
  for (size_t i = 0; i < sources_.size(); ++i)
    total_weight += sources_[i]->GetWeight();
  DCHECK_GT(total_weight, 0);

  pages_value_.reset(new base::ListValue());

  // Evaluate how many items to obtain from each sources. We use error diffusion
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
      pages_value_->Append(sources_[i]->PopItem());
    next_item_index_for_source.push_back(pages_value_->GetSize());
  }

  // Fill in extra items, prioritizing the first source.
  DictionaryValue* item;
  // Rather than updating |next_item_index_for_source| we keep track of the
  // number of extra items that were added and offset indices by that much.
  size_t extra_items_added = 0;
  for (size_t i = 0; i < sources_.size() &&
      pages_value_->GetSize() < suggestions_count_; ++i) {

    size_t index = next_item_index_for_source[i] + extra_items_added;
    while (pages_value_->GetSize() < suggestions_count_ &&
        (item = sources_[i]->PopItem())) {
      pages_value_->Insert(index++, item);
      extra_items_added++;
    }
  }
}

void SuggestionsCombiner::OnItemsReady() {
  DCHECK_GT(sources_fetching_count_, 0);
  sources_fetching_count_--;
  if (sources_fetching_count_ == 0) {
    FillPagesValue();
    suggestions_handler_->OnPagesValueReady();
  }
}
