// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/instant_types.h"

InstantSuggestion::InstantSuggestion() {
}

InstantSuggestion::InstantSuggestion(const base::string16& in_text,
                                     const std::string& in_metadata)
    : text(in_text),
      metadata(in_metadata) {
}

InstantSuggestion::~InstantSuggestion() {
}

RGBAColor::RGBAColor()
    : r(0),
      g(0),
      b(0),
      a(0) {
}

RGBAColor::~RGBAColor() {
}

bool RGBAColor::operator==(const RGBAColor& rhs) const {
  return r == rhs.r &&
      g == rhs.g &&
      b == rhs.b &&
      a == rhs.a;
}

ThemeBackgroundInfo::ThemeBackgroundInfo()
    : using_default_theme(true),
      background_color(),
      text_color(),
      link_color(),
      text_color_light(),
      header_color(),
      section_border_color(),
      image_horizontal_alignment(THEME_BKGRND_IMAGE_ALIGN_CENTER),
      image_vertical_alignment(THEME_BKGRND_IMAGE_ALIGN_CENTER),
      image_tiling(THEME_BKGRND_IMAGE_NO_REPEAT),
      image_height(0),
      has_attribution(false),
      logo_alternate(false) {
}

ThemeBackgroundInfo::~ThemeBackgroundInfo() {
}

bool ThemeBackgroundInfo::operator==(const ThemeBackgroundInfo& rhs) const {
  return using_default_theme == rhs.using_default_theme &&
      background_color == rhs.background_color &&
      text_color == rhs.text_color &&
      link_color == rhs.link_color &&
      text_color_light == rhs.text_color_light &&
      header_color == rhs.header_color &&
      section_border_color == rhs.section_border_color &&
      theme_id == rhs.theme_id &&
      image_horizontal_alignment == rhs.image_horizontal_alignment &&
      image_vertical_alignment == rhs.image_vertical_alignment &&
      image_tiling == rhs.image_tiling &&
      image_height == rhs.image_height &&
      has_attribution == rhs.has_attribution &&
      logo_alternate == rhs.logo_alternate;
}
