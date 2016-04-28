// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/manifest/manifest_icon_selector.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include "base/strings/utf_string_conversions.h"
#include "components/mime_util/mime_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/screen.h"

using content::Manifest;

ManifestIconSelector::ManifestIconSelector(int ideal_icon_size_in_px,
                                           int minimum_icon_size_in_px)
    : ideal_icon_size_in_px_(ideal_icon_size_in_px),
      minimum_icon_size_in_px_(minimum_icon_size_in_px) {
}

bool ManifestIconSelector::IconSizesContainsPreferredSize(
    const std::vector<gfx::Size>& sizes) const {
  for (size_t i = 0; i < sizes.size(); ++i) {
    if (sizes[i].height() != sizes[i].width())
      continue;
    if (sizes[i].width() == ideal_icon_size_in_px_)
      return true;
  }

  return false;
}

bool ManifestIconSelector::IconSizesContainsBiggerThanMinimumSize(
    const std::vector<gfx::Size>& sizes) const {
  for (size_t i = 0; i < sizes.size(); ++i) {
    if (sizes[i].height() != sizes[i].width())
      continue;
    if (sizes[i].width() >= minimum_icon_size_in_px_)
      return true;
  }
  return false;
}

int ManifestIconSelector::FindClosestIconToIdealSize(
    const std::vector<content::Manifest::Icon>& icons) const {
  int best_index = -1;
  int best_delta = std::numeric_limits<int>::min();

  for (size_t i = 0; i < icons.size(); ++i) {
    const std::vector<gfx::Size>& sizes = icons[i].sizes;
    for (size_t j = 0; j < sizes.size(); ++j) {
      if (sizes[j].height() != sizes[j].width())
        continue;
      int delta = sizes[j].width() - ideal_icon_size_in_px_;
      if (delta == 0)
        return i;
      if (best_delta > 0 && delta < 0)
        continue;
      if ((best_delta > 0 && delta < best_delta) ||
          (best_delta < 0 && delta > best_delta)) {
        best_index = i;
        best_delta = delta;
      }
    }
  }

  return best_index;
}

int ManifestIconSelector::FindBestMatchingIcon(
    const std::vector<content::Manifest::Icon>& icons) const {
  int best_index = -1;

  // The first pass is to find the ideal icon - one with the exact right size.
  for (size_t i = 0; i < icons.size(); ++i) {
    if (IconSizesContainsPreferredSize(icons[i].sizes))
      return i;

    // If there is an icon size 'any', keep it on the side and only use it if
    // nothing better is found.
    if (IconSizesContainsAny(icons[i].sizes))
      best_index = i;
  }
  if (best_index != -1)
    return best_index;

  // The last pass will try to find the smallest icon larger than the ideal
  // size, or the largest icon smaller than the ideal size.
  best_index = FindClosestIconToIdealSize(icons);

  if (best_index != -1 &&
        IconSizesContainsBiggerThanMinimumSize(icons[best_index].sizes))
    return best_index;

  return -1;
}


// static
bool ManifestIconSelector::IconSizesContainsAny(
    const std::vector<gfx::Size>& sizes) {
  for (size_t i = 0; i < sizes.size(); ++i) {
    if (sizes[i].IsEmpty())
      return true;
  }
  return false;
}

// static
std::vector<Manifest::Icon> ManifestIconSelector::FilterIconsByType(
    const std::vector<content::Manifest::Icon>& icons) {
  std::vector<Manifest::Icon> result;

  for (size_t i = 0; i < icons.size(); ++i) {
    if (icons[i].type.is_null() ||
        mime_util::IsSupportedImageMimeType(
            base::UTF16ToUTF8(icons[i].type.string()))) {
      result.push_back(icons[i]);
    }
  }

  return result;
}

// static
GURL ManifestIconSelector::FindBestMatchingIcon(
    const std::vector<Manifest::Icon>& unfiltered_icons,
    const int ideal_icon_size_in_dp,
    const int minimum_icon_size_in_dp) {
  DCHECK(minimum_icon_size_in_dp <= ideal_icon_size_in_dp);

  const int ideal_icon_size_in_px =
    ConvertIconSizeFromDpToPx(ideal_icon_size_in_dp);
  const int minimum_icon_size_in_px =
    ConvertIconSizeFromDpToPx(minimum_icon_size_in_dp);

  std::vector<Manifest::Icon> icons =
      ManifestIconSelector::FilterIconsByType(unfiltered_icons);

  ManifestIconSelector selector(ideal_icon_size_in_px,
                                minimum_icon_size_in_px);
  int index = selector.FindBestMatchingIcon(icons);
  if (index == -1)
    return GURL();
  return icons[index].src;
}

// static
int ManifestIconSelector::ConvertIconSizeFromDpToPx(int icon_size_in_dp) {
  return static_cast<int>(round(
      icon_size_in_dp *
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor()));
}
