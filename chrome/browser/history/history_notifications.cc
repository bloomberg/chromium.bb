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
      archived(false) {
}

URLsDeletedDetails::~URLsDeletedDetails() {}

KeywordSearchUpdatedDetails::KeywordSearchUpdatedDetails(
    const GURL& url,
    TemplateURLID keyword_id,
    const string16& term)
    : url(url),
      keyword_id(keyword_id),
      term(term) {
}

KeywordSearchUpdatedDetails::~KeywordSearchUpdatedDetails() {}

KeywordSearchDeletedDetails::KeywordSearchDeletedDetails(const GURL& url)
    : url(url) {
}

KeywordSearchDeletedDetails::~KeywordSearchDeletedDetails() {}

}  // namespace history
