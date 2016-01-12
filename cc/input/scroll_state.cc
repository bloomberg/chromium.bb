// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_state.h"

#include <utility>

#include "cc/layers/layer_impl.h"

namespace cc {

ScrollState::ScrollState(double delta_x,
                         double delta_y,
                         int start_position_x,
                         int start_position_y,
                         double velocity_x,
                         double velocity_y,
                         bool is_beginning,
                         bool is_in_inertial_phase,
                         bool is_ending,
                         bool should_propagate,
                         bool delta_consumed_for_scroll_sequence,
                         bool is_direct_manipulation)
    : data_(delta_x,
            delta_y,
            start_position_x,
            start_position_y,
            velocity_x,
            velocity_y,
            is_beginning,
            is_in_inertial_phase,
            is_ending,
            should_propagate,
            delta_consumed_for_scroll_sequence,
            is_direct_manipulation) {}

ScrollState::ScrollState(ScrollStateData data) : data_(data) {}

ScrollState::~ScrollState() {}

void ScrollState::ConsumeDelta(double x, double y) {
  data_.delta_x -= x;
  data_.delta_y -= y;

  if (x || y)
    data_.delta_consumed_for_scroll_sequence = true;
}

void ScrollState::DistributeToScrollChainDescendant() {
  if (!scroll_chain_.empty()) {
    LayerImpl* next = scroll_chain_.front();
    scroll_chain_.pop_front();
    next->DistributeScroll(this);
  }
}

}  // namespace cc
