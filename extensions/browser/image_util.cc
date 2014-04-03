// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/image_util.h"

#include <stdint.h>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "third_party/skia/include/core/SkColor.h"

namespace extensions {
namespace image_util {

bool ParseCSSColorString(const std::string& color_string, SkColor* result) {
  std::string formatted_color;
  // Check the string for incorrect formatting.
  if (color_string.empty() || color_string[0] != '#')
    return false;

  // Convert the string from #FFF format to #FFFFFF format.
  if (color_string.length() == 4) {
    for (size_t i = 1; i < 4; ++i) {
      formatted_color += color_string[i];
      formatted_color += color_string[i];
    }
  } else if (color_string.length() == 7) {
    formatted_color = color_string.substr(1, 6);
  } else {
    return false;
  }

  // Convert the string to an integer and make sure it is in the correct value
  // range.
  std::vector<uint8_t> color_bytes;
  if (!base::HexStringToBytes(formatted_color, &color_bytes))
    return false;

  *result = SkColorSetARGB(255, color_bytes[0], color_bytes[1], color_bytes[2]);
  return true;
}

}  // namespace image_util
}  // namespace extensions
