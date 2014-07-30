// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/bsp_walk_action.h"

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "cc/output/direct_renderer.h"
#include "cc/quads/draw_polygon.h"

namespace cc {

void BspWalkActionToVector::operator()(DrawPolygon* item) {
  list_->push_back(item);
}

BspWalkActionToVector::BspWalkActionToVector(std::vector<DrawPolygon*>* in_list)
    : list_(in_list) {
}

}  // namespace cc
