// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/update_applicator.h"

#include <vector>

#include "base/logging.h"
#include "chrome/browser/sync/engine/syncer_util.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/syncable/syncable_id.h"

using std::vector;

namespace browser_sync {

UpdateApplicator::UpdateApplicator(ConflictResolver* resolver,
                                   Cryptographer* cryptographer,
                                   const UpdateIterator& begin,
                                   const UpdateIterator& end,
                                   const ModelSafeRoutingInfo& routes,
                                   ModelSafeGroup group_filter)
    : resolver_(resolver),
      cryptographer_(cryptographer),
      begin_(begin),
      end_(end),
      pointer_(begin),
      group_filter_(group_filter),
      progress_(false),
      routing_info_(routes),
      application_results_(end - begin) {
  size_t item_count = end - begin;
  DVLOG(1) << "UpdateApplicator created for " << item_count << " items.";
}

UpdateApplicator::~UpdateApplicator() {
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

    DVLOG(1) << "UpdateApplicator doing additional pass.";
    pointer_ = begin_;
    progress_ = false;

    // Clear the tracked failures to avoid double-counting.
    application_results_.ClearConflicts();
  }

  syncable::Entry read_only(trans, syncable::GET_BY_HANDLE, *pointer_);
  if (SkipUpdate(read_only)) {
    Advance();
    return true;
  }

  syncable::MutableEntry entry(trans, syncable::GET_BY_HANDLE, *pointer_);
  UpdateAttemptResponse updateResponse = SyncerUtil::AttemptToUpdateEntry(
      trans, &entry, resolver_, cryptographer_);
  switch (updateResponse) {
    case SUCCESS:
      Advance();
      progress_ = true;
      application_results_.AddSuccess(entry.Get(syncable::ID));
      break;
    case CONFLICT:
      pointer_++;
      application_results_.AddConflict(entry.Get(syncable::ID));
      break;
    case CONFLICT_ENCRYPTION:
      pointer_++;
      application_results_.AddEncryptionConflict(entry.Get(syncable::ID));
      break;
    default:
      NOTREACHED();
      break;
  }
  DVLOG(1) << "Apply Status for " << entry.Get(syncable::META_HANDLE)
           << " is " << updateResponse;

  return true;
}

void UpdateApplicator::Advance() {
  --end_;
  *pointer_ = *end_;
}

bool UpdateApplicator::SkipUpdate(const syncable::Entry& entry) {
  syncable::ModelType type = entry.GetServerModelType();
  ModelSafeGroup g = GetGroupForModelType(type, routing_info_);
  // The extra routing_info count check here is to support GetUpdateses for
  // a subset of the globally enabled types, and not attempt to update items
  // if their type isn't permitted in the current run.  These would typically
  // be unapplied items from a previous sync.
  if (g != group_filter_)
    return true;
  if (g == GROUP_PASSIVE &&
      !routing_info_.count(type) &&
      type != syncable::UNSPECIFIED &&
      type != syncable::TOP_LEVEL_FOLDER) {
    DVLOG(1) << "Skipping update application, type not permitted.";
    return true;
  }
  return false;
}

bool UpdateApplicator::AllUpdatesApplied() const {
  return application_results_.no_conflicts() && begin_ == end_;
}

void UpdateApplicator::SaveProgressIntoSessionState(
    sessions::ConflictProgress* conflict_progress,
    sessions::UpdateProgress* update_progress) {
  DCHECK(begin_ == end_ || ((pointer_ == end_) && !progress_))
      << "SaveProgress called before updates exhausted.";

  application_results_.SaveProgress(conflict_progress, update_progress);
}

UpdateApplicator::ResultTracker::ResultTracker(size_t num_results) {
  successful_ids_.reserve(num_results);
}

UpdateApplicator::ResultTracker::~ResultTracker() {
}

void UpdateApplicator::ResultTracker::AddConflict(syncable::Id id) {
  conflicting_ids_.push_back(id);
}

void UpdateApplicator::ResultTracker::AddEncryptionConflict(syncable::Id id) {
  encryption_conflict_ids_.push_back(id);
}

void UpdateApplicator::ResultTracker::AddSuccess(syncable::Id id) {
  successful_ids_.push_back(id);
}

void UpdateApplicator::ResultTracker::SaveProgress(
    sessions::ConflictProgress* conflict_progress,
    sessions::UpdateProgress* update_progress) {
  vector<syncable::Id>::const_iterator i;
  for (i = conflicting_ids_.begin(); i != conflicting_ids_.end(); ++i) {
    conflict_progress->AddConflictingItemById(*i);
    update_progress->AddAppliedUpdate(CONFLICT, *i);
  }
  for (i = encryption_conflict_ids_.begin();
       i != encryption_conflict_ids_.end(); ++i) {
    // Encryption conflicts should not put the syncer into a stuck state. We
    // mark as conflict, so that we reattempt to apply updates, but add it to
    // the list of nonblocking conflicts instead of normal conflicts.
    conflict_progress->AddNonblockingConflictingItemById(*i);
    update_progress->AddAppliedUpdate(CONFLICT, *i);
  }
  for (i = successful_ids_.begin(); i != successful_ids_.end(); ++i) {
    conflict_progress->EraseConflictingItemById(*i);
    update_progress->AddAppliedUpdate(SUCCESS, *i);
  }
}

void UpdateApplicator::ResultTracker::ClearConflicts() {
  conflicting_ids_.clear();
  encryption_conflict_ids_.clear();
}

bool UpdateApplicator::ResultTracker::no_conflicts() const {
  return conflicting_ids_.empty();
}

}  // namespace browser_sync
