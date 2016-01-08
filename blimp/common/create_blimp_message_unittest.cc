// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/compositor.pb.h"
#include "blimp/common/proto/input.pb.h"
#include "blimp/common/proto/navigation.pb.h"
#include "blimp/common/proto/render_widget.pb.h"
#include "blimp/common/proto/tab_control.pb.h"
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

TEST(CreateBlimpMessageTest, TabControlMessage) {
  TabControlMessage* details = nullptr;
  scoped_ptr<BlimpMessage> message = CreateBlimpMessage(&details);
  EXPECT_NE(nullptr, details);
  EXPECT_NE(nullptr, message);
  EXPECT_EQ(details, message->mutable_tab_control());
}

TEST(CreateBlimpMessageTest, InputMessage) {
  InputMessage* details = nullptr;
  scoped_ptr<BlimpMessage> message = CreateBlimpMessage(&details);
  EXPECT_NE(nullptr, details);
  EXPECT_NE(nullptr, message);
  EXPECT_EQ(details, message->mutable_input());
}

TEST(CreateBlimpMessageTest, NavigationMessage) {
  NavigationMessage* details = nullptr;
  scoped_ptr<BlimpMessage> message = CreateBlimpMessage(&details, kTabId);
  EXPECT_NE(nullptr, details);
  EXPECT_NE(nullptr, message);
  EXPECT_EQ(details, message->mutable_navigation());
  EXPECT_EQ(kTabId, message->target_tab_id());
}

TEST(CreateBlimpMessageTest, RenderWidgetMessage) {
  RenderWidgetMessage* details = nullptr;
  scoped_ptr<BlimpMessage> message = CreateBlimpMessage(&details, kTabId);
  EXPECT_NE(nullptr, details);
  EXPECT_NE(nullptr, message);
  EXPECT_EQ(details, message->mutable_render_widget());
  EXPECT_EQ(kTabId, message->target_tab_id());
}

TEST(CreateBlimpMessageTest, SizeMessage) {
  SizeMessage* details = nullptr;
  scoped_ptr<BlimpMessage> message = CreateBlimpMessage(&details);
  EXPECT_NE(nullptr, details);
  EXPECT_NE(nullptr, message);
  EXPECT_EQ(TabControlMessage::SIZE, message->mutable_tab_control()->type());
  EXPECT_EQ(details, message->mutable_tab_control()->mutable_size());
}

TEST(CreateBlimpMessageTest, StartConnectionMessage) {
  const char* client_token = "token";
  const int protocol_version = 1;
  scoped_ptr<BlimpMessage> message =
      CreateStartConnectionMessage(client_token, protocol_version);
  EXPECT_NE(nullptr, message);
  EXPECT_EQ(BlimpMessage::PROTOCOL_CONTROL, message->type());
  EXPECT_EQ(ProtocolControlMessage::START_CONNECTION,
            message->protocol_control().type());
  EXPECT_EQ(client_token,
            message->protocol_control().start_connection().client_token());
  EXPECT_EQ(protocol_version,
            message->protocol_control().start_connection().protocol_version());
}

}  // namespace
}  // namespace blimp
