// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_paths.h"

#include "base/path_service.h"

namespace content {

bool PathProvider(int key, FilePath* result) {
  switch (key) {
    case CHILD_PROCESS_EXE:
      return PathService::Get(base::FILE_EXE, result);
    default:
      break;
  }

  return false;
}

// This cannot be done as a static initializer sadly since Visual Studio will
// eliminate this object file if there is no direct entry point into it.
void RegisterPathProvider() {
  PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace content
