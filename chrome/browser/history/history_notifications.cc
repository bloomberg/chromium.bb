// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_notifications.h"

namespace history {

URLVisitedDetails::URLVisitedDetails()
    : transition(content::PAGE_TRANSITION_LINK) {}

URLVisitedDetails::~URLVisitedDetails() {}

URLsModifiedDetails::URLsModifiedDetails() {}

URLsModifiedDetails::~URLsModifiedDetails() {}

URLsDeletedDetails::URLsDeletedDetails()
    : all_history(false),
      expired(false) {
}

URLsDeletedDetails::~URLsDeletedDetails() {}

KeywordSearchUpdatedDetails::KeywordSearchUpdatedDetails(
    const URLRow& url_row,
    KeywordID keyword_id,
    const base::string16& term)
    : url_row(url_row),
      keyword_id(keyword_id),
      term(term) {
}

KeywordSearchUpdatedDetails::~KeywordSearchUpdatedDetails() {}

KeywordSearchDeletedDetails::KeywordSearchDeletedDetails(URLID url_row_id)
    : url_row_id(url_row_id) {
}

KeywordSearchDeletedDetails::~KeywordSearchDeletedDetails() {}

}  // namespace history
