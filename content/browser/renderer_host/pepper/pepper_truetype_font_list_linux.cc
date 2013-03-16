// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_truetype_font_list.h"

#include <pango/pango.h>
#include <pango/pangocairo.h>

#include <string>

namespace content {

void GetFontFamilies_SlowBlocking(std::vector<std::string>* font_families) {
  PangoFontMap* font_map = ::pango_cairo_font_map_get_default();
  PangoFontFamily** families = NULL;
  int num_families = 0;
  ::pango_font_map_list_families(font_map, &families, &num_families);

  for (int i = 0; i < num_families; i++)
    font_families->push_back(::pango_font_family_get_name(families[i]));
  g_free(families);
}

}  // namespace content
