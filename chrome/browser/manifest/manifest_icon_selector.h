// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANIFEST_MANIFEST_ICON_SELECTOR_H_
#define CHROME_BROWSER_MANIFEST_MANIFEST_ICON_SELECTOR_H_

#include "base/macros.h"
#include "content/public/common/manifest.h"
#include "url/gurl.h"

// Selects the icon most closely matching the size constraints.  This follows
// very basic heuristics -- improvements are welcome.
class ManifestIconSelector {
 public:
  // Runs the algorithm to find the best matching icon in the icons listed in
  // the Manifest.
  //
  // Size is defined in Android's density-independent pixels (dp):
  // http://developer.android.com/guide/practices/screens_support.html
  // If/when this class is generalized, it may be a good idea to switch this to
  // taking in pixels, instead.
  //
  // Any icon returned will be close as possible to |ideal_icon_size_in_dp|
  // with a size not less than |minimum_icon_size_in_dp|.
  //
  // Returns the icon url if a suitable icon is found. An empty URL otherwise.
  static GURL FindBestMatchingIcon(
      const std::vector<content::Manifest::Icon>& icons,
      int ideal_icon_size_in_dp,
      int minimum_icon_size_in_dp);

  static int ConvertIconSizeFromDpToPx(int icon_size_in_dp);

 private:
  ManifestIconSelector(int ideal_icon_size_in_px,
                       int minimum_icon_size_in_px);
  virtual ~ManifestIconSelector() {}

  // Returns the square icon that is the smallest icon larger than
  // ideal_icon_size_in_px_ (if it exists), or the largest icon smaller than
  // ideal_icon_size_in_px_ otherwise.
  int FindClosestIconToIdealSize(
      const std::vector<content::Manifest::Icon>& icons) const;

  // Runs the algorithm to find the best matching icon in the icons listed in
  // the Manifest.
  // Returns the icon url if a suitable icon is found. An empty URL otherwise.
  int FindBestMatchingIcon(
      const std::vector<content::Manifest::Icon>& icons) const;

  // Returns whether the |preferred_icon_size_in_pixels_| is in |sizes|.
  bool IconSizesContainsPreferredSize(
      const std::vector<gfx::Size>& sizes) const;

  // Returns whether a size bigger than |minimun_icon_size_in_pixels_| is in
  // |sizes|.
  bool IconSizesContainsBiggerThanMinimumSize(
      const std::vector<gfx::Size>& sizes) const;

  // Returns an array containing the items in |icons| without the unsupported
  // image MIME types.
  static std::vector<content::Manifest::Icon> FilterIconsByType(
      const std::vector<content::Manifest::Icon>& icons);

  // Returns whether the 'any' (ie. gfx::Size(0,0)) is in |sizes|.
  static bool IconSizesContainsAny(const std::vector<gfx::Size>& sizes);

  const int ideal_icon_size_in_px_;
  const int minimum_icon_size_in_px_;

  friend class ManifestIconSelectorTest;

  DISALLOW_COPY_AND_ASSIGN(ManifestIconSelector);
};

#endif  // CHROME_BROWSER_MANIFEST_MANIFEST_ICON_SELECTOR_H_
