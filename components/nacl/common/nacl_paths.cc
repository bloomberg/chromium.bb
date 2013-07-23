// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/common/nacl_paths.h"

#include "base/file_util.h"
#include "base/path_service.h"

namespace {

#if defined(OS_POSIX) && !defined(OS_MACOSX)
// File name of the nacl_helper and nacl_helper_bootstrap, Linux only.
const base::FilePath::CharType kInternalNaClHelperFileName[] =
    FILE_PATH_LITERAL("nacl_helper");
const base::FilePath::CharType kInternalNaClHelperBootstrapFileName[] =
    FILE_PATH_LITERAL("nacl_helper_bootstrap");
#endif

}  // namespace

namespace nacl {

bool PathProvider(int key, base::FilePath* result) {
  base::FilePath cur;
  switch (key) {
#if defined(OS_LINUX)
    case FILE_NACL_HELPER:
      if (!PathService::Get(base::DIR_MODULE, &cur))
        return false;
      cur = cur.Append(kInternalNaClHelperFileName);
      break;
    case FILE_NACL_HELPER_BOOTSTRAP:
      if (!PathService::Get(base::DIR_MODULE, &cur))
        return false;
      cur = cur.Append(kInternalNaClHelperBootstrapFileName);
      break;
#endif
    default:
      return false;
  }

  *result = cur;
  return true;
}

// This cannot be done as a static initializer sadly since Visual Studio will
// eliminate this object file if there is no direct entry point into it.
void RegisterPathProvider() {
  PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace nacl
