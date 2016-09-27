// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestions_provider.h"

#include "components/ntp_snippets/category_factory.h"

namespace ntp_snippets {

ContentSuggestionsProvider::ContentSuggestionsProvider(
    Observer* observer,
    CategoryFactory* category_factory)
    : observer_(observer), category_factory_(category_factory) {}

ContentSuggestionsProvider::~ContentSuggestionsProvider() = default;

std::string ContentSuggestionsProvider::MakeUniqueID(
    Category category,
    const std::string& within_category_id) const {
  return category_factory()->MakeUniqueID(category, within_category_id);
}

Category ContentSuggestionsProvider::GetCategoryFromUniqueID(
    const std::string& unique_id) const {
  return category_factory()->GetCategoryFromUniqueID(unique_id);
}

std::string ContentSuggestionsProvider::GetWithinCategoryIDFromUniqueID(
    const std::string& unique_id) const {
  return category_factory()->GetWithinCategoryIDFromUniqueID(unique_id);
}

}  // namespace ntp_snippets
