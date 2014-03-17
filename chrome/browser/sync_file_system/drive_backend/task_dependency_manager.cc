// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/task_dependency_manager.h"

#include <utility>

#include "base/logging.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

// Erases all items in |item_to_erase| from |container|.
template <typename Container1, typename Container2>
void EraseContainer(const Container1& items_to_erase, Container2* container) {
  for (typename Container1::const_iterator itr = items_to_erase.begin();
       itr != items_to_erase.end(); ++itr) {
    container->erase(*itr);
  }
}

// Inserts all items in |items_to_insert| to |container|, returns true if all
// items are inserted successfully.  Otherwise, returns false and leave
// |container| have the original contents.
template <typename Container1, typename Container2>
bool InsertAllOrNone(const Container1& items_to_insert, Container2* container) {
  typedef typename Container1::const_iterator iterator;
  for (iterator itr = items_to_insert.begin();
       itr != items_to_insert.end(); ++itr) {
    if (!container->insert(*itr).second) {
      // Revert all successful insertion.
      iterator end = itr;
      itr = items_to_insert.begin();
      for (; itr != end; ++itr)
        container->erase(*itr);
      return false;
    }
  }
  return true;
}

bool InsertPaths(std::vector<base::FilePath> paths_to_insert,
                 SubtreeSet* paths) {
  typedef std::vector<base::FilePath>::const_iterator iterator;
  for (iterator itr = paths_to_insert.begin();
       itr != paths_to_insert.end(); ++itr) {
    if (!paths->insert(*itr)) {
      iterator end = itr;
      for (itr = paths_to_insert.begin(); itr != end; ++itr)
        paths->erase(*itr);
      return false;
    }
  }
  return true;
}

}  // namespace

BlockingFactor::BlockingFactor() {}
BlockingFactor::~BlockingFactor() {}

TaskDependencyManager::TaskDependencyManager() {}

TaskDependencyManager::~TaskDependencyManager() {
  DCHECK(paths_by_app_id_.empty());
  DCHECK(file_ids_.empty());
  DCHECK(tracker_ids_.empty());
}

bool TaskDependencyManager::Insert(const BlockingFactor& blocking_factor) {
  if (!InsertAllOrNone(blocking_factor.tracker_ids, &tracker_ids_))
    goto fail_on_tracker_id_insertion;

  if (!InsertAllOrNone(blocking_factor.file_ids, &file_ids_))
    goto fail_on_file_id_insertion;

  if (!InsertPaths(blocking_factor.paths,
                   &paths_by_app_id_[blocking_factor.app_id])) {
    if (paths_by_app_id_[blocking_factor.app_id].empty())
      paths_by_app_id_.erase(blocking_factor.app_id);
    goto fail_on_path_insertion;
  }

  return true;

 fail_on_path_insertion:
  EraseContainer(blocking_factor.file_ids, &file_ids_);
 fail_on_file_id_insertion:
  EraseContainer(blocking_factor.tracker_ids, &tracker_ids_);
 fail_on_tracker_id_insertion:

  return false;
}

void TaskDependencyManager::Erase(const BlockingFactor& blocking_factor) {
  EraseContainer(blocking_factor.paths,
                 &paths_by_app_id_[blocking_factor.app_id]);
  if (paths_by_app_id_[blocking_factor.app_id].empty())
    paths_by_app_id_.erase(blocking_factor.app_id);

  EraseContainer(blocking_factor.file_ids, &file_ids_);
  EraseContainer(blocking_factor.tracker_ids, &tracker_ids_);
}

}  // namespace drive_backend
}  // namespace sync_file_system
