// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_UTIL_H_
#define CHROME_BROWSER_FAVICON_FAVICON_UTIL_H_

#include <vector>

#include "ui/base/layout.h"

namespace chrome {
struct FaviconBitmapResult;
}

namespace gfx {
class Image;
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

  // Sets the color space used for converting |image| to an NSImage to the
  // system colorspace. This makes the favicon look the same in the browser UI
  // as it does in the renderer.
  static void SetFaviconColorSpace(gfx::Image* image);

  // Takes a vector of png-encoded frames, decodes them, and converts them to
  // a favicon of size favicon_size (in DIPs) at the desired ui scale factors.
  static gfx::Image SelectFaviconFramesFromPNGs(
      const std::vector<chrome::FaviconBitmapResult>& png_data,
      const std::vector<ui::ScaleFactor>& scale_factors,
      int favicon_size);
};

#endif  // CHROME_BROWSER_FAVICON_FAVICON_UTIL_H_
