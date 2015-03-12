// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/manifest/manifest_icon_selector.h"

#include <limits>

#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/web_contents.h"
#include "net/base/mime_util.h"
#include "ui/gfx/screen.h"

using content::Manifest;

ManifestIconSelector::ManifestIconSelector(float preferred_icon_size_in_pixels)
    : preferred_icon_size_in_pixels_(preferred_icon_size_in_pixels) {
}

bool ManifestIconSelector::IconSizesContainsPreferredSize(
    const std::vector<gfx::Size>& sizes) {
  for (size_t i = 0; i < sizes.size(); ++i) {
    if (sizes[i].height() != sizes[i].width())
      continue;
    if (sizes[i].width() == preferred_icon_size_in_pixels_)
      return true;
  }

  return false;
}

GURL ManifestIconSelector::FindBestMatchingIconForDensity(
    const std::vector<content::Manifest::Icon>& icons,
    float density) {
  GURL url;
  int best_delta = std::numeric_limits<int>::min();

  for (size_t i = 0; i < icons.size(); ++i) {
    if (icons[i].density != density)
        continue;

    const std::vector<gfx::Size>& sizes = icons[i].sizes;
    for (size_t j = 0; j < sizes.size(); ++j) {
      if (sizes[j].height() != sizes[j].width())
        continue;
      int delta = sizes[j].width() - preferred_icon_size_in_pixels_;
      if (delta == 0)
        return icons[i].src;
      if (best_delta > 0 && delta < 0)
        continue;
      if ((best_delta > 0 && delta < best_delta) ||
          (best_delta < 0 && delta > best_delta)) {
        url = icons[i].src;
        best_delta = delta;
      }
    }
  }

  return url;
}

GURL ManifestIconSelector::FindBestMatchingIcon(
    const std::vector<content::Manifest::Icon>& unfiltered_icons,
    float density) {
  GURL url;
  std::vector<Manifest::Icon> icons = FilterIconsByType(unfiltered_icons);

  // The first pass is to find the ideal icon. That icon is of the right size
  // with the default density or the device's density.
  for (size_t i = 0; i < icons.size(); ++i) {
    if (icons[i].density == density &&
        IconSizesContainsPreferredSize(icons[i].sizes)) {
      return icons[i].src;
    }

    // If there is an icon with the right size but not the right density, keep
    // it on the side and only use it if nothing better is found.
    if (icons[i].density == Manifest::Icon::kDefaultDensity &&
        IconSizesContainsPreferredSize(icons[i].sizes)) {
      url = icons[i].src;
    }
  }

  // The second pass is to find an icon with 'any'. The current device scale
  // factor is preferred. Otherwise, the default scale factor is used.
  for (size_t i = 0; i < icons.size(); ++i) {
    if (icons[i].density == density &&
        IconSizesContainsAny(icons[i].sizes)) {
      return icons[i].src;
    }

    // If there is an icon with 'any' but not the right density, keep it on the
    // side and only use it if nothing better is found.
    if (icons[i].density == Manifest::Icon::kDefaultDensity &&
        IconSizesContainsAny(icons[i].sizes)) {
      url = icons[i].src;
    }
  }

  // The last pass will try to find the best suitable icon for the device's
  // scale factor. If none, another pass will be run using kDefaultDensity.
  if (!url.is_valid())
    url = FindBestMatchingIconForDensity(icons, density);
  if (!url.is_valid())
    url = FindBestMatchingIconForDensity(icons,
                                         Manifest::Icon::kDefaultDensity);

  return url;
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
        net::IsSupportedImageMimeType(
            base::UTF16ToUTF8(icons[i].type.string()))) {
      result.push_back(icons[i]);
    }
  }

  return result;
}

// static
GURL ManifestIconSelector::FindBestMatchingIcon(
    const std::vector<Manifest::Icon>& unfiltered_icons,
    const float preferred_icon_size_in_dp,
    const gfx::Screen* screen) {
  const float device_scale_factor =
      screen->GetPrimaryDisplay().device_scale_factor();
  const float preferred_icon_size_in_pixels =
      preferred_icon_size_in_dp * device_scale_factor;

  ManifestIconSelector selector(preferred_icon_size_in_pixels);
  return selector.FindBestMatchingIcon(unfiltered_icons, device_scale_factor);
}
