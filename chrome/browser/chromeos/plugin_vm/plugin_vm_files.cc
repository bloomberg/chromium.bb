// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_files.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

void DirExistsResult(bool result, base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(result);
}

void EnsureDirExistsOnIOThread(base::FilePath dir,
                               base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::File::Error error = base::File::FILE_OK;
  bool result = base::CreateDirectoryAndGetError(dir, &error);
  if (!result) {
    LOG(ERROR) << "Failed to create PluginVm shared dir " << dir.value() << ": "
               << base::File::ErrorToString(error);
  }
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&DirExistsResult, result, std::move(callback)));
}

}  // namespace

namespace plugin_vm {

void EnsureDefaultSharedDirExists(Profile* profile,
                                  base::OnceCallback<void(bool)> callback) {
  base::FilePath dir =
      file_manager::util::GetMyFilesFolderForProfile(profile).Append(
          kPluginVmName);
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&EnsureDirExistsOnIOThread, dir, std::move(callback)));
}

}  // namespace plugin_vm
