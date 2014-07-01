// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/viewport_selection_bound.h"

namespace cc {

ViewportSelectionBound::ViewportSelectionBound()
    : type(SELECTION_BOUND_EMPTY), visible(false) {
}

ViewportSelectionBound::~ViewportSelectionBound() {
}

bool operator==(const ViewportSelectionBound& lhs,
                const ViewportSelectionBound& rhs) {
  return lhs.type == rhs.type && lhs.visible == rhs.visible &&
         lhs.viewport_rect == rhs.viewport_rect;
}

bool operator!=(const ViewportSelectionBound& lhs,
                const ViewportSelectionBound& rhs) {
  return !(lhs == rhs);
}

}  // namespace cc
