// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
      routing_info_(routes) {
    size_t item_count = end - begin;
    VLOG(1) << "UpdateApplicator created for " << item_count << " items.";
    successful_ids_.reserve(item_count);
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

    VLOG(1) << "UpdateApplicator doing additional pass.";
    pointer_ = begin_;
    progress_ = false;

    // Clear the tracked failures to avoid double-counting.
    conflicting_ids_.clear();
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
  VLOG(1) << "Apply Status for " << entry.Get(syncable::META_HANDLE)
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
    VLOG(1) << "Skipping update application, type not permitted.";
    return true;
  }
  return false;
}

bool UpdateApplicator::AllUpdatesApplied() const {
  return conflicting_ids_.empty() && begin_ == end_;
}

void UpdateApplicator::SaveProgressIntoSessionState(
    sessions::ConflictProgress* conflict_progress,
    sessions::UpdateProgress* update_progress) {
  DCHECK(begin_ == end_ || ((pointer_ == end_) && !progress_))
      << "SaveProgress called before updates exhausted.";

  vector<syncable::Id>::const_iterator i;
  for (i = conflicting_ids_.begin(); i != conflicting_ids_.end(); ++i) {
    conflict_progress->AddConflictingItemById(*i);
    update_progress->AddAppliedUpdate(CONFLICT, *i);
  }
  for (i = successful_ids_.begin(); i != successful_ids_.end(); ++i) {
    conflict_progress->EraseConflictingItemById(*i);
    update_progress->AddAppliedUpdate(SUCCESS, *i);
  }
}

}  // namespace browser_sync
