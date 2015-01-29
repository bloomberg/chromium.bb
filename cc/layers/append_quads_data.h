// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_APPEND_QUADS_DATA_H_
#define CC_LAYERS_APPEND_QUADS_DATA_H_

#include "base/basictypes.h"
#include "cc/quads/render_pass_id.h"

namespace cc {

struct AppendQuadsData {
  AppendQuadsData()
      : num_incomplete_tiles(0),
        num_missing_tiles(0),
        visible_content_area(0),
        approximated_visible_content_area(0) {}

  // Set by the layer appending quads.
  int64 num_incomplete_tiles;
  // Set by the layer appending quads.
  int64 num_missing_tiles;
  // Set by the layer appending quads.
  int64 visible_content_area;
  // Set by the layer appending quads.
  int64 approximated_visible_content_area;
};

}  // namespace cc
#endif  // CC_LAYERS_APPEND_QUADS_DATA_H_
