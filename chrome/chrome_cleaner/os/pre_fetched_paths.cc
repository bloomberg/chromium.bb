// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"

#include <shlobj.h>

#include "base/base_paths_win.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"

namespace chrome_cleaner {

// static
PreFetchedPaths* PreFetchedPaths::GetInstance() {
  return base::Singleton<PreFetchedPaths>::get();
}

PreFetchedPaths::~PreFetchedPaths() = default;

// Pre-fetches all paths and returns true if all fetching operations were
// successful.
bool PreFetchedPaths::Initialize() {
  DCHECK(!initialized_);

  initialized_ = FetchPath(base::FILE_EXE) &&
                 FetchPath(base::DIR_PROGRAM_FILES) &&
                 FetchPath(base::DIR_WINDOWS);
  return initialized_;
}

void PreFetchedPaths::DisableForTesting() {
  cache_disabled_ = true;
}

base::FilePath PreFetchedPaths::GetExecutablePath() const {
  return Get(base::FILE_EXE);
}

base::FilePath PreFetchedPaths::GetProgramFilesFolder() const {
  return Get(base::DIR_PROGRAM_FILES);
}

base::FilePath PreFetchedPaths::GetWindowsFolder() const {
  return Get(base::DIR_WINDOWS);
}

PreFetchedPaths::PreFetchedPaths() = default;

bool PreFetchedPaths::FetchPath(int key) {
  base::FilePath path;
  if (base::PathService::Get(key, &path) && !path.empty()) {
    paths_[key] = NormalizePath(path);
    return true;
  }

  LOG(ERROR) << "Cannot retrieve file path for key " << key;
  return false;
}

base::FilePath PreFetchedPaths::Get(int key) const {
  if (!cache_disabled_) {
    CHECK(initialized_);
    DCHECK(paths_.count(key));
    return paths_.at(key);
  }

  base::FilePath path;
  const bool success = base::PathService::Get(key, &path);
  CHECK(success && !path.empty());
  return path;
}

}  // namespace chrome_cleaner
