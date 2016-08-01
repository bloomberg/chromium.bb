// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestions_category.h"

#include "base/logging.h"

namespace ntp_snippets {

ContentSuggestionsCategory::ContentSuggestionsCategory(int id) : id_(id) {}

bool ContentSuggestionsCategory::IsKnownCategory(
    KnownSuggestionsCategories known_category) const {
  return id_ == static_cast<int>(known_category);
}

bool operator==(const ContentSuggestionsCategory& left,
                const ContentSuggestionsCategory& right) {
  return left.id() == right.id();
}

bool operator!=(const ContentSuggestionsCategory& left,
                const ContentSuggestionsCategory& right) {
  return !(left == right);
}

std::ostream& operator<<(std::ostream& os,
                         const ContentSuggestionsCategory& obj) {
  os << obj.id();
  return os;
}

}  // namespace ntp_snippets
