// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/find_bar/find_match_rects_details.h"

FindMatchRectsDetails::FindMatchRectsDetails()
    : version_(-1) {
}

FindMatchRectsDetails::FindMatchRectsDetails(
    int version,
    const std::vector<gfx::RectF>& rects,
    const gfx::RectF& active_rect)
    : version_(version),
      rects_(rects),
      active_rect_(active_rect) {
}

FindMatchRectsDetails::~FindMatchRectsDetails() {
}
