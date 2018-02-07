// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"

#include <stdlib.h>

#include "base/base_paths_fuchsia.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/process.h"

namespace base {
namespace {

constexpr char kPackageRoot[] = "/pkg";

}  // namespace

base::FilePath GetPackageRoot() {
  base::FilePath path_obj(kPackageRoot);

  // Fuchsia's appmgr will set argv[0] to a fully qualified executable path
  // under /pkg for packaged binaries.
  if (path_obj.IsParent(base::CommandLine::ForCurrentProcess()->GetProgram())) {
    return path_obj;
  } else {
    return base::FilePath();
  }
}

bool PathProviderFuchsia(int key, FilePath* result) {
  switch (key) {
    case FILE_MODULE:
      NOTIMPLEMENTED();
      return false;
    case FILE_EXE: {
      *result = base::MakeAbsoluteFilePath(base::FilePath(
          base::CommandLine::ForCurrentProcess()->GetProgram().AsUTF8Unsafe()));
      return true;
    }
    case DIR_SOURCE_ROOT:
      *result = GetPackageRoot();
      if (result->empty()) {
        *result = FilePath("/system");
      }
      return true;
    case DIR_CACHE:
      *result = FilePath("/data");
      return true;
    case DIR_ASSETS:
      *result = GetPackageRoot();
      if (result->empty()) {
        return PathService::Get(DIR_EXE, result);
      }
      return true;
  }
  return false;
}

}  // namespace base
