// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SEARCH_SELECTED_COLORS_INFO_H_
#define CHROME_COMMON_SEARCH_SELECTED_COLORS_INFO_H_

#include <stdint.h>

#include "third_party/skia/include/core/SkColor.h"

namespace chrome_colors {

struct ColorInfo {
  ColorInfo(int id, SkColor color, const char* label)
      : id(id), color(color), label(label) {}
  constexpr ColorInfo(int id,
                      SkColor color,
                      const char* label,
                      const char* icon_data)
      : id(id), color(color), label(label), icon_data(icon_data) {}
  int id;
  SkColor color;
  const char* label;
  const char* icon_data;
};

// TODO(gayane): Add colors selected by UX.
// List of preselected colors to show in Chrome Colors menu.
const ColorInfo kSelectedColorsInfo[] = {};

}  // namespace chrome_colors

#endif  // CHROME_COMMON_SEARCH_SELECTED_COLORS_INFO_H_
