// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/compositor/render_widget_message_processor.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/compositor.pb.h"
#include "blimp/common/proto/render_widget.pb.h"
#include "blimp/net/test_common.h"
#include "cc/proto/compositor_message.pb.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InvokeArgument;
using testing::Ref;
using testing::Return;
using testing::SaveArg;

namespace blimp {

namespace {

class MockRenderWidgetMessageDelegate
    : public RenderWidgetMessageProcessor::RenderWidgetMessageDelegate {
 public:
  // RenderWidgetMessageDelegate implementation.
  void OnRenderWidgetInitialized() override {
    MockableOnRenderWidgetInitialized();
  }
  void OnCompositorMessageReceived(
      scoped_ptr<cc::proto::CompositorMessage> message) override {
    MockableOnCompositorMessageReceived(*message);
  }

  MOCK_METHOD0(MockableOnRenderWidgetInitialized, void());
  MOCK_METHOD1(MockableOnCompositorMessageReceived,
               void(const cc::proto::CompositorMessage& message));
};

MATCHER_P2(CompMsgEquals, tab_id, rw_id, "") {
  return arg.compositor().render_widget_id() == rw_id &&
      arg.target_tab_id() == tab_id;
}

void SendRenderWidgetMessage(BlimpMessageProcessor* processor,
                             int tab_id,
                             uint32_t rw_id) {
  scoped_ptr<BlimpMessage> message(new BlimpMessage);
  message->set_type(BlimpMessage::RENDER_WIDGET);
  message->set_target_tab_id(tab_id);

  RenderWidgetMessage* details = message->mutable_render_widget();
  details->set_type(RenderWidgetMessage::INITIALIZE);
  details->set_render_widget_id(rw_id);
  processor->ProcessMessage(std::move(message),
                            net::CompletionCallback());
}

void SendCompositorMessage(BlimpMessageProcessor* processor,
                             int tab_id,
                             uint32_t rw_id) {
  scoped_ptr<BlimpMessage> message(new BlimpMessage);
  message->set_type(BlimpMessage::COMPOSITOR);
  message->set_target_tab_id(tab_id);

  CompositorMessage* details = message->mutable_compositor();
  details->set_render_widget_id(rw_id);
  processor->ProcessMessage(std::move(message),
                            net::CompletionCallback());
}

}  // namespace

class RenderWidgetMessageProcessorTest : public testing::Test {
 public:
  RenderWidgetMessageProcessorTest()
      : processor_(&out_processor_, &out_processor_) {}

  void SetUp() override {
    processor_.SetDelegate(1, &delegate1_);
    processor_.SetDelegate(2, &delegate2_);
  }

 protected:
  MockBlimpMessageProcessor out_processor_;
  MockRenderWidgetMessageDelegate delegate1_;
  MockRenderWidgetMessageDelegate delegate2_;

  RenderWidgetMessageProcessor processor_;
};

TEST_F(RenderWidgetMessageProcessorTest, DelegateCallsOK) {
  EXPECT_CALL(delegate1_, MockableOnRenderWidgetInitialized()).Times(1);
  SendRenderWidgetMessage(&processor_, 1, 1U);

  EXPECT_CALL(delegate1_, MockableOnCompositorMessageReceived(_)).Times(1);
  SendCompositorMessage(&processor_, 1, 1U);

  EXPECT_CALL(delegate2_, MockableOnRenderWidgetInitialized()).Times(1);
  SendRenderWidgetMessage(&processor_, 2, 2U);

  EXPECT_CALL(delegate2_, MockableOnCompositorMessageReceived(_)).Times(1);
  SendCompositorMessage(&processor_, 2, 2U);
}

TEST_F(RenderWidgetMessageProcessorTest, RepliesHaveCorrectRenderWidgetId) {
  SendRenderWidgetMessage(&processor_, 1, 2U);
  SendRenderWidgetMessage(&processor_, 2, 1U);

  EXPECT_CALL(out_processor_,
              MockableProcessMessage(CompMsgEquals(1, 2U), _)).Times(1);
  processor_.SendCompositorMessage(1, cc::proto::CompositorMessage());

  SendRenderWidgetMessage(&processor_, 1, 3U);

  EXPECT_CALL(out_processor_,
              MockableProcessMessage(CompMsgEquals(1, 3U), _)).Times(1);
  processor_.SendCompositorMessage(1, cc::proto::CompositorMessage());

  EXPECT_CALL(out_processor_,
              MockableProcessMessage(CompMsgEquals(2, 1U), _)).Times(1);
  processor_.SendCompositorMessage(2, cc::proto::CompositorMessage());
}

}  // namespace blimp
