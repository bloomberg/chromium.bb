// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANIFEST_MANIFEST_ICON_SELECTOR_H_
#define CHROME_BROWSER_MANIFEST_MANIFEST_ICON_SELECTOR_H_

#include "base/macros.h"
#include "content/public/common/manifest.h"
#include "url/gurl.h"

// Selects the square icon with the supported image MIME types and the specified
// icon purpose that most closely matches the size constraints.
// This follows very basic heuristics -- improvements are welcome.
class ManifestIconSelector {
 public:
  // Runs the algorithm to find the best matching icon in the icons listed in
  // the Manifest. Size is defined in pixels.
  //
  // Any icon returned will be close as possible to |ideal_icon_size_in_px|
  // with a size not less than |minimum_icon_size_in_px|. Additionally, it must
  // be square, have supported image MIME types, and have icon purpose
  // |purpose|.
  //
  // Returns the icon url if a suitable icon is found. An empty URL otherwise.
  static GURL FindBestMatchingIcon(
      const std::vector<content::Manifest::Icon>& icons,
      int ideal_icon_size_in_px,
      int minimum_icon_size_in_px,
      content::Manifest::Icon::IconPurpose purpose);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ManifestIconSelector);
};

#endif  // CHROME_BROWSER_MANIFEST_MANIFEST_ICON_SELECTOR_H_
