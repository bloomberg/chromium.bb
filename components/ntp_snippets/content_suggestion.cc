// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestion.h"

namespace ntp_snippets {

bool ContentSuggestion::ID::operator==(const ID& rhs) const {
  return category_ == rhs.category_ &&
         id_within_category_ == rhs.id_within_category_;
}

bool ContentSuggestion::ID::operator!=(const ID& rhs) const {
  return !(*this == rhs);
}

ContentSuggestion::ContentSuggestion(const ID& id, const GURL& url)
    : id_(id), url_(url), score_(0) {}

ContentSuggestion::ContentSuggestion(Category category,
                                     const std::string& id_within_category,
                                     const GURL& url)
    : id_(category, id_within_category), url_(url), score_(0) {}

ContentSuggestion::ContentSuggestion(ContentSuggestion&&) = default;

ContentSuggestion& ContentSuggestion::operator=(ContentSuggestion&&) = default;

ContentSuggestion::~ContentSuggestion() = default;

std::ostream& operator<<(std::ostream& os, const ContentSuggestion::ID& id) {
  os << id.category() << "|" << id.id_within_category();
  return os;
}

}  // namespace ntp_snippets
