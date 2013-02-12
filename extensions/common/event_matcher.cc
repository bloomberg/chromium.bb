// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/event_matcher.h"

#include "extensions/common/event_filtering_info.h"

namespace {
const char kUrlFiltersKey[] = "url";
}

namespace extensions {

EventMatcher::EventMatcher(scoped_ptr<base::DictionaryValue> filter)
    : filter_(filter.Pass()) {
}

EventMatcher::~EventMatcher() {
}

bool EventMatcher::MatchNonURLCriteria(
    const EventFilteringInfo& event_info) const {
  // There is currently no criteria apart from URL criteria.
  return true;
}

int EventMatcher::GetURLFilterCount() const {
  base::ListValue* url_filters = NULL;
  if (filter_->GetList(kUrlFiltersKey, &url_filters))
    return url_filters->GetSize();
  return 0;
}

bool EventMatcher::GetURLFilter(int i, base::DictionaryValue** url_filter_out) {
  base::ListValue* url_filters = NULL;
  if (filter_->GetList(kUrlFiltersKey, &url_filters)) {
    return url_filters->GetDictionary(i, url_filter_out);
  }
  return false;
}

int EventMatcher::HasURLFilters() const {
  return GetURLFilterCount() != 0;
}

}  // namespace extensions
