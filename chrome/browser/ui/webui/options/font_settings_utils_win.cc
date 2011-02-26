// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/font_settings_utils.h"

#include <set>
#include <string>
#include <windows.h>

#include "base/values.h"

static int CALLBACK EnumFontFamExProc(ENUMLOGFONTEXW *logical_font,
                                      NEWTEXTMETRICEXW *physical_font,
                                      DWORD font_type,
                                      LPARAM lparam) {
  std::set<std::wstring>* font_names =
      reinterpret_cast<std::set<std::wstring>*>(lparam);
  if (font_names) {
    const LOGFONTW& lf = logical_font->elfLogFont;
    if (lf.lfFaceName[0] && lf.lfFaceName[0] != '@') {
      std::wstring face_name(lf.lfFaceName);
      font_names->insert(face_name);
    }
  }
  return 1;
}

ListValue* FontSettingsUtilities::GetFontsList() {
  std::set<std::wstring> font_names;

  LOGFONTW logfont;
  memset(&logfont, 0, sizeof(logfont));
  logfont.lfCharSet = DEFAULT_CHARSET;

  HDC hdc = ::GetDC(NULL);
  ::EnumFontFamiliesExW(hdc, &logfont, (FONTENUMPROCW)&EnumFontFamExProc,
                        (LPARAM)&font_names, 0);
  ::ReleaseDC(NULL, hdc);

  ListValue* font_list = new ListValue;
  std::set<std::wstring>::iterator iter;
  for (iter = font_names.begin(); iter != font_names.end(); iter++) {
    ListValue* font_item = new ListValue();
    font_item->Append(Value::CreateStringValue(*iter));
    font_item->Append(Value::CreateStringValue(*iter));
    font_list->Append(font_item);
  }
  return font_list;
}

void FontSettingsUtilities::ValidateSavedFonts(PrefService* prefs) {
  // nothing to do for Windows.
}

