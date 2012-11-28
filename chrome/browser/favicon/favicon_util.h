// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_UTIL_H_
#define CHROME_BROWSER_FAVICON_FAVICON_UTIL_H_

#include "ui/base/layout.h"

class GURL;

namespace content {
class RenderViewHost;
}

namespace gfx {
class Image;
}

namespace history {
struct FaviconBitmapResult;
}

// Utility class for common favicon related code.
class FaviconUtil {
 public:
  // Returns the scale factors at which favicons should be fetched. This is
  // different from ui::GetSupportedScaleFactors() because clients which do
  // not support 1x should still fetch a favicon for 1x to push to sync. This
  // guarantees that the clients receiving sync updates pushed by this client
  // receive a favicon (potentially of the wrong scale factor) and do not show
  // the default favicon.
  static std::vector<ui::ScaleFactor> GetFaviconScaleFactors();

  // Takes a vector of png-encoded frames, decodes them, and converts them to
  // a favicon of size favicon_size (in DIPs) at the desired ui scale factors.
  static gfx::Image SelectFaviconFramesFromPNGs(
      const std::vector<history::FaviconBitmapResult>& png_data,
      const std::vector<ui::ScaleFactor> scale_factors,
      int favicon_size);

  // Takes a vector of bitmaps and returns the index of the image that will best
  // produce an image of size |desired_size| for the given |scale_factors|.
  static size_t SelectBestFaviconFromBitmaps(
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<ui::ScaleFactor>& scale_factors,
      int desired_size);
};

#endif  // CHROME_BROWSER_FAVICON_FAVICON_UTIL_H_
