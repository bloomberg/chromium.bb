// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_WATCHER_STABILITY_PATHS_H_
#define COMPONENTS_BROWSER_WATCHER_STABILITY_PATHS_H_

#include "base/files/file_path.h"
#include "base/process/process.h"

namespace browser_watcher {

#if defined(OS_WIN)

// Returns the the stability debugging directory.
base::FilePath GetStabilityDir(const base::FilePath& user_data_dir);

// On success, |path| contains the path to the stability debugging information
// file for |process|.
bool GetStabilityFileForProcess(const base::Process& process,
                                const base::FilePath& user_data_dir,
                                base::FilePath* path);

// Returns a pattern that matches file names returned by GetFileForProcess.
base::FilePath::StringType GetStabilityFilePattern();

// Marks the stability file for deletion.
void MarkStabilityFileForDeletion(const base::FilePath& user_data_dir);

#endif  // defined(OS_WIN)

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_STABILITY_PATHS_H_
