// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"

#include "base/callback.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier_factory.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/common/fileapi/file_system_types.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace file_manager {
namespace file_tasks {
namespace {

bool IsSupportedFileSystemType(storage::FileSystemType type) {
  switch (type) {
    case storage::kFileSystemTypeNativeLocal:
    case storage::kFileSystemTypeRestrictedNativeLocal:
    case storage::kFileSystemTypeDriveFs:
      return true;
    default:
      return false;
  }
}

}  // namespace

FileTasksNotifier::FileTasksNotifier(Profile* profile) : profile_(profile) {}

FileTasksNotifier::~FileTasksNotifier() = default;

// static
FileTasksNotifier* FileTasksNotifier::GetForProfile(Profile* profile) {
  return FileTasksNotifierFactory::GetInstance()->GetForProfile(profile);
}

void FileTasksNotifier::AddObserver(FileTasksObserver* observer) {
  observers_.AddObserver(observer);
}

void FileTasksNotifier::RemoveObserver(FileTasksObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FileTasksNotifier::NotifyFileTasks(
    const std::vector<storage::FileSystemURL>& file_urls) {
  std::vector<base::FilePath> paths;
  for (const auto& url : file_urls) {
    if (IsSupportedFileSystemType(url.type())) {
      paths.push_back(url.path());
    }
  }
  NotifyObservers(paths, FileTasksObserver::OpenType::kLaunch);
}

void FileTasksNotifier::NotifyFileDialogSelection(
    const std::vector<ui::SelectedFileInfo>& files,
    bool for_open) {
  std::vector<base::FilePath> paths;
  for (const auto& file : files) {
    paths.push_back(file.file_path);
  }
  NotifyObservers(paths, for_open ? FileTasksObserver::OpenType::kOpen
                                  : FileTasksObserver::OpenType::kSaveAs);
}

void FileTasksNotifier::NotifyObservers(
    const std::vector<base::FilePath>& paths,
    FileTasksObserver::OpenType open_type) {
  std::vector<FileTasksObserver::FileOpenEvent> opens;
  for (const auto& path : paths) {
    if (profile_->GetPath().IsParent(path) ||
        base::FilePath("/run/arc/sdcard/write/emulated/0").IsParent(path) ||
        base::FilePath("/media/fuse").IsParent(path)) {
      opens.push_back({path, open_type});
    }
  }
  if (opens.empty()) {
    return;
  }
  for (auto& observer : observers_) {
    observer.OnFilesOpened(opens);
  }
}

}  // namespace file_tasks
}  // namespace file_manager
