// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webui/options/font_settings_utils.h"

#include <set>
#include <string>

#include <pango/pango.h>
#include <pango/pangocairo.h>

#include "base/values.h"

ListValue* FontSettingsUtilities::GetFontsList() {
  ListValue* font_list = new ListValue;

  PangoFontMap* font_map = ::pango_cairo_font_map_get_default();
  PangoFontFamily** families = NULL;
  int num_families = 0;
  ::pango_font_map_list_families(font_map, &families, &num_families);

  std::set<std::string> sorted_families;
  for (int i = 0; i < num_families; i++) {
    sorted_families.insert(::pango_font_family_get_name(families[i]));
  }
  g_free(families);

  for (std::set<std::string>::const_iterator iter = sorted_families.begin();
       iter != sorted_families.end(); ++iter) {
    ListValue* font_item = new ListValue();
    font_item->Append(Value::CreateStringValue(*iter));
    font_item->Append(Value::CreateStringValue(*iter));  // localized name.
    // TODO(yusukes): Support localized family names.
    font_list->Append(font_item);
  }

  return font_list;
}

void FontSettingsUtilities::ValidateSavedFonts(PrefService* prefs) {
  // nothing to do for GTK.
}
