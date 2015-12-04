// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/compositor.pb.h"
#include "blimp/common/proto/input.pb.h"
#include "blimp/common/proto/render_widget.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace {

const int kTabId = 1234;

TEST(CreateBlimpMessageTest, CompositorMessage) {
  CompositorMessage* details = nullptr;
  scoped_ptr<BlimpMessage> message = CreateBlimpMessage(&details, kTabId);
  EXPECT_NE(nullptr, details);
  EXPECT_NE(nullptr, message);
  EXPECT_EQ(details, message->mutable_compositor());
  EXPECT_EQ(kTabId, message->target_tab_id());
}

TEST(CreateBlimpMessageTest, InputMessage) {
  InputMessage* details = nullptr;
  scoped_ptr<BlimpMessage> message = CreateBlimpMessage(&details);
  EXPECT_NE(nullptr, details);
  EXPECT_NE(nullptr, message);
  EXPECT_EQ(details, message->mutable_input());
}

TEST(CreateBlimpMessageTest, RenderWidgetMessage) {
  RenderWidgetMessage* details = nullptr;
  scoped_ptr<BlimpMessage> message = CreateBlimpMessage(&details, kTabId);
  EXPECT_NE(nullptr, details);
  EXPECT_NE(nullptr, message);
  EXPECT_EQ(details, message->mutable_render_widget());
  EXPECT_EQ(kTabId, message->target_tab_id());
}

}  // namespace
}  // namespace blimp
