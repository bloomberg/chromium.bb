// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_TASKS_NOTIFIER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_TASKS_NOTIFIER_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace storage {
class FileSystemURL;
}  // namespace storage

namespace ui {
struct SelectedFileInfo;
}

namespace file_manager {
namespace file_tasks {

class FileTasksNotifier : public KeyedService {
 public:
  enum class FileAvailability {
    // File exists and is accessible.
    kOk,

    // The file exists but is currently unavailable; e.g. a Drive file while
    // offline
    kTemporarilyUnavailable,

    // The current state is unknown; e.g. crostini isn't running
    kUnknown,

    // The file is known to be deleted.
    kGone,
  };

  explicit FileTasksNotifier(Profile* profile);
  ~FileTasksNotifier() override;

  static FileTasksNotifier* GetForProfile(Profile* profile);

  void AddObserver(FileTasksObserver*);
  void RemoveObserver(FileTasksObserver*);

  void NotifyFileTasks(const std::vector<storage::FileSystemURL>& file_urls);

  void NotifyFileDialogSelection(const std::vector<ui::SelectedFileInfo>& files,
                                 bool for_open);

 private:
  void NotifyObservers(const std::vector<base::FilePath>& paths,
                       FileTasksObserver::OpenType open_type);

  Profile* const profile_;
  base::ObserverList<FileTasksObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(FileTasksNotifier);
};

}  // namespace file_tasks
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_TASKS_NOTIFIER_H_
