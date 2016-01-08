// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_LAYER_TREE_MUTATION_H_
#define CC_ANIMATION_LAYER_TREE_MUTATION_H_

#include "base/containers/hash_tables.h"
#include "cc/animation/mutable_properties.h"
#include "third_party/skia/include/utils/SkMatrix44.h"

namespace cc {

class LayerTreeMutation {
 public:
  void SetOpacity(float opacity) {
    mutated_flags_ |= kMutablePropertyOpacity;
    opacity_ = opacity;
  }
  void SetScrollLeft(float scroll_left) {
    mutated_flags_ |= kMutablePropertyScrollLeft;
    scroll_left_ = scroll_left;
  }
  void SetScrollTop(float scroll_top) {
    mutated_flags_ |= kMutablePropertyScrollTop;
    scroll_top_ = scroll_top;
  }
  void SetTransform(const SkMatrix44& transform) {
    mutated_flags_ |= kMutablePropertyTransform;
    transform_ = transform;
  }

  bool is_opacity_mutated() const {
    return !!(mutated_flags_ & kMutablePropertyOpacity);
  }
  bool is_scroll_left_mutated() const {
    return !!(mutated_flags_ & kMutablePropertyScrollLeft);
  }
  bool is_scroll_top_mutated() const {
    return !!(mutated_flags_ & kMutablePropertyScrollTop);
  }
  bool is_transform_mutated() const {
    return !!(mutated_flags_ & kMutablePropertyTransform);
  }

  float opacity() const { return opacity_; }
  float scroll_left() const { return scroll_left_; }
  float scroll_top() const { return scroll_top_; }
  SkMatrix44 transform() const { return transform_; }

 private:
  uint32_t mutated_flags_ = 0;
  float opacity_ = 0;
  float scroll_left_ = 0;
  float scroll_top_ = 0;
  SkMatrix44 transform_;
};

typedef base::hash_map<uint64_t, LayerTreeMutation> LayerTreeMutationMap;

}  // namespace cc

#endif  // CC_ANIMATION_LAYER_TREE_MUTATION_H_
