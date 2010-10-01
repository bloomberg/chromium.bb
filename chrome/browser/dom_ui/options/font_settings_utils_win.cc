// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/options/font_settings_utils.h"

#include <windows.h>

#include "base/values.h"

static int CALLBACK EnumFontFamExProc(ENUMLOGFONTEXW *logical_font,
                                      NEWTEXTMETRICEXW *physical_font,
                                      DWORD font_type,
                                      LPARAM lparam) {
  ListValue* font_list = reinterpret_cast<ListValue*>(lparam);
  if (font_list) {
    const LOGFONTW& lf = logical_font->elfLogFont;
    if (lf.lfFaceName[0] && lf.lfFaceName[0] != '@') {
      ListValue* font_item = new ListValue();
      std::wstring face_name(lf.lfFaceName);
      font_item->Append(Value::CreateStringValue(face_name));
      font_item->Append(Value::CreateStringValue(face_name));
      font_list->Append(font_item);
    }
  }
  return 1;
}

ListValue* FontSettingsUtilities::GetFontsList() {
  ListValue* font_list = new ListValue;

  LOGFONTW logfont;
  memset(&logfont, 0, sizeof(logfont));
  logfont.lfCharSet = DEFAULT_CHARSET;

  HDC hdc = ::GetDC(NULL);
  ::EnumFontFamiliesExW(hdc, &logfont, (FONTENUMPROCW)&EnumFontFamExProc,
                        (LPARAM)font_list, 0);
  ::ReleaseDC(NULL, hdc);

  return font_list;
}

void FontSettingsUtilities::ValidateSavedFonts(PrefService* prefs) {
  // nothing to do for Windows.
}

