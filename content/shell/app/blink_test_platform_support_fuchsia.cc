// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/app/blink_test_platform_support.h"

#include "skia/ext/fontmgr_default_android.h"
#include "third_party/skia/include/ports/SkFontMgr.h"
#include "third_party/skia/include/ports/SkFontMgr_android.h"

namespace content {

bool CheckLayoutSystemDeps() {
  return true;
}

bool BlinkTestPlatformInitialize() {
  // Initialize Skia with the font configuration files crafted for layout tests.
  SkFontMgr_Android_CustomFonts custom;
  custom.fSystemFontUse = SkFontMgr_Android_CustomFonts::kOnlyCustom;
  custom.fBasePath = "/system/fonts/";
  custom.fFontsXml = "/system/fonts/fonts.xml";
  custom.fFallbackFontsXml = "/system/fonts/fonts_fallback.xml";
  custom.fIsolated = false;

  SetDefaultSkiaFactory(SkFontMgr_New_Android(&custom));

  return true;
}

}  // namespace content
