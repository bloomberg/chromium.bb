// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/session/tab_control_feature.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/size.pb.h"
#include "blimp/common/proto/tab_control.pb.h"
#include "blimp/net/test_common.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

using testing::_;

namespace blimp {

MATCHER_P3(EqualsSizeMessage, width, height, dp_to_px, "") {
  return arg.tab_control().type() == TabControlMessage::SIZE &&
         arg.tab_control().size().width() == width &&
         arg.tab_control().size().height() == height &&
         arg.tab_control().size().device_pixel_ratio() == dp_to_px;
}

class TabControlFeatureTest : public testing::Test {
 public:
  TabControlFeatureTest() : out_processor_(nullptr) {}

  void SetUp() override {
    out_processor_ = new MockBlimpMessageProcessor();
    feature_.set_outgoing_message_processor(make_scoped_ptr(out_processor_));
  }

 protected:
  // This is a raw pointer to a class that is owned by the ControlFeature.
  MockBlimpMessageProcessor* out_processor_;

  TabControlFeature feature_;
};

TEST_F(TabControlFeatureTest, CreatesCorrectSizeMessage) {
  uint64_t width = 10;
  uint64_t height = 15;
  float dp_to_px = 1.23f;

  EXPECT_CALL(
      *out_processor_,
      MockableProcessMessage(EqualsSizeMessage(width, height, dp_to_px), _))
      .Times(1);
  feature_.SetSizeAndScale(gfx::Size(width, height), 1.23f);
}

}  // namespace blimp
