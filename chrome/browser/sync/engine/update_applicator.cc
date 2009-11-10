// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/update_applicator.h"

#include <vector>

#include "base/logging.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/syncable/syncable_id.h"

using std::vector;

namespace browser_sync {

UpdateApplicator::UpdateApplicator(ConflictResolver* resolver,
                                   const UpdateIterator& begin,
                                   const UpdateIterator& end)
    : resolver_(resolver),
      begin_(begin),
      end_(end),
      pointer_(begin),
      progress_(false) {
    size_t item_count = end - begin;
    LOG(INFO) << "UpdateApplicator created for " << item_count << " items.";
    successful_ids_.reserve(item_count);
  }

// Returns true if there's more to do.
bool UpdateApplicator::AttemptOneApplication(
    syncable::WriteTransaction* trans) {
  // If there are no updates left to consider, we're done.
  if (end_ == begin_)
    return false;
  if (pointer_ == end_) {
    if (!progress_)
      return false;

    LOG(INFO) << "UpdateApplicator doing additional pass.";
    pointer_ = begin_;
    progress_ = false;

    // Clear the tracked failures to avoid double-counting.
    conflicting_ids_.clear();
  }
  syncable::MutableEntry entry(trans, syncable::GET_BY_HANDLE, *pointer_);
  UpdateAttemptResponse updateResponse =
      SyncerUtil::AttemptToUpdateEntry(trans, &entry, resolver_);
  switch (updateResponse) {
    case SUCCESS:
      --end_;
      *pointer_ = *end_;
      progress_ = true;
      successful_ids_.push_back(entry.Get(syncable::ID));
      break;
    case CONFLICT:
      pointer_++;
      conflicting_ids_.push_back(entry.Get(syncable::ID));
      break;
    default:
      NOTREACHED();
      break;
  }
  LOG(INFO) << "Apply Status for " << entry.Get(syncable::META_HANDLE)
            << " is " << updateResponse;

  return true;
}

bool UpdateApplicator::AllUpdatesApplied() const {
  return conflicting_ids_.empty() && begin_ == end_;
}

void UpdateApplicator::SaveProgressIntoSessionState(SyncerSession* session) {
  DCHECK(begin_ == end_ || ((pointer_ == end_) && !progress_))
      << "SaveProgress called before updates exhausted.";

  vector<syncable::Id>::const_iterator i;
  for (i = conflicting_ids_.begin(); i != conflicting_ids_.end(); ++i) {
    session->AddCommitConflict(*i);
    session->AddAppliedUpdate(CONFLICT, *i);
  }
  for (i = successful_ids_.begin(); i != successful_ids_.end(); ++i) {
    session->EraseCommitConflict(*i);
    session->AddAppliedUpdate(SUCCESS, *i);
  }
}

}  // namespace browser_sync
