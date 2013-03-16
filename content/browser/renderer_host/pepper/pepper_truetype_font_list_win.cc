// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_truetype_font_list.h"

#include <windows.h>

#include "base/utf_string_conversions.h"
#include "base/win/scoped_hdc.h"

namespace content {

namespace {

static int CALLBACK EnumFontFamExProc(ENUMLOGFONTEXW* logical_font,
                                      NEWTEXTMETRICEXW* physical_font,
                                      DWORD font_type,
                                      LPARAM lparam) {
  std::vector<std::string>* font_families =
      reinterpret_cast<std::vector<std::string>*>(lparam);
  if (font_families) {
    const LOGFONTW& lf = logical_font->elfLogFont;
    if (lf.lfFaceName[0] && lf.lfFaceName[0] != '@' &&
        lf.lfOutPrecision == OUT_STROKE_PRECIS) {  // Outline fonts only.
      std::string face_name(UTF16ToUTF8(lf.lfFaceName));
      font_families->push_back(face_name);
    }
  }
  return 1;
}

}  // namespace

void GetFontFamilies_SlowBlocking(std::vector<std::string>* font_families) {
  LOGFONTW logfont;
  memset(&logfont, 0, sizeof(logfont));
  logfont.lfCharSet = DEFAULT_CHARSET;
  base::win::ScopedCreateDC hdc(::GetDC(NULL));
  ::EnumFontFamiliesExW(hdc, &logfont, (FONTENUMPROCW)&EnumFontFamExProc,
                        (LPARAM)font_families, 0);
}

}  // namespace content
