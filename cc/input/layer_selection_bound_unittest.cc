// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/layer_selection_bound.h"

#include "cc/proto/layer_selection_bound.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"

namespace cc {
namespace {

void VerifySerializeAndDeserializeProto(const LayerSelectionBound& bound1) {
  proto::LayerSelectionBound proto;
  bound1.ToProtobuf(&proto);
  LayerSelectionBound bound2;
  bound2.FromProtobuf(proto);
  EXPECT_EQ(bound1, bound2);
}

TEST(LayerSelectionBoundTest, AllTypePermutations) {
  LayerSelectionBound bound;
  bound.type = SelectionBoundType::SELECTION_BOUND_LEFT;
  bound.edge_top = gfx::Point(3, 14);
  bound.edge_bottom = gfx::Point(6, 28);
  bound.layer_id = 42;
  VerifySerializeAndDeserializeProto(bound);
  bound.type = SelectionBoundType::SELECTION_BOUND_RIGHT;
  VerifySerializeAndDeserializeProto(bound);
  bound.type = SelectionBoundType::SELECTION_BOUND_CENTER;
  VerifySerializeAndDeserializeProto(bound);
  bound.type = SelectionBoundType::SELECTION_BOUND_EMPTY;
  VerifySerializeAndDeserializeProto(bound);
}

}  // namespace
}  // namespace cc
