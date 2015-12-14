// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/browser/engine_render_widget_message_processor.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/compositor.pb.h"
#include "blimp/common/proto/render_widget.pb.h"
#include "blimp/net/input_message_generator.h"
#include "blimp/net/test_common.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using testing::_;
using testing::InSequence;
using testing::Sequence;

namespace blimp {

namespace {

class MockHostRenderWidgetMessageDelegate
    : public EngineRenderWidgetMessageProcessor::RenderWidgetMessageDelegate {
 public:
  // EngineRenderWidgetMessageProcessor implementation.
  void OnWebInputEvent(scoped_ptr<blink::WebInputEvent> event) override {
    MockableOnWebInputEvent();
  }

  void OnCompositorMessageReceived(
      const std::vector<uint8_t>& message) override {
    MockableOnCompositorMessageReceived(message);
  }

  MOCK_METHOD0(MockableOnWebInputEvent, void());
  MOCK_METHOD1(MockableOnCompositorMessageReceived,
               void(const std::vector<uint8_t>& message));
};

MATCHER_P(CompMsgEquals, contents, "") {
  if (contents.size() != arg.size())
      return false;

  return memcmp(contents.data(), arg.data(), contents.size()) == 0;
}

MATCHER_P3(BlimpCompMsgEquals, tab_id, rw_id, contents, "") {
  if (contents.size() != arg.compositor().payload().size())
    return false;

  if (memcmp(contents.data(),
             arg.compositor().payload().data(),
             contents.size()) != 0) {
    return false;
  }

  return arg.compositor().render_widget_id() == rw_id &&
      arg.target_tab_id() == tab_id;
}

MATCHER_P2(BlimpRWMsgEquals, tab_id, rw_id, "") {
  return arg.render_widget().render_widget_id() == rw_id &&
      arg.target_tab_id() == tab_id;
}

void SendInputMessage(BlimpMessageProcessor* processor,
                             int tab_id,
                             uint32_t rw_id) {
  blink::WebGestureEvent input_event;
  input_event.type = blink::WebInputEvent::Type::GestureTap;

  InputMessageGenerator generator;
  scoped_ptr<BlimpMessage> message = generator.GenerateMessage(input_event);
  message->set_type(BlimpMessage::INPUT);
  message->set_target_tab_id(tab_id);
  message->mutable_input()->set_render_widget_id(rw_id);

  processor->ProcessMessage(std::move(message),
                            net::CompletionCallback());
}

void SendCompositorMessage(BlimpMessageProcessor* processor,
                             int tab_id,
                             uint32_t rw_id,
                             const std::vector<uint8_t>& payload) {
  CompositorMessage* details;
  scoped_ptr<BlimpMessage> message = CreateBlimpMessage(&details, tab_id);
  details->set_render_widget_id(rw_id);
  details->set_payload(payload.data(), base::checked_cast<int>(payload.size()));
  processor->ProcessMessage(std::move(message),
                            net::CompletionCallback());
}

}  // namespace

class EngineRenderWidgetMessageProcessorTest : public testing::Test {
 public:
  EngineRenderWidgetMessageProcessorTest()
      : processor_(&out_processor_, &out_processor_) {}

  void SetUp() override {
    processor_.SetDelegate(1, &delegate1_);
    processor_.SetDelegate(2, &delegate2_);
  }

 protected:
  MockBlimpMessageProcessor out_processor_;
  MockHostRenderWidgetMessageDelegate delegate1_;
  MockHostRenderWidgetMessageDelegate delegate2_;
  EngineRenderWidgetMessageProcessor processor_;
};

TEST_F(EngineRenderWidgetMessageProcessorTest, DelegateCallsOK) {
  std::vector<uint8_t> payload = { 'd', 'a', 'v', 'i', 'd' };

  EXPECT_CALL(out_processor_, MockableProcessMessage(_, _)).Times(2);
  EXPECT_CALL(delegate1_,
              MockableOnCompositorMessageReceived(CompMsgEquals(payload)))
      .Times(1);
  EXPECT_CALL(delegate1_, MockableOnWebInputEvent()).Times(1);
  EXPECT_CALL(delegate2_,
              MockableOnCompositorMessageReceived(CompMsgEquals(payload)))
      .Times(1);
  EXPECT_CALL(delegate2_, MockableOnWebInputEvent()).Times(0);

  processor_.OnRenderWidgetInitialized(1);
  processor_.OnRenderWidgetInitialized(2);
  SendCompositorMessage(&processor_, 1, 1U, payload);
  SendInputMessage(&processor_, 1, 1U);
  SendCompositorMessage(&processor_, 2, 1U, payload);
}

TEST_F(EngineRenderWidgetMessageProcessorTest, DropsStaleMessages) {
  InSequence sequence;
  std::vector<uint8_t> payload = { 'f', 'u', 'n' };
  std::vector<uint8_t> new_payload = {'n', 'o', ' ', 'f', 'u', 'n'};

  EXPECT_CALL(out_processor_,
              MockableProcessMessage(BlimpRWMsgEquals(1, 1U), _));
  EXPECT_CALL(delegate1_,
              MockableOnCompositorMessageReceived(CompMsgEquals(payload)));
  EXPECT_CALL(out_processor_,
              MockableProcessMessage(BlimpRWMsgEquals(1, 2U), _));
  EXPECT_CALL(delegate1_,
              MockableOnCompositorMessageReceived(CompMsgEquals(new_payload)));
  EXPECT_CALL(delegate1_, MockableOnWebInputEvent());

  processor_.OnRenderWidgetInitialized(1);
  SendCompositorMessage(&processor_, 1, 1U, payload);
  processor_.OnRenderWidgetInitialized(1);

  // These next three calls should be dropped.
  SendCompositorMessage(&processor_, 1, 1U, payload);
  SendCompositorMessage(&processor_, 1, 1U, payload);
  SendInputMessage(&processor_, 1, 1U);

  SendCompositorMessage(&processor_, 1, 2U, new_payload);
  SendInputMessage(&processor_, 1, 2U);
}

TEST_F(EngineRenderWidgetMessageProcessorTest,
       RepliesHaveCorrectRenderWidgetId) {
  Sequence delegate1_sequence;
  Sequence delegate2_sequence;
  std::vector<uint8_t> payload = { 'a', 'b', 'c', 'd' };

  EXPECT_CALL(out_processor_,
              MockableProcessMessage(BlimpRWMsgEquals(1, 1U), _))
      .InSequence(delegate1_sequence);
  EXPECT_CALL(out_processor_,
              MockableProcessMessage(BlimpRWMsgEquals(1, 2U), _))
      .InSequence(delegate1_sequence);
  EXPECT_CALL(out_processor_,
              MockableProcessMessage(BlimpCompMsgEquals(1, 2U, payload), _))
      .InSequence(delegate1_sequence);
  EXPECT_CALL(out_processor_,
              MockableProcessMessage(BlimpRWMsgEquals(2, 1U), _))
      .InSequence(delegate2_sequence);
  EXPECT_CALL(out_processor_,
              MockableProcessMessage(BlimpRWMsgEquals(2, 2U), _))
      .InSequence(delegate2_sequence);

  processor_.OnRenderWidgetInitialized(1);
  processor_.OnRenderWidgetInitialized(2);
  processor_.OnRenderWidgetInitialized(1);
  processor_.OnRenderWidgetInitialized(2);
  processor_.SendCompositorMessage(1, payload);
}


}  // namespace blimp
