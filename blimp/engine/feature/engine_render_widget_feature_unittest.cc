// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/compositor.pb.h"
#include "blimp/common/proto/render_widget.pb.h"
#include "blimp/engine/feature/engine_render_widget_feature.h"
#include "blimp/net/input_message_generator.h"
#include "blimp/net/test_common.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using testing::_;
using testing::InSequence;
using testing::Sequence;

namespace blimp {

namespace {

class MockHostRenderWidgetMessageDelegate
    : public EngineRenderWidgetFeature::RenderWidgetMessageDelegate {
 public:
  // EngineRenderWidgetFeature implementation.
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

  net::TestCompletionCallback cb;
  processor->ProcessMessage(std::move(message), cb.callback());
  EXPECT_EQ(net::OK, cb.WaitForResult());
}

void SendCompositorMessage(BlimpMessageProcessor* processor,
                             int tab_id,
                             uint32_t rw_id,
                             const std::vector<uint8_t>& payload) {
  CompositorMessage* details;
  scoped_ptr<BlimpMessage> message = CreateBlimpMessage(&details, tab_id);
  details->set_render_widget_id(rw_id);
  details->set_payload(payload.data(), base::checked_cast<int>(payload.size()));
  net::TestCompletionCallback cb;
  processor->ProcessMessage(std::move(message), cb.callback());
  EXPECT_EQ(net::OK, cb.WaitForResult());
}

}  // namespace

class EngineRenderWidgetFeatureTest : public testing::Test {
 public:
  EngineRenderWidgetFeatureTest() {}

  void SetUp() override {
    render_widget_message_sender_ = new MockBlimpMessageProcessor;
    feature_.set_render_widget_message_sender(
        make_scoped_ptr(render_widget_message_sender_));
    compositor_message_sender_ = new MockBlimpMessageProcessor;
    feature_.set_compositor_message_sender(
        make_scoped_ptr(compositor_message_sender_));
    feature_.SetDelegate(1, &delegate1_);
    feature_.SetDelegate(2, &delegate2_);
  }

 protected:
  MockBlimpMessageProcessor* render_widget_message_sender_;
  MockBlimpMessageProcessor* compositor_message_sender_;
  MockHostRenderWidgetMessageDelegate delegate1_;
  MockHostRenderWidgetMessageDelegate delegate2_;
  EngineRenderWidgetFeature feature_;
};

TEST_F(EngineRenderWidgetFeatureTest, DelegateCallsOK) {
  std::vector<uint8_t> payload = { 'd', 'a', 'v', 'i', 'd' };

  EXPECT_CALL(*render_widget_message_sender_, MockableProcessMessage(_, _))
      .Times(2);
  EXPECT_CALL(delegate1_,
              MockableOnCompositorMessageReceived(CompMsgEquals(payload)))
      .Times(1);
  EXPECT_CALL(delegate1_, MockableOnWebInputEvent()).Times(1);
  EXPECT_CALL(delegate2_,
              MockableOnCompositorMessageReceived(CompMsgEquals(payload)))
      .Times(1);
  EXPECT_CALL(delegate2_, MockableOnWebInputEvent()).Times(0);

  feature_.OnRenderWidgetInitialized(1);
  feature_.OnRenderWidgetInitialized(2);
  SendCompositorMessage(&feature_, 1, 1U, payload);
  SendInputMessage(&feature_, 1, 1U);
  SendCompositorMessage(&feature_, 2, 1U, payload);
}

TEST_F(EngineRenderWidgetFeatureTest, DropsStaleMessages) {
  InSequence sequence;
  std::vector<uint8_t> payload = { 'f', 'u', 'n' };
  std::vector<uint8_t> new_payload = {'n', 'o', ' ', 'f', 'u', 'n'};

  EXPECT_CALL(*render_widget_message_sender_,
              MockableProcessMessage(BlimpRWMsgEquals(1, 1U), _));
  EXPECT_CALL(delegate1_,
              MockableOnCompositorMessageReceived(CompMsgEquals(payload)));
  EXPECT_CALL(*render_widget_message_sender_,
              MockableProcessMessage(BlimpRWMsgEquals(1, 2U), _));
  EXPECT_CALL(delegate1_,
              MockableOnCompositorMessageReceived(CompMsgEquals(new_payload)));
  EXPECT_CALL(delegate1_, MockableOnWebInputEvent());

  feature_.OnRenderWidgetInitialized(1);
  SendCompositorMessage(&feature_, 1, 1U, payload);
  feature_.OnRenderWidgetInitialized(1);

  // These next three calls should be dropped.
  SendCompositorMessage(&feature_, 1, 1U, payload);
  SendCompositorMessage(&feature_, 1, 1U, payload);
  SendInputMessage(&feature_, 1, 1U);

  SendCompositorMessage(&feature_, 1, 2U, new_payload);
  SendInputMessage(&feature_, 1, 2U);
}

TEST_F(EngineRenderWidgetFeatureTest, RepliesHaveCorrectRenderWidgetId) {
  Sequence delegate1_sequence;
  Sequence delegate2_sequence;
  std::vector<uint8_t> payload = { 'a', 'b', 'c', 'd' };

  EXPECT_CALL(*render_widget_message_sender_,
              MockableProcessMessage(BlimpRWMsgEquals(1, 1U), _))
      .InSequence(delegate1_sequence);
  EXPECT_CALL(*render_widget_message_sender_,
              MockableProcessMessage(BlimpRWMsgEquals(1, 2U), _))
      .InSequence(delegate1_sequence);
  EXPECT_CALL(*compositor_message_sender_,
              MockableProcessMessage(BlimpCompMsgEquals(1, 2U, payload), _))
      .InSequence(delegate1_sequence);
  EXPECT_CALL(*render_widget_message_sender_,
              MockableProcessMessage(BlimpRWMsgEquals(2, 1U), _))
      .InSequence(delegate2_sequence);
  EXPECT_CALL(*render_widget_message_sender_,
              MockableProcessMessage(BlimpRWMsgEquals(2, 2U), _))
      .InSequence(delegate2_sequence);

  feature_.OnRenderWidgetInitialized(1);
  feature_.OnRenderWidgetInitialized(2);
  feature_.OnRenderWidgetInitialized(1);
  feature_.OnRenderWidgetInitialized(2);
  feature_.SendCompositorMessage(1, payload);
}


}  // namespace blimp
