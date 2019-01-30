// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_VISUAL_UTILS_H_
#define COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_VISUAL_UTILS_H_

#include "components/safe_browsing/proto/csd.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace safe_browsing {
namespace visual_utils {

using QuantizedColor = uint32_t;

// Utility methods for working with QuantizedColors.
QuantizedColor SkColorToQuantizedColor(SkColor color);
int GetQuantizedR(QuantizedColor color);
int GetQuantizedG(QuantizedColor color);
int GetQuantizedB(QuantizedColor color);

// Computes the color histogram for the image. This buckets the pixels according
// to their QuantizedColor, then reports their weight and centroid.
bool GetHistogramForImage(const SkBitmap& image,
                          VisualFeatures::ColorHistogram* histogram);

}  // namespace visual_utils
}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_VISUAL_UTILS_H_
