// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/common/url_database/url_database_types.h"

#include "base/stl_util.h"

namespace history {

// URLRow ----------------------------------------------------------------------

URLRow::URLRow() {
  Initialize();
}

URLRow::URLRow(const GURL& url) : url_(url) {
  // Initialize will not set the URL, so our initialization above will stay.
  Initialize();
}

URLRow::URLRow(const GURL& url, URLID id) : url_(url) {
  // Initialize will not set the URL, so our initialization above will stay.
  Initialize();
  // Initialize will zero the id_, so set it here.
  id_ = id;
}

URLRow::~URLRow() {
}

URLRow& URLRow::operator=(const URLRow& other) {
  id_ = other.id_;
  url_ = other.url_;
  title_ = other.title_;
  visit_count_ = other.visit_count_;
  typed_count_ = other.typed_count_;
  last_visit_ = other.last_visit_;
  hidden_ = other.hidden_;
  return *this;
}

void URLRow::Swap(URLRow* other) {
  std::swap(id_, other->id_);
  url_.Swap(&other->url_);
  title_.swap(other->title_);
  std::swap(visit_count_, other->visit_count_);
  std::swap(typed_count_, other->typed_count_);
  std::swap(last_visit_, other->last_visit_);
  std::swap(hidden_, other->hidden_);
}

void URLRow::Initialize() {
  id_ = 0;
  visit_count_ = 0;
  typed_count_ = 0;
  last_visit_ = base::Time();
  hidden_ = false;
}

// KeywordSearchTermVisit -----------------------------------------------------

KeywordSearchTermVisit::KeywordSearchTermVisit() : visits(0) {}

KeywordSearchTermVisit::~KeywordSearchTermVisit() {}

// KeywordSearchTermRow --------------------------------------------------------

KeywordSearchTermRow::KeywordSearchTermRow() : keyword_id(0), url_id(0) {}

KeywordSearchTermRow::~KeywordSearchTermRow() {}

// Autocomplete thresholds -----------------------------------------------------

const int kLowQualityMatchTypedLimit = 1;
const int kLowQualityMatchVisitLimit = 4;
const int kLowQualityMatchAgeLimitInDays = 3;

base::Time AutocompleteAgeThreshold() {
  return (base::Time::Now() -
          base::TimeDelta::FromDays(kLowQualityMatchAgeLimitInDays));
}

bool RowQualifiesAsSignificant(const URLRow& row,
                               const base::Time& threshold) {
  const base::Time& real_threshold =
      threshold.is_null() ? AutocompleteAgeThreshold() : threshold;
  return (row.typed_count() >= kLowQualityMatchTypedLimit) ||
         (row.visit_count() >= kLowQualityMatchVisitLimit) ||
         (row.last_visit() >= real_threshold);
}

// IconMapping ----------------------------------------------------------------

IconMapping::IconMapping()
    : mapping_id(0),
      icon_id(0),
      icon_type(INVALID_ICON) {
}

IconMapping::~IconMapping() {}

}  // namespace history
