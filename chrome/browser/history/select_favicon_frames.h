// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_SELECT_FAVICON_FRAMES_H_
#define CHROME_BROWSER_HISTORY_SELECT_FAVICON_FRAMES_H_

#include <vector>

#include "chrome/browser/history/history_types.h"
#include "ui/base/layout.h"

class SkBitmap;

namespace gfx {
class ImageSkia;
}

// Score which is smaller than the minimum score returned by
// SelectFaviconFrames() or SelectFaviconBitmapIDs().
extern const float kSelectFaviconFramesInvalidScore;

// Takes a list of all bitmaps found in a .ico file, and creates an
// ImageSkia that's |desired_size| x |desired_size| DIP big. This
// function adds a representation at every desired scale factor.
// If |desired_size| is 0, the largest bitmap is returned unmodified.
// If score is non-NULL, it receives a score between 0 (bad) and 1 (good)
// that describes how well |bitmaps| were able to produce an image at
// |desired_size| for |scale_factors|.
// The score is arbitrary, but it's best for exact size matches,
// and gets worse the more resampling needs to happen.
gfx::ImageSkia SelectFaviconFrames(
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<ui::ScaleFactor>& scale_factors,
    int desired_size,
    float* score);

// Takes a list of all history::FaviconBitmapIDSize for a favicon and returns
// the FaviconBitmapIDs (in |filtered_favicon_bitmap_ids|) whose bitmap data
// should be fetched in order to create an ImageSkia that's |desired_size| x
// |desired_size| DIP big.
// If |desired_size| is 0, the FaviconBitmapID with the largest size is
// returned. If score is non-NULL, it receives a score between 0 (bad) and
// 1 (good) that describes how well |filtered_favicon_bitmap_ids| will
// produce an image at |desired_size| for |scale_factors|. The score is
// arbitrary, but it's best for exact size matches, and gets worse the
// more resampling needs to happen.
// TODO(pkotwicz): Remove need to pass in |scale_factors|.
void SelectFaviconBitmapIDs(
    const std::vector<history::FaviconBitmapIDSize>& bitmap_id_sizes,
    const std::vector<ui::ScaleFactor>& scale_factors,
    int desired_size,
    std::vector<history::FaviconBitmapID>* filtered_favicon_bitmap_ids,
    float* score);

#endif  // CHROME_BROWSER_HISTORY_SELECT_FAVICON_FRAMES_H_
