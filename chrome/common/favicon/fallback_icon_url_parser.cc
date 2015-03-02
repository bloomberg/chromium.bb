// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/favicon/fallback_icon_url_parser.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "third_party/skia/include/utils/SkParse.h"
#include "ui/gfx/favicon_size.h"

namespace chrome {

ParsedFallbackIconPath::ParsedFallbackIconPath()
    : size_in_pixels_(gfx::kFaviconSize) {
}

ParsedFallbackIconPath::~ParsedFallbackIconPath() {
}

bool ParsedFallbackIconPath::Parse(const std::string& path) {
  if (path.empty())
    return false;

  size_t slash = path.find("/", 0);
  if (slash == std::string::npos)
    return false;
  std::string spec_str = path.substr(0, slash);
  if (!ParseSpecs(spec_str, &size_in_pixels_, &style_))
    return false;  // Parse failed.

  // Extract URL, which may be empty (if first slash appears at the end).
  std::string url_str = path.substr(slash + 1);
  url_ = GURL(url_str);
  return url_str.empty() || url_.is_valid();  // Allow empty URL.
}

// static
bool ParsedFallbackIconPath::ParseSpecs(
    const std::string& specs_str,
    int *size,
    favicon_base::FallbackIconStyle* style) {
  DCHECK(size);
  DCHECK(style);

  std::vector<std::string> tokens;
  base::SplitStringDontTrim(specs_str, ',', &tokens);
  if (tokens.size() != 5)  // Force "," for empty fields.
    return false;

  *size = gfx::kFaviconSize;
  if (!tokens[0].empty() && !base::StringToInt(tokens[0], size))
    return false;
  if (*size <= 0)
    return false;

  if (!tokens[1].empty() && !ParseColor(tokens[1], &style->background_color))
    return false;

  if (tokens[2].empty())
    favicon_base::MatchFallbackIconTextColorAgainstBackgroundColor(style);
  else if (!ParseColor(tokens[2], &style->text_color))
    return false;

  if (!tokens[3].empty() &&
      !base::StringToDouble(tokens[3], &style->font_size_ratio))
    return false;

  if (!tokens[4].empty() && !base::StringToDouble(tokens[4], &style->roundness))
    return false;

  return favicon_base::ValidateFallbackIconStyle(*style);
}

// static
bool ParsedFallbackIconPath::ParseColor(const std::string& color_str,
                                        SkColor* color) {
  const char* end = SkParse::FindColor(color_str.c_str(), color);
  // Return true if FindColor() succeeds and |color_str| is entirely consumed.
  return end && !*end;
}

}  // namespace chrome
