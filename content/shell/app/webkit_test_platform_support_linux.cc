// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/app/webkit_test_platform_support.h"

#include <fontconfig/fontconfig.h>
#include <unistd.h>

#include <iostream>

#include "base/files/file_path.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"

namespace content {

namespace {

bool CheckAndLoadFontFile(
    FcConfig* fontcfg, const char* path1, const char* path2) {
  const char* font = path1;
  if (access(font, R_OK) < 0) {
    font = path2;
    if (access(font, R_OK) < 0) {
      LOG(WARNING) << "You are missing " << path1 << " or " << path2 << ". "
                   << "Without this, some layout tests may fail. See "
                   << "http://code.google.com/p/chromium/wiki/LayoutTestsLinux "
                   << "for more.\n";
      return false;
    }
  }
  if (!FcConfigAppFontAddFile(
          fontcfg, reinterpret_cast<const FcChar8*>(font))) {
    LOG(ERROR) << "Failed to load font " << font << "\n";
    return false;
  }
  return true;
}

static bool LoadFontResources(const base::FilePath& base_path,
                              FcConfig* font_config) {
  const char* const own_fonts[] = {"AHEM____.TTF", "GardinerModBug.ttf",
                                   "GardinerModCat.ttf"};

  for (size_t i = 0; i < arraysize(own_fonts); ++i) {
    base::FilePath font_path = base_path.Append(own_fonts[i]);
    if (access(font_path.value().c_str(), R_OK) < 0 ||
        !FcConfigAppFontAddFile(
            font_config,
            reinterpret_cast<const FcChar8*>(font_path.value().c_str()))) {
      LOG(ERROR) << "Failed to load test font resource "
                 << font_path.value().c_str() << ".\n";
      return false;
    }
  }
  return true;
}

const char* const kFonts[] = {
    "/usr/share/fonts/truetype/kochi/kochi-gothic.ttf",
    "/usr/share/fonts/truetype/kochi/kochi-mincho.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Arial_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Arial_Bold_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Arial_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Comic_Sans_MS.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Comic_Sans_MS_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Courier_New.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Courier_New_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Courier_New_Bold_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Courier_New_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Georgia.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Georgia_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Georgia_Bold_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Georgia_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Impact.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Trebuchet_MS.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Trebuchet_MS_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Trebuchet_MS_Bold_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Trebuchet_MS_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Bold_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Verdana.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Verdana_Bold.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Verdana_Bold_Italic.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Verdana_Italic.ttf",
    // The DejaVuSans font is used by the css2.1 tests.
    "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/ttf-indic-fonts-core/lohit_hi.ttf",
    "/usr/share/fonts/truetype/ttf-indic-fonts-core/lohit_ta.ttf",
    "/usr/share/fonts/truetype/ttf-indic-fonts-core/MuktiNarrow.ttf",
};

bool SetupFontConfig() {
  FcInit();

  base::FilePath base_path;
  PathService::Get(base::DIR_MODULE, &base_path);
  base::FilePath fonts_conf = base_path.Append(FILE_PATH_LITERAL("fonts.conf"));

  FcConfig* font_config = FcConfigCreate();
  if (!FcConfigParseAndLoad(
          font_config,
          reinterpret_cast<const FcChar8*>(fonts_conf.value().c_str()),
          true)) {
    LOG(ERROR) << "Failed to parse fontconfig config file\n";
    return false;
  }

  for (size_t i = 0; i < arraysize(kFonts); ++i) {
    if (access(kFonts[i], R_OK) < 0) {
      LOG(ERROR) << "You are missing " << kFonts[i] << ". Try re-running "
                 << "build/install-build-deps.sh. Also see "
                 << "http://code.google.com/p/chromium/wiki/LayoutTestsLinux";
      return false;
    }
    if (!FcConfigAppFontAddFile(
            font_config, reinterpret_cast<const FcChar8*>(kFonts[i]))) {
      LOG(ERROR) << "Failed to load font " << kFonts[i] << "\n";
      return false;
    }
  }

  if (!CheckAndLoadFontFile(
          font_config,
          "/usr/share/fonts/truetype/thai/Garuda.ttf",
          "/usr/share/fonts/truetype/tlwg/Garuda.ttf")) {
    return false;
  }

  // We special case these fonts because they're only needed in a few layout
  // tests.
  CheckAndLoadFontFile(
      font_config,
      "/usr/share/fonts/truetype/ttf-indic-fonts-core/lohit_pa.ttf",
      "/usr/share/fonts/truetype/ttf-punjabi-fonts/lohit_pa.ttf");

  if (!LoadFontResources(base_path, font_config))
    return false;

  if (!FcConfigSetCurrent(font_config)) {
    LOG(ERROR) << "Failed to set the default font configuration\n";
    return false;
  }

  return true;
}

}  // namespace

bool CheckLayoutSystemDeps() {
  return true;
}

bool WebKitTestPlatformInitialize() {
  return SetupFontConfig();
}

}  // namespace content
