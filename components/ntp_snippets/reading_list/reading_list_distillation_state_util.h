// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_READING_LIST_READING_LIST_DISTILLATION_STATE_UTIL_H_
#define COMPONENTS_NTP_SNIPPETS_READING_LIST_READING_LIST_DISTILLATION_STATE_UTIL_H_

#include "components/ntp_snippets/content_suggestion.h"
#include "components/reading_list/core/reading_list_entry.h"

namespace ntp_snippets {

ReadingListEntry::DistillationState ReadingListStateFromSuggestionState(
    ReadingListSuggestionExtra::ReadingListSuggestionDistilledState
        distilled_state);

ReadingListSuggestionExtra::ReadingListSuggestionDistilledState
SuggestionStateFromReadingListState(
    ReadingListEntry::DistillationState distilled_state);

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_READING_LIST_READING_LIST_DISTILLATION_STATE_UTIL_H_
