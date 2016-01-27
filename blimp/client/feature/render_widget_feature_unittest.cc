// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/render_widget_feature.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/compositor.pb.h"
#include "blimp/common/proto/render_widget.pb.h"
#include "blimp/net/test_common.h"
#include "cc/proto/compositor_message.pb.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;
using testing::Sequence;

namespace blimp {
namespace client {

namespace {

class MockRenderWidgetFeatureDelegate
    : public RenderWidgetFeature::RenderWidgetFeatureDelegate {
 public:
  // RenderWidgetFeatureDelegate implementation.
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
  RenderWidgetMessage* details;
  scoped_ptr<BlimpMessage> message = CreateBlimpMessage(&details, tab_id);
  details->set_type(RenderWidgetMessage::INITIALIZE);
  details->set_render_widget_id(rw_id);
  net::TestCompletionCallback cb;
  processor->ProcessMessage(std::move(message), cb.callback());
  EXPECT_EQ(net::OK, cb.WaitForResult());
}

void SendCompositorMessage(BlimpMessageProcessor* processor,
                           int tab_id,
                           uint32_t rw_id) {
  CompositorMessage* details;
  scoped_ptr<BlimpMessage> message = CreateBlimpMessage(&details, tab_id);
  details->set_render_widget_id(rw_id);
  net::TestCompletionCallback cb;
  processor->ProcessMessage(std::move(message), cb.callback());
  EXPECT_EQ(net::OK, cb.WaitForResult());
}

}  // namespace

class RenderWidgetFeatureTest : public testing::Test {
 public:
  RenderWidgetFeatureTest()
      : out_input_processor_(nullptr), out_compositor_processor_(nullptr) {}

  void SetUp() override {
    out_input_processor_ = new MockBlimpMessageProcessor();
    out_compositor_processor_ = new MockBlimpMessageProcessor();
    feature_.set_outgoing_input_message_processor(
        make_scoped_ptr(out_input_processor_));
    feature_.set_outgoing_compositor_message_processor(
        make_scoped_ptr(out_compositor_processor_));

    feature_.SetDelegate(1, &delegate1_);
    feature_.SetDelegate(2, &delegate2_);
  }

 protected:
  // These are raw pointers to classes that are owned by the
  // RenderWidgetFeature.
  MockBlimpMessageProcessor* out_input_processor_;
  MockBlimpMessageProcessor* out_compositor_processor_;

  MockRenderWidgetFeatureDelegate delegate1_;
  MockRenderWidgetFeatureDelegate delegate2_;

  RenderWidgetFeature feature_;
};

TEST_F(RenderWidgetFeatureTest, DelegateCallsOK) {
  EXPECT_CALL(delegate1_, MockableOnRenderWidgetInitialized()).Times(1);
  EXPECT_CALL(delegate1_, MockableOnCompositorMessageReceived(_)).Times(2);
  EXPECT_CALL(delegate2_, MockableOnRenderWidgetInitialized()).Times(1);
  EXPECT_CALL(delegate2_, MockableOnCompositorMessageReceived(_)).Times(0);

  SendRenderWidgetMessage(&feature_, 1, 1U);
  SendCompositorMessage(&feature_, 1, 1U);
  SendCompositorMessage(&feature_, 1, 1U);
  SendRenderWidgetMessage(&feature_, 2, 2U);
}

TEST_F(RenderWidgetFeatureTest, RepliesHaveCorrectRenderWidgetId) {
  InSequence sequence;

  EXPECT_CALL(*out_compositor_processor_,
              MockableProcessMessage(CompMsgEquals(1, 2U), _));
  EXPECT_CALL(*out_compositor_processor_,
              MockableProcessMessage(CompMsgEquals(1, 3U), _));
  EXPECT_CALL(*out_compositor_processor_,
              MockableProcessMessage(CompMsgEquals(2, 1U), _));

  SendRenderWidgetMessage(&feature_, 1, 2U);
  feature_.SendCompositorMessage(1, cc::proto::CompositorMessage());
  SendRenderWidgetMessage(&feature_, 1, 3U);
  feature_.SendCompositorMessage(1, cc::proto::CompositorMessage());
  SendRenderWidgetMessage(&feature_, 2, 1U);
  feature_.SendCompositorMessage(2, cc::proto::CompositorMessage());
}

}  // namespace client
}  // namespace blimp
