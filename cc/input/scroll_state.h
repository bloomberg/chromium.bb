// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLL_STATE_H_
#define CC_INPUT_SCROLL_STATE_H_

#include <list>

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/input/scroll_state_data.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"

namespace cc {

class LayerImpl;

// ScrollState is based on the proposal for scroll customization in blink, found
// here: https://goo.gl/1ipTpP.
class CC_EXPORT ScrollState {
 public:
  static scoped_ptr<ScrollState> Create(const gfx::Vector2dF& scroll_delta,
                                        const gfx::Point& viewport_point,
                                        const gfx::Vector2dF& scroll_velocity,
                                        bool is_beginning,
                                        bool is_in_inertial_phase,
                                        bool is_ending) {
    return make_scoped_ptr(new ScrollState(
        scroll_delta.x(), scroll_delta.y(), viewport_point.x(),
        viewport_point.y(), scroll_velocity.x(), scroll_velocity.y(),
        is_beginning, is_in_inertial_phase, is_ending));
  }

  ScrollState(double delta_x,
              double delta_y,
              int start_position_x,
              int start_position_y,
              double velocity_x,
              double velocity_y,
              bool is_beginning,
              bool is_in_inertial_phase,
              bool is_ending,
              bool should_propagate = false,
              bool delta_consumed_for_scroll_sequence = false,
              bool is_direct_manipulation = false);
  explicit ScrollState(ScrollStateData data);
  ~ScrollState();

  // Reduce deltas by x, y.
  void ConsumeDelta(double x, double y);
  // Pops the first layer off of |scroll_chain_| and calls
  // |DistributeScroll| on it.
  void DistributeToScrollChainDescendant();
  // Positive when scrolling left.
  double delta_x() const { return data_.delta_x; }
  // Positive when scrolling up.
  double delta_y() const { return data_.delta_y; }
  // The location the scroll started at. For touch, the starting
  // position of the finger. For mouse, the location of the cursor.
  int start_position_x() const { return data_.start_position_x; }
  int start_position_y() const { return data_.start_position_y; }

  double velocity_x() const { return data_.velocity_x; }
  double velocity_y() const { return data_.velocity_y; }

  bool is_beginning() const { return data_.is_beginning; }
  void set_is_beginning(bool is_beginning) {
    data_.is_beginning = is_beginning;
  }
  bool is_in_inertial_phase() const { return data_.is_in_inertial_phase; }
  void set_is_in_inertial_phase(bool is_in_inertial_phase) {
    data_.is_in_inertial_phase = is_in_inertial_phase;
  }
  bool is_ending() const { return data_.is_ending; }
  void set_is_ending(bool is_ending) { data_.is_ending = is_ending; }

  // True if this scroll is allowed to bubble upwards.
  bool should_propagate() const { return data_.should_propagate; }
  // True if the user interacts directly with the screen, e.g., via touch.
  bool is_direct_manipulation() const { return data_.is_direct_manipulation; }
  void set_is_direct_manipulation(bool is_direct_manipulation) {
    data_.is_direct_manipulation = is_direct_manipulation;
  }

  void set_scroll_chain(const std::list<LayerImpl*>& scroll_chain) {
    scroll_chain_ = scroll_chain;
  }

  void set_current_native_scrolling_layer(LayerImpl* layer) {
    data_.current_native_scrolling_layer = layer;
  }

  LayerImpl* current_native_scrolling_layer() const {
    return data_.current_native_scrolling_layer;
  }

  bool delta_consumed_for_scroll_sequence() const {
    return data_.delta_consumed_for_scroll_sequence;
  }
  void set_delta_consumed_for_scroll_sequence(bool delta_consumed) {
    data_.delta_consumed_for_scroll_sequence = delta_consumed;
  }

  bool FullyConsumed() const { return !data_.delta_x && !data_.delta_y; }

  void set_caused_scroll(bool x, bool y) {
    data_.caused_scroll_x |= x;
    data_.caused_scroll_y |= y;
  }

  bool caused_scroll_x() const { return data_.caused_scroll_x; }
  bool caused_scroll_y() const { return data_.caused_scroll_y; }

  ScrollStateData* data() { return &data_; }

 private:
  ScrollStateData data_;
  std::list<LayerImpl*> scroll_chain_;
};

}  // namespace cc

#endif  // CC_INPUT_SCROLL_STATE_H_
