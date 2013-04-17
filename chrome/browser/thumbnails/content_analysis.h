// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAILS_CONTENT_ANALYSIS_H_
#define CHROME_BROWSER_THUMBNAILS_CONTENT_ANALYSIS_H_

#include <vector>

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_export.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

class SkBitmap;

namespace thumbnailing_utils {

// Compute in-place gaussian gradient magnitude of |input_bitmap| with sigma
// |kernel_sigma|. |input_bitmap| is requried to be of SkBitmap::kA8_Config
// type. The routine computes first-order gaussian derivative on a
// gaussian-smoothed image. Beware, this is fairly slow since kernel size is
// 4 * kernel_sigma + 1.
void ApplyGaussianGradientMagnitudeFilter(SkBitmap* input_bitmap,
                                          float kernel_sigma);

// Accumulates vertical and horizontal sum of pixel values from a subsection of
// |input_bitmap| defined by |image_area|. The image is required to be of
// SkBitmap::kA8_Config type.
// If non-empty |target_size| is given, the routine will use it to process the
// profiles with closing operator sized to eliminate gaps which would be smaller
// than 1 pixel after rescaling to |target_size|.
// If |apply_log| is true, logarithm of counts are used for morhology (and
// returned).
void ExtractImageProfileInformation(const SkBitmap& input_bitmap,
                                    const gfx::Rect& image_area,
                                    const gfx::Size& target_size,
                                    bool apply_log,
                                    std::vector<float>* rows,
                                    std::vector<float>* columns);

// Compute a threshold value separating background (low) from signal (high)
// areas in the |input| profile.
float AutoSegmentPeaks(const std::vector<float>& input);

// Shrinks the source |bitmap| by removing rows and columns where |rows| and
// |columns| are false, respectively. The function returns a new bitmap if the
// shrinking can be performed and an empty instance otherwise.
SkBitmap ComputeDecimatedImage(const SkBitmap& bitmap,
                               const std::vector<bool>& rows,
                               const std::vector<bool>& columns);

// Creates a new bitmap which contains only 'interesting' areas of
// |source_bitmap|. The |target_size| is used to estimate some computation
// parameters, but the resulting bitmap will not necessarily be of that size.
// |kernel_sigma| defines the degree of image smoothing in gradient computation.
// For a natural-sized (not shrunk) screenshot at 96 DPI and regular font size
// 5.0 was determined to be a good value.
SkBitmap CreateRetargettedThumbnailImage(const SkBitmap& source_bitmap,
                                         const gfx::Size& target_size,
                                         float kernel_sigma);

}  // namespace thumbnailing_utils

#endif  // CHROME_BROWSER_THUMBNAILS_CONTENT_ANALYSIS_H_
