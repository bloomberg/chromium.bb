// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/app/blink_test_platform_support.h"

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/fontconfig_util_linux.h"

namespace content {

namespace {

const char* const kLocalFonts[] = {
  "AHEM____.TTF",
  "GardinerModBug.ttf",
  "GardinerModCat.ttf",
};

}  // namespace

bool CheckLayoutSystemDeps() {
  return true;
}

bool BlinkTestPlatformInitialize() {
  base::SetUpFontconfig();

  base::FilePath base_path;
  base::PathService::Get(base::DIR_MODULE, &base_path);
  for (size_t i = 0; i < arraysize(kLocalFonts); ++i) {
    if (!base::LoadFontIntoFontconfig(base_path.Append(kLocalFonts[i])))
      return false;
  }

  return true;
}

}  // namespace content
