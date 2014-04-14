// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_TASK_DEPENDENCY_MANAGER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_TASK_DEPENDENCY_MANAGER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync_file_system/subtree_set.h"

namespace sync_file_system {
namespace drive_backend {

struct BlockingFactor {
  bool exclusive;
  std::string app_id;
  std::vector<base::FilePath> paths;
  std::vector<std::string> file_ids;
  std::vector<int64> tracker_ids;

  BlockingFactor();
  ~BlockingFactor();
};

// This class manages dependency of the background tasks.
class TaskDependencyManager {
 public:
  TaskDependencyManager();
  ~TaskDependencyManager();

  // Inserts |blocking_factor| to the collection and returns true if it
  // completes successfully.  Returns false and doesn't modify the collection
  // if |blocking_factor| conflicts other |blocking_factor| that is inserted
  // before.
  // Two |blocking_factor| are handled as conflict if:
  //  - They have common |file_id| or |tracker_id|.
  //  - Or, they have the same |app_id| and have a |path| that one of its parent
  //    belongs to the |blocking_factor|.
  bool Insert(const BlockingFactor* blocking_factor);

  void Erase(const BlockingFactor* blocking_factor);

 private:
  friend class TaskDependencyManagerTest;

  int running_task_count_;
  bool running_exclusive_task_;
  std::map<std::string, SubtreeSet> paths_by_app_id_;
  std::set<std::string> file_ids_;
  std::set<int64> tracker_ids_;

  DISALLOW_COPY_AND_ASSIGN(TaskDependencyManager);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_TASK_DEPENDENCY_MANAGER_H_
