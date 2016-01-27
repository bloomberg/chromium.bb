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
#include "ui/gfx/screen.h"

using content::Manifest;

ManifestIconSelector::ManifestIconSelector(int ideal_icon_size_in_px,
                                           int minimum_icon_size_in_px)
    : ideal_icon_size_in_px_(ideal_icon_size_in_px),
      minimum_icon_size_in_px_(minimum_icon_size_in_px) {
}

bool ManifestIconSelector::IconSizesContainsPreferredSize(
    const std::vector<gfx::Size>& sizes) {
  for (size_t i = 0; i < sizes.size(); ++i) {
    if (sizes[i].height() != sizes[i].width())
      continue;
    if (sizes[i].width() == ideal_icon_size_in_px_)
      return true;
  }

  return false;
}

bool ManifestIconSelector::IconSizesContainsBiggerThanMinimumSize(
    const std::vector<gfx::Size>& sizes) {
  for (size_t i = 0; i < sizes.size(); ++i) {
    if (sizes[i].height() != sizes[i].width())
      continue;
    if (sizes[i].width() >= minimum_icon_size_in_px_)
      return true;
  }
  return false;
}

int ManifestIconSelector::FindBestMatchingIconForDensity(
    const std::vector<content::Manifest::Icon>& icons,
    float density) {
  int best_index = -1;
  int best_delta = std::numeric_limits<int>::min();

  for (size_t i = 0; i < icons.size(); ++i) {
    if (icons[i].density != density)
        continue;

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
    const std::vector<content::Manifest::Icon>& icons,
    float density) {
  int best_index = -1;

  // The first pass is to find the ideal icon. That icon is of the right size
  // with the default density or the device's density.
  for (size_t i = 0; i < icons.size(); ++i) {
    if (icons[i].density == density &&
        IconSizesContainsPreferredSize(icons[i].sizes)) {
      return i;
    }

    // If there is an icon with the right size but not the right density, keep
    // it on the side and only use it if nothing better is found.
    if (icons[i].density == Manifest::Icon::kDefaultDensity &&
        IconSizesContainsPreferredSize(icons[i].sizes)) {
      best_index = i;
    }
  }

  if (best_index != -1)
    return best_index;

  // The second pass is to find an icon with 'any'. The current device scale
  // factor is preferred. Otherwise, the default scale factor is used.
  for (size_t i = 0; i < icons.size(); ++i) {
    if (icons[i].density == density &&
        IconSizesContainsAny(icons[i].sizes)) {
      return i;
    }

    // If there is an icon with 'any' but not the right density, keep it on the
    // side and only use it if nothing better is found.
    if (icons[i].density == Manifest::Icon::kDefaultDensity &&
        IconSizesContainsAny(icons[i].sizes)) {
      best_index = i;
    }
  }

  if (best_index != -1)
    return best_index;

  // The last pass will try to find the best suitable icon for the device's
  // scale factor. If none, another pass will be run using kDefaultDensity.
  best_index = FindBestMatchingIconForDensity(icons, density);
  if (best_index != -1 &&
        IconSizesContainsBiggerThanMinimumSize(icons[best_index].sizes))
    return best_index;

  best_index = FindBestMatchingIconForDensity(icons,
      Manifest::Icon::kDefaultDensity);
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

  const float device_scale_factor =
      gfx::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();
  const int ideal_icon_size_in_px =
      static_cast<int>(round(ideal_icon_size_in_dp * device_scale_factor));
  const int minimum_icon_size_in_px =
      static_cast<int>(round(minimum_icon_size_in_dp * device_scale_factor));

  std::vector<Manifest::Icon> icons =
      ManifestIconSelector::FilterIconsByType(unfiltered_icons);

  ManifestIconSelector selector(ideal_icon_size_in_px,
                                minimum_icon_size_in_px);
  int index = selector.FindBestMatchingIcon(icons, device_scale_factor);
  if (index == -1)
    return GURL();
  return icons[index].src;
}
