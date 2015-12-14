// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLL_STATE_DATA_H_
#define CC_INPUT_SCROLL_STATE_DATA_H_

#include <list>

#include "cc/base/cc_export.h"

namespace cc {

class LayerImpl;

struct CC_EXPORT ScrollStateData {
 public:
  ScrollStateData();
  ScrollStateData(double delta_x,
                  double delta_y,
                  int start_position_x,
                  int start_position_y,
                  bool should_propagate,
                  bool delta_consumed_for_scroll_sequence,
                  bool is_direct_manipulation);
  double delta_x;
  double delta_y;
  double start_position_x;
  double start_position_y;

  bool should_propagate;

  // The last layer to respond to a scroll, or null if none exists.
  LayerImpl* current_native_scrolling_layer;
  // Whether the scroll sequence has had any delta consumed, in the
  // current frame, or any child frames.
  bool delta_consumed_for_scroll_sequence;
  // True if the user interacts directly with the display, e.g., via
  // touch.
  bool is_direct_manipulation;
  // TODO(tdresser): ScrollState shouldn't need to keep track of whether or not
  // this ScrollState object has caused a scroll. Ideally, any native scroller
  // consuming delta has caused a scroll. Currently, there are some cases where
  // we consume delta without scrolling, such as in
  // |Viewport::AdjustOverscroll|. Once these cases are fixed, we should get rid
  // of |caused_scroll_*_|. See crbug.com/510045 for details.
  bool caused_scroll_x;
  bool caused_scroll_y;
};

}  // namespace cc

#endif  // CC_INPUT_SCROLL_STATE_DATA_H_
