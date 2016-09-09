// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/compositor/blimp_compositor.h"

#include "base/threading/thread_task_runner_handle.h"
#include "blimp/client/core/compositor/blimp_compositor_dependencies.h"
#include "blimp/client/core/compositor/blob_image_serialization_processor.h"
#include "blimp/client/support/compositor/mock_compositor_dependencies.h"
#include "cc/layers/layer.h"
#include "cc/proto/compositor_message.pb.h"
#include "cc/surfaces/surface_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace blimp {
namespace client {

class MockBlimpCompositorClient : public BlimpCompositorClient {
 public:
  MockBlimpCompositorClient() = default;
  ~MockBlimpCompositorClient() override = default;

  void SendWebGestureEvent(
      int render_widget_id,
      const blink::WebGestureEvent& gesture_event) override {
    MockableSendWebGestureEvent(render_widget_id);
  }
  void SendCompositorMessage(
      int render_widget_id,
      const cc::proto::CompositorMessage& message) override {
    MockableSendCompositorMessage(render_widget_id);
  }

  MOCK_METHOD1(MockableSendWebGestureEvent, void(int));
  MOCK_METHOD1(MockableSendCompositorMessage, void(int));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBlimpCompositorClient);
};

class BlimpCompositorForTesting : public BlimpCompositor {
 public:
  BlimpCompositorForTesting(
      int render_widget_id,
      BlimpCompositorDependencies* compositor_dependencies,
      BlimpCompositorClient* client)
      : BlimpCompositor(render_widget_id, compositor_dependencies, client) {}

  void SendProto(const cc::proto::CompositorMessage& proto) {
    SendCompositorProto(proto);
  }

  void SendGestureEvent(const blink::WebGestureEvent& gesture_event) {
    SendWebGestureEvent(gesture_event);
  }

  cc::LayerTreeHostInterface* host() const { return host_.get(); }
};

class BlimpCompositorTest : public testing::Test {
 public:
  BlimpCompositorTest() : render_widget_id_(1), loop_(new base::MessageLoop) {}

  void SetUp() override {
    compositor_dependencies_ = base::MakeUnique<BlimpCompositorDependencies>(
        base::MakeUnique<MockCompositorDependencies>());

    compositor_ = base::MakeUnique<BlimpCompositorForTesting>(
        render_widget_id_, compositor_dependencies_.get(), &compositor_client_);
  }

  void TearDown() override {
    compositor_.reset();
    compositor_dependencies_.reset();
  }

  ~BlimpCompositorTest() override {}

  int render_widget_id_;
  std::unique_ptr<base::MessageLoop> loop_;
  MockBlimpCompositorClient compositor_client_;
  std::unique_ptr<BlimpCompositorDependencies> compositor_dependencies_;
  std::unique_ptr<BlimpCompositorForTesting> compositor_;
  BlobImageSerializationProcessor blob_image_serialization_processor_;
};

TEST_F(BlimpCompositorTest, ToggleVisibilityWithHost) {
  compositor_->SetVisible(true);

  // Check that the visibility is set correctly on the host.
  EXPECT_TRUE(compositor_->host()->IsVisible());

  // Make the compositor invisible. This should make the |host_| invisible.
  compositor_->SetVisible(false);
  EXPECT_FALSE(compositor_->host()->IsVisible());
}

TEST_F(BlimpCompositorTest, MessagesHaveCorrectId) {
  EXPECT_CALL(compositor_client_,
              MockableSendCompositorMessage(render_widget_id_))
      .Times(1);
  EXPECT_CALL(compositor_client_,
              MockableSendWebGestureEvent(render_widget_id_))
      .Times(1);

  compositor_->SendProto(cc::proto::CompositorMessage());
  compositor_->SendGestureEvent(blink::WebGestureEvent());
}

}  // namespace client
}  // namespace blimp
