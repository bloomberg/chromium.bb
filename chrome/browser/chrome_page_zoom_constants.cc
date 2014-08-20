// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_page_zoom_constants.h"

namespace chrome_page_zoom {

// This list is duplicated in chrome/browser/resources/pdf/viewport.js. Please
// make sure the two match.
const double kPresetZoomFactors[] = { 0.25, 0.333, 0.5, 0.666, 0.75, 0.9, 1.0,
                                      1.1, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0, 4.0,
                                      5.0 };
const size_t kPresetZoomFactorsSize = arraysize(kPresetZoomFactors);

}  // namespace chrome_page_zoom
