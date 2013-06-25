// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_suite_init.h"

#import <Cocoa/Cocoa.h>

#include "base/files/file_path.h"
#include "base/mac/bundle_locations.h"
#include "base/path_service.h"

namespace ash {
namespace test {

void OverrideFrameworkBundle() {
  // Look in the AuraShell.app directory for resources.
  base::FilePath path;
  PathService::Get(base::DIR_EXE, &path);
  path = path.Append(FILE_PATH_LITERAL("AuraShell.app"));
  base::mac::SetOverrideFrameworkBundlePath(path);
}

}  // namespace test
}  // namespace ash
