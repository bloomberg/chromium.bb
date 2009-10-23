// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// THIS CLASS PROVIDES NO SYNCHRONIZATION GUARANTEES.

#include "chrome/browser/sync/engine/conflict_resolution_view.h"

#include <map>
#include <set>

#include "chrome/browser/sync/engine/sync_process_state.h"
#include "chrome/browser/sync/engine/syncer_session.h"

using std::map;
using std::set;

namespace browser_sync {

ConflictResolutionView::ConflictResolutionView(SyncerSession* session)
    : process_state_(session->sync_process_state_) {}

int ConflictResolutionView::conflicting_updates() const {
  return process_state_->conflicting_updates();
}

int ConflictResolutionView::conflicting_commits() const {
  return process_state_->conflicting_commits();
}

void ConflictResolutionView::set_conflicting_commits(const int val) {
  process_state_->set_conflicting_commits(val);
}

int ConflictResolutionView::num_sync_cycles() const {
  return process_state_->num_sync_cycles();
}

void ConflictResolutionView::increment_num_sync_cycles() {
  process_state_->increment_num_sync_cycles();
}

void ConflictResolutionView::zero_num_sync_cycles() {
  process_state_->set_num_sync_cycles(0);
}

int64 ConflictResolutionView::current_sync_timestamp() const {
  return process_state_->current_sync_timestamp();
}

int64 ConflictResolutionView::num_server_changes_remaining() const {
  return process_state_->num_server_changes_remaining();
}

// True iff we're stuck. User should contact support.
bool ConflictResolutionView::syncer_stuck() const {
  return process_state_->syncer_stuck();
}

void ConflictResolutionView::set_syncer_stuck(const bool val) {
  process_state_->set_syncer_stuck(val);
}

IdToConflictSetMap::const_iterator ConflictResolutionView::IdToConflictSetFind(
    const syncable::Id& the_id) const {
  return process_state_->IdToConflictSetFind(the_id);
}

IdToConflictSetMap::const_iterator
    ConflictResolutionView::IdToConflictSetBegin() const {
  return process_state_->IdToConflictSetBegin();
}

IdToConflictSetMap::const_iterator
    ConflictResolutionView::IdToConflictSetEnd() const {
  return process_state_->IdToConflictSetEnd();
}

IdToConflictSetMap::size_type
    ConflictResolutionView::IdToConflictSetSize() const {
  return process_state_->IdToConflictSetSize();
}

const ConflictSet*
    ConflictResolutionView::IdToConflictSetGet(const syncable::Id& the_id) {
  return process_state_->IdToConflictSetGet(the_id);
}

set<ConflictSet*>::const_iterator
    ConflictResolutionView::ConflictSetsBegin() const {
  return process_state_->ConflictSetsBegin();
}

set<ConflictSet*>::const_iterator
    ConflictResolutionView::ConflictSetsEnd() const {
  return process_state_->ConflictSetsEnd();
}

set<ConflictSet*>::size_type
    ConflictResolutionView::ConflictSetsSize() const {
  return process_state_->ConflictSetsSize();
}

void ConflictResolutionView::MergeSets(const syncable::Id& set1,
                                       const syncable::Id& set2) {
  process_state_->MergeSets(set1, set2);
}

void ConflictResolutionView::CleanupSets() {
  process_state_->CleanupSets();
}

bool ConflictResolutionView::HasCommitConflicts() const {
  return process_state_->HasConflictingItems();
}

bool ConflictResolutionView::HasBlockedItems() const {
  return process_state_->HasBlockedItems();
}

int ConflictResolutionView::CommitConflictsSize() const {
  return process_state_->ConflictingItemsSize();
}

int ConflictResolutionView::BlockedItemsSize() const {
  return process_state_->BlockedItemsSize();
}

void ConflictResolutionView::AddCommitConflict(const syncable::Id& the_id) {
  process_state_->AddConflictingItem(the_id);
}

void ConflictResolutionView::AddBlockedItem(const syncable::Id& the_id) {
  process_state_->AddBlockedItem(the_id);
}

void ConflictResolutionView::EraseCommitConflict(
    set<syncable::Id>::iterator it) {
  process_state_->EraseConflictingItem(it);
}

void ConflictResolutionView::EraseBlockedItem(
    set<syncable::Id>::iterator it) {
  process_state_->EraseBlockedItem(it);
}

set<syncable::Id>::iterator
ConflictResolutionView::CommitConflictsBegin() const {
  return process_state_->ConflictingItemsBegin();
}

set<syncable::Id>::iterator
ConflictResolutionView::BlockedItemsBegin() const {
  return process_state_->BlockedItemsBegin();
}

set<syncable::Id>::iterator
ConflictResolutionView::CommitConflictsEnd() const {
  return process_state_->ConflictingItemsEnd();
}

set<syncable::Id>::iterator ConflictResolutionView::BlockedItemsEnd() const {
  return process_state_->BlockedItemsEnd();
}

}  // namespace browser_sync
