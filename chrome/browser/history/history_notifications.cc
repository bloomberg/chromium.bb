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

URLsStarredDetails::URLsStarredDetails(bool being_starred)
    : starred(being_starred) {
}

URLsStarredDetails::~URLsStarredDetails() {}

FaviconChangeDetails::FaviconChangeDetails() {}

FaviconChangeDetails::~FaviconChangeDetails() {}

KeywordSearchTermDetails::KeywordSearchTermDetails() : keyword_id(0) {}

KeywordSearchTermDetails::~KeywordSearchTermDetails() {}

}  // namespace history
