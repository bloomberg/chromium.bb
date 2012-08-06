// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_SELECT_FAVICON_FRAMES_H_
#define CHROME_BROWSER_FAVICON_SELECT_FAVICON_FRAMES_H_

#include <vector>

#include "ui/base/layout.h"

class SkBitmap;

namespace gfx {
class ImageSkia;
}

// Takes a list of all bitmaps found in a .ico file, and creates an
// ImageSkia that's desired_size x desired_size pixels big. This
// function adds a representation at every desired scale factor.
// If desired_size is 0, the largest bitmap is returned unmodified.
gfx::ImageSkia SelectFaviconFrames(
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<ui::ScaleFactor>& scale_factors,
    int desired_size);

#endif  // CHROME_BROWSER_FAVICON_SELECT_FAVICON_FRAMES_H_
