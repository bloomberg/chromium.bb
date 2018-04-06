// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/harmony/material_refresh_layout_provider.h"

int MaterialRefreshLayoutProvider::GetCornerRadiusMetric(
    ChromeEmphasisMetric emphasis_metric,
    const gfx::Rect& bounds) const {
  switch (emphasis_metric) {
    case EMPHASIS_LOW:
      return 4;
    case EMPHASIS_MEDIUM:
      return 8;
    case EMPHASIS_HIGH:
      return std::min(bounds.width(), bounds.height()) / 2;
    default:
      NOTREACHED();
      return 0;
  }
}
