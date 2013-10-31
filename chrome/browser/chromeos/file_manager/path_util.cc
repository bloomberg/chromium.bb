// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/path_util.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"

namespace file_manager {
namespace util {

const char kDownloadsFolderName[] = "Downloads";

base::FilePath GetDownloadsFolderForProfile(Profile* profile) {
  // TODO(kinaba) crbug/309556: "Downloads" directory should be per-profile.
  //
  // For this to be per-profile, a unique directory path from profile->GetPath()
  // should be used rather than the HOME directory. We'll switch to the new path
  // once we have upgraded all code locations assuming the old path.
  base::FilePath path;
  CHECK(PathService::Get(base::DIR_HOME, &path));
  return path.AppendASCII(kDownloadsFolderName);
}

}  // namespace util
}  // namespace file_manager
