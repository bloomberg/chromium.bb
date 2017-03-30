// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/reading_list/reading_list_distillation_state_util.h"

#include "base/logging.h"

namespace ntp_snippets {

ReadingListEntry::DistillationState ReadingListStateFromSuggestionState(
    ReadingListSuggestionExtra::ReadingListSuggestionDistilledState
        distilled_state) {
  switch (distilled_state) {
    case ReadingListSuggestionExtra::ReadingListSuggestionDistilledState::
        PENDING:
      return ReadingListEntry::WAITING;
    case ReadingListSuggestionExtra::ReadingListSuggestionDistilledState::
        SUCCESS:
      return ReadingListEntry::PROCESSED;
    case ReadingListSuggestionExtra::ReadingListSuggestionDistilledState::
        FAILURE:
      return ReadingListEntry::DISTILLATION_ERROR;
  }
  NOTREACHED();
  return ReadingListEntry::PROCESSING;
}

ReadingListSuggestionExtra::ReadingListSuggestionDistilledState
SuggestionStateFromReadingListState(
    ReadingListEntry::DistillationState distilled_state) {
  switch (distilled_state) {
    case ReadingListEntry::WILL_RETRY:
    case ReadingListEntry::PROCESSING:
    case ReadingListEntry::WAITING:
      return ReadingListSuggestionExtra::ReadingListSuggestionDistilledState::
          PENDING;
    case ReadingListEntry::PROCESSED:
      return ReadingListSuggestionExtra::ReadingListSuggestionDistilledState::
          SUCCESS;
    case ReadingListEntry::DISTILLATION_ERROR:
      return ReadingListSuggestionExtra::ReadingListSuggestionDistilledState::
          FAILURE;
  }
  NOTREACHED();
  return ReadingListSuggestionExtra::ReadingListSuggestionDistilledState::
      PENDING;
}

}  // namespace ntp_snippets
