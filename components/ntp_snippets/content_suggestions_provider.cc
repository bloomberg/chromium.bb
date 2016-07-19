// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestions_provider.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace ntp_snippets {

namespace {

const char kCombinedIDFormat[] = "%d|%s";
const char kSeparator = '|';

}  // namespace

ContentSuggestionsProvider::ContentSuggestionsProvider(
    const std::vector<ContentSuggestionsCategory>& provided_categories)
    : provided_categories_(provided_categories) {}

ContentSuggestionsProvider::~ContentSuggestionsProvider() {}

// static
std::string ContentSuggestionsProvider::MakeUniqueID(
    ContentSuggestionsCategory category,
    const std::string& within_category_id) {
  return base::StringPrintf(kCombinedIDFormat,
                            static_cast<int>(category),
                            within_category_id.c_str());
}

// static
ContentSuggestionsCategory ContentSuggestionsProvider::GetCategoryFromUniqueID(
    const std::string& unique_id) {
  size_t colon_index = unique_id.find(kSeparator);
  DCHECK_NE(std::string::npos, colon_index) << "Not a valid unique_id: "
                                            << unique_id;
  int category = -1;
  DCHECK(base::StringToInt(unique_id.substr(0, colon_index), &category))
      << "Non-numeric category part in unique_id: " << unique_id;
  DCHECK(0 <= category && category < int(ContentSuggestionsCategory::COUNT))
      << "Category does not exist: " << category;
  return ContentSuggestionsCategory(category);
}

// static
std::string ContentSuggestionsProvider::GetWithinCategoryIDFromUniqueID(
    const std::string& unique_id) {
  size_t colon_index = unique_id.find(kSeparator);
  DCHECK_NE(std::string::npos, colon_index) << "Not a valid unique_id: "
                                            << unique_id;
  return unique_id.substr(colon_index + 1);
}

}  // namespace ntp_snippets
