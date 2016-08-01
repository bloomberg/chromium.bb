// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestions_category_factory.h"

#include <algorithm>

#include "base/logging.h"

namespace ntp_snippets {

ContentSuggestionsCategoryFactory::ContentSuggestionsCategoryFactory() {
  // Add all local categories in a fixed order.
  AddKnownCategory(KnownSuggestionsCategories::OFFLINE_PAGES);
}

ContentSuggestionsCategoryFactory::~ContentSuggestionsCategoryFactory() {}

ContentSuggestionsCategory ContentSuggestionsCategoryFactory::FromKnownCategory(
    KnownSuggestionsCategories known_category) {
  if (known_category < KnownSuggestionsCategories::LOCAL_CATEGORIES_COUNT) {
    // Local categories should have been added already.
    DCHECK(CategoryExists(static_cast<int>(known_category)));
  } else {
    DCHECK_GT(known_category,
              KnownSuggestionsCategories::REMOTE_CATEGORIES_OFFSET);
  }
  return InternalFromID(static_cast<int>(known_category));
}

ContentSuggestionsCategory
ContentSuggestionsCategoryFactory::FromRemoteCategory(int remote_category) {
  DCHECK_GT(remote_category, 0);
  return InternalFromID(
      static_cast<int>(KnownSuggestionsCategories::REMOTE_CATEGORIES_OFFSET) +
      remote_category);
}

ContentSuggestionsCategory ContentSuggestionsCategoryFactory::FromIDValue(
    int id) {
  DCHECK_GE(id, 0);
  DCHECK(id < static_cast<int>(
                  KnownSuggestionsCategories::LOCAL_CATEGORIES_COUNT) ||
         id > static_cast<int>(
                  KnownSuggestionsCategories::REMOTE_CATEGORIES_OFFSET));
  return InternalFromID(id);
}

bool ContentSuggestionsCategoryFactory::CompareCategories(
    const ContentSuggestionsCategory& left,
    const ContentSuggestionsCategory& right) const {
  if (left == right)
    return false;
  return std::find(ordered_categories_.begin(), ordered_categories_.end(),
                   left) < std::find(ordered_categories_.begin(),
                                     ordered_categories_.end(), right);
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

bool ContentSuggestionsCategoryFactory::CategoryExists(int id) {
  return std::find(ordered_categories_.begin(), ordered_categories_.end(),
                   ContentSuggestionsCategory(id)) != ordered_categories_.end();
}

void ContentSuggestionsCategoryFactory::AddKnownCategory(
    KnownSuggestionsCategories known_category) {
  InternalFromID(static_cast<int>(known_category));
}

ContentSuggestionsCategory ContentSuggestionsCategoryFactory::InternalFromID(
    int id) {
  auto it = std::find(ordered_categories_.begin(), ordered_categories_.end(),
                      ContentSuggestionsCategory(id));
  if (it != ordered_categories_.end())
    return *it;

  ContentSuggestionsCategory category(id);
  ordered_categories_.push_back(category);
  return category;
}

}  // namespace ntp_snippets
