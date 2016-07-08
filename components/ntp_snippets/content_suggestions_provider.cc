// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestions_provider.h"

#include "base/strings/stringprintf.h"

namespace ntp_snippets {

namespace {

const char kCombinedIDFormat[] = "%d:%s";

}  // namespace

ContentSuggestionsProvider::ContentSuggestionsProvider(
    const std::vector<ContentSuggestionsCategory>& provided_categories)
    : provided_categories_(provided_categories) {}

ContentSuggestionsProvider::~ContentSuggestionsProvider() {}

std::string ContentSuggestionsProvider::MakeUniqueID(
    ContentSuggestionsCategory category,
    const std::string& within_category_id) {
  return base::StringPrintf(kCombinedIDFormat, int(category),
                            within_category_id.c_str());
}

}  // namespace ntp_snippets
