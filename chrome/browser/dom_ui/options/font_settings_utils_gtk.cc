// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/options/font_settings_utils.h"

#include <pango/pango.h>
#include <pango/pangocairo.h>

#include "base/values.h"

ListValue* FontSettingsUtilities::GetFontsList() {
  ListValue* font_list = new ListValue;

  PangoFontMap* font_map = ::pango_cairo_font_map_get_default();
  PangoFontFamily** families = NULL;
  int num_families = 0;
  ::pango_font_map_list_families(font_map, &families, &num_families);

  for (int i = 0; i < num_families; i++) {
    ListValue* font_item = new ListValue();
    const char* family_name = ::pango_font_family_get_name(families[i]);
    font_item->Append(Value::CreateStringValue(family_name));
    font_item->Append(Value::CreateStringValue(family_name));
    font_list->Append(font_item);
  }

  g_free(families);

  return font_list;
}

void FontSettingsUtilities::ValidateSavedFonts(PrefService* prefs) {
  // nothing to do for GTK.
}

