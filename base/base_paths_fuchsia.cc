// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"

#include <stdlib.h>

#include "base/base_paths_fuchsia.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/process/process.h"

namespace base {

base::LazyInstance<base::FilePath>::Leaky g_data_path =
    LAZY_INSTANCE_INITIALIZER;

void SetPersistentDataPath(const base::FilePath path) {
  g_data_path.Get() = path;
}

bool PathProviderFuchsia(int key, FilePath* result) {
  switch (key) {
    case FILE_MODULE:
      NOTIMPLEMENTED();
      return false;
    case FILE_EXE:
      *result = CommandLine::ForCurrentProcess()->GetProgram();
      return true;
    case DIR_APP_DATA:
    case DIR_CACHE:
      *result = g_data_path.Get();
      return true;
    case DIR_ASSETS:
    case DIR_SOURCE_ROOT:
      *result = base::FilePath("/pkg");
      return true;
  }
  return false;
}

}  // namespace base
