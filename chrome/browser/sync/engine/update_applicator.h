// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An UpdateApplicator is used to iterate over a number of unapplied updates,
// applying them to the client using the given syncer session.
//
// UpdateApplicator might resemble an iterator, but it actually keeps retrying
// failed updates until no remaining updates can be successfully applied.

#ifndef CHROME_BROWSER_SYNC_ENGINE_UPDATE_APPLICATOR_H_
#define CHROME_BROWSER_SYNC_ENGINE_UPDATE_APPLICATOR_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/port.h"
#include "chrome/browser/sync/syncable/syncable.h"

namespace browser_sync {

namespace sessions {
class ConflictProgress;
class UpdateProgress;
}

class ConflictResolver;

class UpdateApplicator {
 public:
  typedef syncable::Directory::UnappliedUpdateMetaHandles::iterator
      UpdateIterator;

  UpdateApplicator(ConflictResolver* resolver,
                   const UpdateIterator& begin,
                   const UpdateIterator& end);

  // returns true if there's more we can do.
  bool AttemptOneApplication(syncable::WriteTransaction* trans);
  // return true if we've applied all updates.
  bool AllUpdatesApplied() const;

  // This class does not automatically save its progress into the
  // SyncSession -- to get that to happen, call this method after update
  // application is finished (i.e., when AttemptOneAllocation stops returning
  // true).
  void SaveProgressIntoSessionState(
      sessions::ConflictProgress* conflict_progress,
      sessions::UpdateProgress* update_progress);

 private:
  // Used to resolve conflicts when trying to apply updates.
  ConflictResolver* const resolver_;

  UpdateIterator const begin_;
  UpdateIterator end_;
  UpdateIterator pointer_;
  bool progress_;

  // Track the result of the various items.
  std::vector<syncable::Id> conflicting_ids_;
  std::vector<syncable::Id> successful_ids_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_UPDATE_APPLICATOR_H_
