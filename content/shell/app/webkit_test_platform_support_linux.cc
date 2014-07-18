// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/app/webkit_test_platform_support.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "ui/gfx/test/fontconfig_util_linux.h"

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

bool WebKitTestPlatformInitialize() {
  gfx::SetUpFontconfig();

  base::FilePath base_path;
  PathService::Get(base::DIR_MODULE, &base_path);
  if (!gfx::LoadConfigFileIntoFontconfig(
          base_path.Append(FILE_PATH_LITERAL("fonts.conf"))))
    return false;

  for (size_t i = 0; i < gfx::kNumSystemFontsForFontconfig; ++i) {
    if (!gfx::LoadFontIntoFontconfig(
            base::FilePath(gfx::kSystemFontsForFontconfig[i]))) {
      return false;
    }
  }
  for (size_t i = 0; i < arraysize(kLocalFonts); ++i) {
    if (!gfx::LoadFontIntoFontconfig(base_path.Append(kLocalFonts[i])))
      return false;
  }

  base::FilePath garuda_path("/usr/share/fonts/truetype/thai/Garuda.ttf");
  if (!base::PathExists(garuda_path))
    garuda_path = base::FilePath("/usr/share/fonts/truetype/tlwg/Garuda.ttf");
  if (!gfx::LoadFontIntoFontconfig(garuda_path))
    return false;

  // We special case these fonts because they're only needed in a few layout
  // tests.
  base::FilePath lohit_path(
      "/usr/share/fonts/truetype/ttf-indic-fonts-core/lohit_pa.ttf");
  if (!base::PathExists(lohit_path)) {
    lohit_path = base::FilePath(
        "/usr/share/fonts/truetype/ttf-punjabi-fonts/lohit_pa.ttf");
  }
  gfx::LoadFontIntoFontconfig(lohit_path);

  return true;
}

}  // namespace content
