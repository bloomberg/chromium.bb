// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_handlers/directory_util.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/filename_util.h"
#include "storage/browser/fileapi/file_system_url.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/file_manager/filesystem_api_util.h"
#endif

namespace extensions {
namespace app_file_handler_util {

void EntryIsDirectory(Profile* profile,
                      const base::FilePath& path,
                      const base::Callback<void(bool)>& callback) {
#if defined(OS_CHROMEOS)
  if (file_manager::util::IsUnderNonNativeLocalPath(profile, path)) {
    file_manager::util::IsNonNativeLocalPathDirectory(profile, path, callback);
    return;
  }
#endif

  base::File::Info file_info;
  bool is_directory = GetFileInfo(path, &file_info) && file_info.is_directory;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, is_directory));
}

IsDirectoryCollector::IsDirectoryCollector(Profile* profile)
    : profile_(profile), left_(0), weak_ptr_factory_(this) {}

IsDirectoryCollector::~IsDirectoryCollector() {}

void IsDirectoryCollector::CollectForEntriesPaths(
    const std::vector<base::FilePath>& paths,
    const CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  paths_ = paths;
  callback_ = callback;

  DCHECK(!result_.get());
  result_.reset(new std::set<base::FilePath>());
  left_ = paths.size();

  if (!left_) {
    // Nothing to process.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback_, base::Passed(&result_)));
    callback_ = CompletionCallback();
    return;
  }

  for (size_t i = 0; i < paths.size(); ++i) {
    EntryIsDirectory(profile_, paths[i],
                     base::Bind(&IsDirectoryCollector::OnIsDirectoryCollected,
                                weak_ptr_factory_.GetWeakPtr(), i));
  }
}

void IsDirectoryCollector::OnIsDirectoryCollected(size_t index,
                                                  bool is_directory) {
  if (is_directory)
    result_->insert(paths_[index]);
  if (!--left_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback_, base::Passed(&result_)));
    // Release the callback to avoid a circullar reference in case an instance
    // of this class is a member of a ref counted class, which instance is bound
    // to this callback.
    callback_ = CompletionCallback();
  }
}

}  // namespace app_file_handler_util
}  // namespace extensions
