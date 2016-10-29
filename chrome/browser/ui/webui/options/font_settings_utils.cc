// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/font_settings_utils.h"

#include "ui/gfx/font_list.h"

namespace options {

std::string FontSettingsUtilities::ResolveFontList(
    const std::string& font_name_or_list) {
  if (!font_name_or_list.empty() && font_name_or_list[0] == ',')
    return gfx::FontList::FirstAvailableOrFirst(font_name_or_list);
  return font_name_or_list;
}

#if !defined(OS_WIN)
std::string FontSettingsUtilities::MaybeGetLocalizedFontName(
    const std::string& font_name_or_list) {
  return ResolveFontList(font_name_or_list);
}
#endif

}  // namespace options
